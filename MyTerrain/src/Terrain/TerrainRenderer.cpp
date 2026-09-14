#include "TerrainRenderer.h"
#include "Frustum.h"
#include "TerrainStitch.h"
#include "../Framework/Framework.h"
#include "../Framework/Camera.h"
#include "../Framework/ShaderUtil.h"
#include "../GameObject/GameObject.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

using namespace DirectX;

namespace
{
    constexpr wchar_t kShaderFile[] = L"BasicTerrain.hlsl";

    // LOD 레벨 색상 (6-1 기법의 "K" 색상 모드). 셰이더를 고치지 않고 gBaseColor 자리에
    // 그대로 밀어 넣는다 -- 체커 분기가 gBaseColor 를 바탕으로 명암을 만들기 때문에
    // 조명/체커가 살아 있는 채로 레벨만 색으로 구분된다.
    const XMFLOAT4& LodLevelColor(int level)
    {
        static const XMFLOAT4 kColors[TerrainLOD::kMaxLevels] =
        {
            { 0.36f, 0.72f, 0.38f, 1.0f },   // 0 : 초록 (풀 해상도)
            { 0.85f, 0.80f, 0.30f, 1.0f },   // 1 : 노랑
            { 0.90f, 0.56f, 0.24f, 1.0f },   // 2 : 주황
            { 0.86f, 0.33f, 0.28f, 1.0f },   // 3 : 빨강
            { 0.62f, 0.40f, 0.76f, 1.0f },   // 4 : 보라 (가장 거침)
        };

        return kColors[std::clamp(level, 0, TerrainLOD::kMaxLevels - 1)];
    }
}

const wchar_t* ToDisplayName(TerrainDisplayMode mode)
{
    switch (mode)
    {
    case TerrainDisplayMode::SolidWireframe: return L"솔리드 + 와이어프레임";
    case TerrainDisplayMode::Wireframe:      return L"와이어프레임";
    case TerrainDisplayMode::Solid:          return L"솔리드";
    default:                                 return L"알 수 없음";
    }
}

void TerrainRenderer::Start()
{
    // 실제 생성은 첫 Render 때 수행한다 (디바이스가 준비된 시점을 보장하기 위해)
}

void TerrainRenderer::Destroy()
{
    m_vertexShader.Reset();
    m_pixelShader.Reset();
    m_inputLayout.Reset();
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_constantBuffer.Reset();
    m_solidRasterizer.Reset();
    m_wireRasterizer.Reset();
    m_depthState.Reset();

    m_heightMapSRV.Reset();
    m_heightMapSampler.Reset();

    m_splatSRV.Reset();
    m_splatSampler.Reset();

    m_quadtreeIndexBuffer.Reset();
    m_quadtree = Quadtree::Tree{};
    m_quadtreeDirty = true;

    m_lodIndexBuffer.Reset();
    m_lodStitchIndexBuffer.Reset();
    m_lodStitchCapacity = 0;
    m_lodStitchedChunkCount = 0;
    m_stitchIndices.clear();
    m_lodStitchStart.clear();
    m_lodStitchCount.clear();
    m_lodChunkMask.clear();
    m_lodGrid = TerrainLOD::Grid{};
    m_lodLevels.clear();
    m_lodDrawList.clear();
    m_lodDrawnChunkCount = 0;
    m_lodDrawnTriangleCount = 0;
    m_lodDirty = true;

    m_debugBoxVertexBuffer.Reset();
    m_debugBoxIndexBuffer.Reset();
    m_debugBoxVertexCapacity = 0;
    m_debugBoxIndexCapacity = 0;

    m_tessVertexShader.Reset();
    m_hullShaderInteger.Reset();
    m_hullShaderFracOdd.Reset();
    m_domainShader.Reset();
    m_tessConstantBuffer.Reset();
    m_patchIndexBuffer.Reset();
    m_patchGrid = PatchGrid::Grid{};
    m_tessDrawList.clear();
    m_tessDrawnPatchCount = 0;
    m_tessEstimatedTriangleCount = 0;
    m_tessPipelineReady = false;
    m_tessPipelineFailed = false;
    m_tessPatchGridDirty = true;

    m_resourcesReady = false;
    m_meshDirty = true;
}

void TerrainRenderer::SetGrid(int divisionsX, int divisionsZ, float cellSize)
{
    divisionsX = std::max(divisionsX, 1);
    divisionsZ = std::max(divisionsZ, 1);
    cellSize = std::max(cellSize, 0.0001f);

    if (m_divisionsX == divisionsX && m_divisionsZ == divisionsZ && m_cellSize == cellSize)
    {
        return;
    }

    m_divisionsX = divisionsX;
    m_divisionsZ = divisionsZ;
    m_cellSize = cellSize;
    m_meshDirty = true;
}

void TerrainRenderer::SetHeightFunction(const GridMesh::HeightFunc& heightFunc)
{
    m_heightFunc = heightFunc;
    m_meshDirty = true;
}

void TerrainRenderer::SetHeightMapResources(ID3D11ShaderResourceView* srv, ID3D11SamplerState* sampler)
{
    // ComPtr 로 받아두어 컴포넌트가 텍스처를 교체해도 그리는 도중에 사라지지 않게 한다
    m_heightMapSRV = srv;
    m_heightMapSampler = sampler;
}

void TerrainRenderer::SetHeightMapMapping(float worldSize, bool flipZ)
{
    m_heightMapWorldSize = std::max(worldSize, 0.0001f);
    m_heightMapFlipZ = flipZ;
}

void TerrainRenderer::SetSplatResources(ID3D11ShaderResourceView* arraySrv, ID3D11SamplerState* sampler)
{
    m_splatSRV = arraySrv;
    m_splatSampler = sampler;
}

void TerrainRenderer::SetSplatParams(float tiling, float slopeStart, float slopeEnd)
{
    m_splatTiling = std::max(tiling, 0.0001f);
    m_splatSlopeStart = std::clamp(slopeStart, 0.0f, 1.0f);
    m_splatSlopeEnd = std::clamp(std::max(slopeEnd, m_splatSlopeStart), 0.0f, 1.0f);
}

void TerrainRenderer::SetQuadtreeLeafSize(int maxLeafCells)
{
    maxLeafCells = std::max(maxLeafCells, 1);
    if (m_quadtreeMaxLeafCells == maxLeafCells)
    {
        return;
    }

    m_quadtreeMaxLeafCells = maxLeafCells;
    m_quadtreeDirty = true;
}

void TerrainRenderer::SetLodChunkSize(int cells)
{
    cells = std::max(cells, 1);
    if (m_lodChunkCells == cells)
    {
        return;
    }

    m_lodChunkCells = cells;
    m_lodDirty = true;
}

void TerrainRenderer::SetLodLevelCount(int count)
{
    count = std::clamp(count, 1, TerrainLOD::kMaxLevels);
    if (m_lodLevelCount == count)
    {
        return;
    }

    m_lodLevelCount = count;
    m_lodDirty = true;
}

void TerrainRenderer::SetLodMorphWidth(float width)
{
    // 0.5 를 넘으면 morph 구간이 레벨 범위를 넘어서서, 세밀한 이웃과 붙어 있는
    // 거친 청크까지 움직이기 시작한다 -- 경계에 실오라기 같은 틈이 생긴다.
    m_lodMorphWidth = std::clamp(width, 0.0f, 0.5f);
}

void TerrainRenderer::SetLodBaseDistance(float distance)
{
    // 거리는 매 프레임 선택에만 쓰이므로 인덱스를 다시 만들 필요가 없다 (즉시 반영된다).
    m_lodBaseDistance = std::max(distance, 1.0f);
}

void TerrainRenderer::SetTessellationEnabled(bool enabled)
{
    if (m_tessellationEnabled == enabled)
    {
        return;
    }

    m_tessellationEnabled = enabled;
    m_tessPatchGridDirty = true;
}

void TerrainRenderer::SetTessFactorRange(float minFactor, float maxFactor)
{
    // D3D11 하드웨어 테셀레이션 팩터의 범위는 [1, 64] 다.
    minFactor = std::clamp(minFactor, 1.0f, 64.0f);
    maxFactor = std::clamp(std::max(maxFactor, minFactor), 1.0f, 64.0f);

    m_tessMinFactor = minFactor;
    m_tessMaxFactor = maxFactor;
}

void TerrainRenderer::SetTessBaseDistance(float distance)
{
    // 거리는 매 프레임 상수 버퍼에만 실리므로 패치 넷을 다시 만들 필요가 없다.
    m_tessBaseDistance = std::max(distance, 1.0f);
}

void TerrainRenderer::SetTessHeightMapScale(float heightScale, float heightOffset)
{
    m_tessHeightScale = heightScale;
    m_tessHeightOffset = heightOffset;
}

void TerrainRenderer::SetTessNormalEpsilon(float epsilon)
{
    m_tessNormalEpsilon = std::max(epsilon, 0.001f);
}

int TerrainRenderer::GetLodChunksAtLevel(int level) const
{
    if (level < 0 || level >= TerrainLOD::kMaxLevels)
    {
        return 0;
    }

    return m_lodLevelHistogram[level];
}

void TerrainRenderer::CycleDisplayMode()
{
    const int next = (static_cast<int>(m_displayMode) + 1) % static_cast<int>(TerrainDisplayMode::Count);
    m_displayMode = static_cast<TerrainDisplayMode>(next);
}

bool TerrainRenderer::CreateDeviceResources()
{
    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return false;
    }

    ID3D11Device* device = framework->GetRenderer().GetDevice();
    if (device == nullptr)
    {
        return false;
    }

    // ---------------- 셰이더 컴파일 ----------------
    std::wstring error;

    ShaderUtil::ComPtr<ID3DBlob> vsBlob = ShaderUtil::CompileFromFile(kShaderFile, "VSMain", "vs_5_0", &error);
    if (!vsBlob)
    {
        ShaderUtil::ReportErrorOnce(error);
        return false;
    }

    ShaderUtil::ComPtr<ID3DBlob> psBlob = ShaderUtil::CompileFromFile(kShaderFile, "PSMain", "ps_5_0", &error);
    if (!psBlob)
    {
        ShaderUtil::ReportErrorOnce(error);
        return false;
    }

    HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                            nullptr, &m_vertexShader);
    if (FAILED(hr)) return false;

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                   nullptr, &m_pixelShader);
    if (FAILED(hr)) return false;

    // ---------------- 입력 레이아웃 (GridMesh::Vertex 와 순서가 같아야 한다) ----------------
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        // 6-2 지오머핑용 (부모 높이, 정점 레벨). morph 계수가 0 이면 셰이더가 쓰지 않는다.
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,    0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = device->CreateInputLayout(layout, ARRAYSIZE(layout),
                                   vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                   &m_inputLayout);
    if (FAILED(hr)) return false;

    // ---------------- 상수 버퍼 ----------------
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(TerrainConstants);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer);
    if (FAILED(hr)) return false;

    // 7번 하드웨어 테셀레이션용 상수 버퍼. 1~6번 기법에서도 PS 가 항상 이 버퍼를
    // 참조하므로(꺼져 있으면 gTessOrigin.w == 0) 항상 만들어 두고, 초기값을 0 으로
    // 명시해서(CreateBuffer 에 초기 데이터를 안 주면 내용이 정의되지 않는다) 테셀레이션을
    // 켠 적이 없는 기법에서 시각화 분기가 우연히 켜지는 일이 없게 한다.
    TessConstants tessZeroInit{};
    D3D11_SUBRESOURCE_DATA tessCbInitData = {};
    tessCbInitData.pSysMem = &tessZeroInit;

    D3D11_BUFFER_DESC tessCbDesc = {};
    tessCbDesc.ByteWidth = sizeof(TessConstants);
    tessCbDesc.Usage = D3D11_USAGE_DYNAMIC;
    tessCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    tessCbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = device->CreateBuffer(&tessCbDesc, &tessCbInitData, &m_tessConstantBuffer);
    if (FAILED(hr)) return false;

    // ---------------- 래스터라이저 상태 ----------------
    // 평면을 아래에서도 볼 수 있도록 컬링은 꺼둔다 (쿼드트리 컬링 기법에서 다시 다룰 예정)
    D3D11_RASTERIZER_DESC solidDesc = {};
    solidDesc.FillMode = D3D11_FILL_SOLID;
    solidDesc.CullMode = D3D11_CULL_NONE;
    solidDesc.FrontCounterClockwise = FALSE;
    solidDesc.DepthClipEnable = TRUE;

    hr = device->CreateRasterizerState(&solidDesc, &m_solidRasterizer);
    if (FAILED(hr)) return false;

    // 와이어프레임은 솔리드 위에 겹쳐 그리므로 깊이를 살짝 당겨준다(z-fighting 방지)
    D3D11_RASTERIZER_DESC wireDesc = solidDesc;
    wireDesc.FillMode = D3D11_FILL_WIREFRAME;
    wireDesc.DepthBias = -2000;
    wireDesc.SlopeScaledDepthBias = -1.0f;
    wireDesc.DepthBiasClamp = 0.0f;

    hr = device->CreateRasterizerState(&wireDesc, &m_wireRasterizer);
    if (FAILED(hr)) return false;

    // ---------------- 깊이 상태 ----------------
    // Direct2D(텍스트)가 파이프라인 상태를 바꿔놓기 때문에 매 프레임 직접 지정한다
    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
    depthDesc.StencilEnable = FALSE;

    hr = device->CreateDepthStencilState(&depthDesc, &m_depthState);
    if (FAILED(hr)) return false;

    return true;
}

bool TerrainRenderer::RebuildMesh()
{
    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return false;
    }

    ID3D11Device* device = framework->GetRenderer().GetDevice();
    if (device == nullptr)
    {
        return false;
    }

    // 높이 함수 평가 + 정점/인덱스 생성 + GPU 업로드까지를 한 덩어리로 잰다.
    // 분할 수나 옥타브를 올렸을 때 비용이 어떻게 늘어나는지 HUD 에서 바로 보인다.
    const auto rebuildStart = std::chrono::steady_clock::now();

    const GridMesh::MeshData mesh = GridMesh::Generate(m_divisionsX, m_divisionsZ, m_cellSize, m_heightFunc);

    m_vertexCount = mesh.GetVertexCount();
    m_triangleCount = mesh.GetTriangleCount();
    m_indexCount = static_cast<UINT>(mesh.indices.size());

    // 이전 버퍼를 먼저 놓아준다
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();

    // ---- 정점 버퍼 ----
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = static_cast<UINT>(sizeof(GridMesh::Vertex) * mesh.vertices.size());
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = mesh.vertices.data();

    HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, &m_vertexBuffer);
    if (FAILED(hr)) return false;

    // ---- 인덱스 버퍼 (정점 수가 6만 개를 훌쩍 넘으므로 32비트 인덱스를 쓴다) ----
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * mesh.indices.size());
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = mesh.indices.data();

    hr = device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer);
    if (FAILED(hr)) return false;

    const auto rebuildEnd = std::chrono::steady_clock::now();
    m_lastRebuildMs = std::chrono::duration<double, std::milli>(rebuildEnd - rebuildStart).count();

    // 정점/인덱스는 이미 위에서 GPU 버퍼로 복사됐으니 CPU 쪽 사본은 옮겨서 들고 있는다
    // (쿼드트리 재구성용 -- 리프 크기만 바뀌었을 때 높이를 다시 계산하지 않기 위해서다).
    m_cpuMesh = std::move(mesh);
    m_quadtreeDirty = true;
    m_lodDirty = true;
    m_tessPatchGridDirty = true;

    m_meshDirty = false;
    return true;
}

bool TerrainRenderer::RebuildQuadtreeIndexBuffer()
{
    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return false;
    }

    ID3D11Device* device = framework->GetRenderer().GetDevice();
    if (device == nullptr)
    {
        return false;
    }

    m_quadtreeIndexBuffer.Reset();

    if (m_quadtreeMaxLeafCells <= 0 || m_cpuMesh.vertices.empty())
    {
        // 기능이 꺼져 있거나(0) 아직 메시가 없으면 비워두고 끝낸다 -- Render() 는
        // m_quadtreeMaxLeafCells <= 0 이면 이 트리를 아예 쳐다보지 않는다.
        m_quadtree = Quadtree::Tree{};
        m_quadtreeDirty = false;
        return true;
    }

    m_quadtree = Quadtree::Build(m_cpuMesh, m_quadtreeMaxLeafCells);

    if (m_quadtree.indices.empty())
    {
        m_quadtreeDirty = false;
        return true;
    }

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * m_quadtree.indices.size());
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = m_quadtree.indices.data();

    const HRESULT hr = device->CreateBuffer(&ibDesc, &ibData, &m_quadtreeIndexBuffer);
    if (FAILED(hr))
    {
        m_quadtree = Quadtree::Tree{};
        return false;
    }

    m_quadtreeDirty = false;
    return true;
}

bool TerrainRenderer::RebuildLodIndexBuffer()
{
    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return false;
    }

    ID3D11Device* device = framework->GetRenderer().GetDevice();
    if (device == nullptr)
    {
        return false;
    }

    m_lodIndexBuffer.Reset();
    m_lodLevels.clear();
    m_lodDrawList.clear();

    if (m_lodChunkCells <= 0 || m_cpuMesh.vertices.empty())
    {
        // 기능이 꺼져 있거나(0) 아직 메시가 없으면 비워두고 끝낸다 -- Render() 는
        // m_lodChunkCells <= 0 이면 이 격자를 아예 쳐다보지 않는다.
        m_lodGrid = TerrainLOD::Grid{};
        m_lodDirty = false;
        return true;
    }

    m_lodGrid = TerrainLOD::Build(m_cpuMesh, m_lodChunkCells, m_lodLevelCount);

    // 청크 AABB 대각선의 최대값 (기준 거리 권장값을 HUD 에 보여주기 위해).
    m_lodMaxChunkDiagonal = 0.0f;
    for (const TerrainLOD::Chunk& chunk : m_lodGrid.chunks)
    {
        const float dx = chunk.bounds.max.x - chunk.bounds.min.x;
        const float dy = chunk.bounds.max.y - chunk.bounds.min.y;
        const float dz = chunk.bounds.max.z - chunk.bounds.min.z;
        m_lodMaxChunkDiagonal = std::max(m_lodMaxChunkDiagonal, std::sqrt(dx * dx + dy * dy + dz * dz));
    }

    if (m_lodGrid.indices.empty())
    {
        m_lodDirty = false;
        return true;
    }

    // 모든 레벨의 인덱스를 한 번에 담은 정적 버퍼. 매 프레임 다시 채우지 않는다.
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * m_lodGrid.indices.size());
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = m_lodGrid.indices.data();

    const HRESULT hr = device->CreateBuffer(&ibDesc, &ibData, &m_lodIndexBuffer);
    if (FAILED(hr))
    {
        m_lodGrid = TerrainLOD::Grid{};
        return false;
    }

    m_lodDirty = false;
    return true;
}

void TerrainRenderer::UpdateLodSelection(const XMMATRIX& viewProj, const XMFLOAT3& cameraPosition)
{
    const size_t chunkCount = m_lodGrid.chunks.size();

    // 프리즈를 막 켠 프레임에 그때의 카메라 위치를 붙잡아 둔다.
    if (m_lodFreezeRequested)
    {
        m_lodFrozenCameraPosition = cameraPosition;
        m_lodFreezeRequested = false;
    }

    const XMFLOAT3 lodOrigin = m_lodFrozen ? m_lodFrozenCameraPosition : cameraPosition;

    // ---- 1) 청크마다 거리로 레벨을 정한다 ----
    m_lodLevels.assign(chunkCount, 0);

    if (m_lodEnabled)
    {
        for (size_t i = 0; i < chunkCount; ++i)
        {
            const float distance = TerrainLOD::DistanceToBounds(m_lodGrid.chunks[i].bounds, lodOrigin);
            m_lodLevels[i] = TerrainLOD::SelectLevel(distance, m_lodBaseDistance, m_lodGrid.levelCount);
        }

        // 스티칭은 "이웃과의 레벨 차이가 1 이하" 를 전제로 하므로, 켜져 있으면 강제한다.
        if (m_lodNeighborClampEnabled || m_lodStitchEnabled)
        {
            TerrainLOD::ClampNeighborLevels(m_lodGrid, m_lodLevels);
        }
    }

    // ---- 2) 절두체 컬링 ----
    // 프리즈 중이어도 컬링은 "지금" 카메라를 그대로 따라간다 -- 레벨을 얼려둔 채
    // 경계까지 날아가서 이음매를 코앞에서 볼 수 있어야 하기 때문이다.
    Frustum frustum;
    if (m_lodFrustumCullingEnabled)
    {
        frustum.ExtractFromViewProjection(viewProj);
    }

    m_lodDrawList.clear();
    m_lodDrawList.reserve(chunkCount);

    m_lodChunkMask.assign(chunkCount, 0);
    m_lodStitchStart.assign(chunkCount, 0);
    m_lodStitchCount.assign(chunkCount, 0);
    m_stitchIndices.clear();
    m_lodStitchedChunkCount = 0;

    for (int& count : m_lodLevelHistogram)
    {
        count = 0;
    }
    m_lodDrawnTriangleCount = 0;

    const int vertexCountX = m_cpuMesh.divisionsX + 1;

    for (size_t i = 0; i < chunkCount; ++i)
    {
        const TerrainLOD::Chunk& chunk = m_lodGrid.chunks[i];

        if (m_lodFrustumCullingEnabled &&
            !frustum.IntersectsAABB(chunk.bounds.min, chunk.bounds.max))
        {
            continue;
        }

        const int level = std::clamp(m_lodLevels[i], 0, TerrainLOD::kMaxLevels - 1);

        m_lodDrawList.push_back(static_cast<int>(i));
        ++m_lodLevelHistogram[level];

        // ---- 3) 이웃보다 세밀한 청크만 테두리를 다시 엮는다 (6-2 스티칭) ----
        const int mask = m_lodStitchEnabled
            ? TerrainLOD::NeighborCoarserMask(m_lodGrid, m_lodLevels, static_cast<int>(i))
            : 0;

        if (mask == 0)
        {
            // 이웃이 전부 같은 레벨이면 미리 구운 통짜를 그대로 그린다.
            m_lodDrawnTriangleCount += chunk.indexCount[level] / 3;
            continue;
        }

        const UINT start = static_cast<UINT>(m_stitchIndices.size());

        TerrainStitch::BuildRing(m_stitchIndices, vertexCountX,
                                 chunk.cellX0, chunk.cellX1,
                                 chunk.cellZ0, chunk.cellZ1,
                                 1 << level, mask);

        m_lodChunkMask[i] = mask;
        m_lodStitchStart[i] = start;
        m_lodStitchCount[i] = static_cast<UINT>(m_stitchIndices.size()) - start;
        ++m_lodStitchedChunkCount;

        // 이 청크는 "정적 버퍼의 코어" + "동적 버퍼의 새 테두리" 로 그려진다.
        m_lodDrawnTriangleCount += (chunk.indexCount[level] - chunk.ringCount[level]) / 3;
        m_lodDrawnTriangleCount += m_lodStitchCount[i] / 3;
    }

    // 레벨이 같으면 상수 버퍼도 같으므로, 레벨 순으로 정렬해두면 프레임당 상수 버퍼
    // 업로드가 레벨 수만큼으로 줄어든다 (청크 순서 그대로면 레벨이 계속 뒤바뀐다).
    std::stable_sort(m_lodDrawList.begin(), m_lodDrawList.end(),
                     [this](int a, int b) { return m_lodLevels[a] < m_lodLevels[b]; });

    m_lodDrawnChunkCount = m_lodDrawList.size();
}

bool TerrainRenderer::UploadLodStitchBuffer(ID3D11DeviceContext* context)
{
    if (m_stitchIndices.empty() || context == nullptr)
    {
        return true;   // 스티칭할 청크가 없으면 할 일도 없다
    }

    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return false;
    }

    ID3D11Device* device = framework->GetRenderer().GetDevice();
    if (device == nullptr)
    {
        return false;
    }

    const UINT needed = static_cast<UINT>(m_stitchIndices.size());

    // 매 프레임 크기가 조금씩 달라지므로 여유를 두고 잡았다가 부족할 때만 다시 만든다.
    if (!m_lodStitchIndexBuffer || needed > m_lodStitchCapacity)
    {
        m_lodStitchCapacity = needed + needed / 2 + 1024;

        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * m_lodStitchCapacity);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        m_lodStitchIndexBuffer.Reset();
        if (FAILED(device->CreateBuffer(&desc, nullptr, &m_lodStitchIndexBuffer)))
        {
            m_lodStitchCapacity = 0;
            return false;
        }
    }

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(m_lodStitchIndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return false;
    }

    std::memcpy(mapped.pData, m_stitchIndices.data(), sizeof(uint32_t) * m_stitchIndices.size());
    context->Unmap(m_lodStitchIndexBuffer.Get(), 0);

    return true;
}

void TerrainRenderer::DrawLodChunks(ID3D11DeviceContext* context, const XMMATRIX& world,
                                    const XMMATRIX& viewProj, const XMFLOAT3& cameraPosition,
                                    const XMFLOAT4& baseColor, bool useLighting, bool allowLevelColor)
{
    const bool levelColor = allowLevelColor && m_lodColorMode;

    // 지오머핑은 정점 단위로 계산되므로 청크마다 바뀌는 상수가 없다. 그래서
    // 상수 버퍼를 다시 올릴 이유는 "레벨 색상" 하나뿐이고, m_lodDrawList 가
    // 레벨 순으로 정렬돼 있으니 프레임당 업로드는 레벨 수만큼(최대 5회)이다.
    int  uploadedLevel = -1;
    bool uploaded = false;

    auto applyLevelState = [&](int level)
    {
        if (!levelColor)
        {
            if (uploaded)
            {
                return;
            }

            UpdateConstantBuffer(context, world, viewProj, cameraPosition,
                                 baseColor, useLighting, m_lodMorphEnabled);
            uploaded = true;
            return;
        }

        if (uploaded && level == uploadedLevel)
        {
            return;
        }

        UpdateConstantBuffer(context, world, viewProj, cameraPosition,
                             LodLevelColor(level), useLighting, m_lodMorphEnabled);

        uploadedLevel = level;
        uploaded = true;
    };

    // ---- 1) 정적 버퍼 : 이웃과 레벨이 같은 청크는 통짜로, 스티칭할 청크는 코어만 ----
    context->IASetIndexBuffer(m_lodIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

    for (int chunkIndex : m_lodDrawList)
    {
        const TerrainLOD::Chunk& chunk = m_lodGrid.chunks[chunkIndex];
        const int level = std::clamp(m_lodLevels[chunkIndex], 0, TerrainLOD::kMaxLevels - 1);

        applyLevelState(level);

        if (m_lodStitchCount[chunkIndex] == 0)
        {
            // 이웃과 레벨이 같다 -- 미리 구운 통짜 하나로 끝난다 (6-1 과 같은 경로).
            context->DrawIndexed(chunk.indexCount[level], chunk.indexStart[level], 0);
            continue;
        }

        // 테두리는 아래 2) 에서 새로 엮은 것으로 그린다.
        // (한 변이 2 스텝셀인 청크는 코어가 비어 있어 여기서 그릴 것이 없다)
        const UINT coreCount = chunk.indexCount[level] - chunk.ringCount[level];
        if (coreCount > 0)
        {
            context->DrawIndexed(coreCount, chunk.indexStart[level] + chunk.ringCount[level], 0);
        }
    }

    // ---- 2) 동적 버퍼 : 이번 프레임에 새로 엮은 테두리 ----
    if (m_lodStitchedChunkCount > 0 && m_lodStitchIndexBuffer)
    {
        context->IASetIndexBuffer(m_lodStitchIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

        for (int chunkIndex : m_lodDrawList)
        {
            const UINT count = m_lodStitchCount[chunkIndex];
            if (count == 0)
            {
                continue;
            }

            applyLevelState(std::clamp(m_lodLevels[chunkIndex], 0, TerrainLOD::kMaxLevels - 1));
            context->DrawIndexed(count, m_lodStitchStart[chunkIndex], 0);
        }

        // 다음 패스(와이어프레임 / 디버그 박스)를 위해 원래 버퍼로 돌려놓는다.
        context->IASetIndexBuffer(m_lodIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    }
}

void TerrainRenderer::UpdateConstantBuffer(ID3D11DeviceContext* context,
                                           const XMMATRIX& world,
                                           const XMMATRIX& viewProj,
                                           const XMFLOAT3& cameraPosition,
                                           const XMFLOAT4& baseColor,
                                           bool useLighting,
                                           bool morphEnabled)
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    TerrainConstants* constants = static_cast<TerrainConstants*>(mapped.pData);

    // HLSL 의 기본 행렬 규약(열 우선)에 맞추기 위해 전치해서 넘긴다
    XMStoreFloat4x4(&constants->world, XMMatrixTranspose(world));
    XMStoreFloat4x4(&constants->worldViewProj, XMMatrixTranspose(world * viewProj));

    constants->baseColor = baseColor;

    XMVECTOR lightDir = XMVector3Normalize(XMLoadFloat3(&m_lightDirection));
    XMStoreFloat3(&constants->lightDirection, lightDir);

    constants->useLighting = useLighting ? 1.0f : 0.0f;
    constants->cameraPosition = cameraPosition;
    constants->cellSize = m_cellSize * m_checkerScale;

    // 고도 색상은 텍스처가 실제로 올라와 있고 조명 패스일 때만 켠다
    // (와이어프레임 패스는 단색이므로 텍스처를 읽을 이유가 없다)
    const bool heightColorOn = m_heightColorMode && useLighting && m_heightMapSRV && m_heightMapSampler;

    constants->heightMapParams = XMFLOAT4(
        1.0f / std::max(m_heightMapWorldSize, 0.0001f),
        m_heightMapFlipZ ? -1.0f : 1.0f,
        heightColorOn ? 1.0f : 0.0f,
        0.0f);

    // 스플래팅도 조명 패스일 때만, 텍스처가 실제로 올라와 있을 때만 켠다
    // (와이어프레임 패스는 단색이라 텍스처를 읽을 이유가 없다)
    const bool splatOn = m_splatMode && useLighting && m_splatSRV && m_splatSampler;

    constants->splatParams = XMFLOAT4(
        m_splatTiling,
        m_splatSlopeStart,
        m_splatSlopeEnd,
        splatOn ? 1.0f : 0.0f);

    // 6-2 지오머핑. morphEnabled 가 false 면 w = 0 이 되어 정점 셰이더의 lerp 계수가
    // 항상 0 이다 -- 1~6-1번 기법과 디버그 박스 패스가 영향을 받지 않는 이유다.
    //
    // 청크마다 달라지는 값이 하나도 없다는 점이 중요하다. morph 계수를 정점 단위로
    // 계산하기 때문에, 지오머핑 때문에 상수 버퍼를 다시 올릴 일이 없다.
    constants->lodParams = XMFLOAT4(
        static_cast<float>(std::max(m_lodGrid.levelCount - 1, 0)),
        m_lodBaseDistance,
        m_lodMorphWidth,
        morphEnabled ? 1.0f : 0.0f);

    const XMFLOAT3 lodOrigin = m_lodFrozen ? m_lodFrozenCameraPosition : cameraPosition;

    constants->lodOrigin = XMFLOAT4(
        lodOrigin.x, lodOrigin.y, lodOrigin.z,
        (m_lodMorphColorMode && morphEnabled && useLighting) ? 1.0f : 0.0f);

    context->Unmap(m_constantBuffer.Get(), 0);
}

//=====================================================================
// 7. 하드웨어 테셀레이션
//=====================================================================
bool TerrainRenderer::CreateTessellationPipelineResources()
{
    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return false;
    }

    ID3D11Device* device = framework->GetRenderer().GetDevice();
    if (device == nullptr)
    {
        return false;
    }

    // 실패해도 CreateDeviceResources() 자체는 건드리지 않는다 -- 1~6번 기법은
    // 이 함수를 아예 호출하지 않으므로(SetTessellationEnabled 를 켠 적이 없으면)
    // 여기서 무슨 일이 있어도 영향받지 않는다.
    std::wstring error;

    ShaderUtil::ComPtr<ID3DBlob> vsBlob =
        ShaderUtil::CompileFromFile(kShaderFile, "VSPatch", "vs_5_0", &error);
    if (!vsBlob)
    {
        ShaderUtil::ReportErrorOnce(L"[7번 테셀레이션] " + error);
        m_tessPipelineFailed = true;
        return false;
    }

    ShaderUtil::ComPtr<ID3DBlob> hsIntBlob =
        ShaderUtil::CompileFromFile(kShaderFile, "HSMain_Integer", "hs_5_0", &error);
    if (!hsIntBlob)
    {
        ShaderUtil::ReportErrorOnce(L"[7번 테셀레이션] " + error);
        m_tessPipelineFailed = true;
        return false;
    }

    ShaderUtil::ComPtr<ID3DBlob> hsFracBlob =
        ShaderUtil::CompileFromFile(kShaderFile, "HSMain_FracOdd", "hs_5_0", &error);
    if (!hsFracBlob)
    {
        ShaderUtil::ReportErrorOnce(L"[7번 테셀레이션] " + error);
        m_tessPipelineFailed = true;
        return false;
    }

    ShaderUtil::ComPtr<ID3DBlob> dsBlob =
        ShaderUtil::CompileFromFile(kShaderFile, "DSMain", "ds_5_0", &error);
    if (!dsBlob)
    {
        ShaderUtil::ReportErrorOnce(L"[7번 테셀레이션] " + error);
        m_tessPipelineFailed = true;
        return false;
    }

    HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                            nullptr, &m_tessVertexShader);
    if (FAILED(hr)) { m_tessPipelineFailed = true; return false; }

    hr = device->CreateHullShader(hsIntBlob->GetBufferPointer(), hsIntBlob->GetBufferSize(),
                                  nullptr, &m_hullShaderInteger);
    if (FAILED(hr)) { m_tessPipelineFailed = true; return false; }

    hr = device->CreateHullShader(hsFracBlob->GetBufferPointer(), hsFracBlob->GetBufferSize(),
                                  nullptr, &m_hullShaderFracOdd);
    if (FAILED(hr)) { m_tessPipelineFailed = true; return false; }

    hr = device->CreateDomainShader(dsBlob->GetBufferPointer(), dsBlob->GetBufferSize(),
                                    nullptr, &m_domainShader);
    if (FAILED(hr)) { m_tessPipelineFailed = true; return false; }

    m_tessPipelineReady = true;
    return true;
}

bool TerrainRenderer::RebuildPatchIndexBuffer()
{
    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return false;
    }

    ID3D11Device* device = framework->GetRenderer().GetDevice();
    if (device == nullptr)
    {
        return false;
    }

    m_patchIndexBuffer.Reset();

    if (!m_tessellationEnabled || m_cpuMesh.vertices.empty())
    {
        m_patchGrid = PatchGrid::Grid{};
        m_tessPatchGridDirty = false;
        return true;
    }

    m_patchGrid = PatchGrid::Build(m_cpuMesh);

    if (m_patchGrid.indices.empty())
    {
        m_tessPatchGridDirty = false;
        return true;
    }

    // 통짜 정적 버퍼 하나로 모든 패치를 담는다 (6-1 의 m_lodIndexBuffer 와 같은 자리 --
    // 다만 여기는 레벨이 없으니 패치당 인덱스 4개가 전부다).
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * m_patchGrid.indices.size());
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = m_patchGrid.indices.data();

    const HRESULT hr = device->CreateBuffer(&ibDesc, &ibData, &m_patchIndexBuffer);
    if (FAILED(hr))
    {
        m_patchGrid = PatchGrid::Grid{};
        return false;
    }

    m_tessPatchGridDirty = false;
    return true;
}

void TerrainRenderer::UpdateTessPatchSelection(const XMMATRIX& viewProj, const XMFLOAT3& cameraPosition)
{
    const size_t patchCount = m_patchGrid.patches.size();

    // 프리즈를 막 켠 프레임에 그때의 카메라 위치를 붙잡아 둔다 (6-1 의 F 와 같은 방식).
    if (m_tessFreezeRequested)
    {
        m_tessFrozenCameraPosition = cameraPosition;
        m_tessFreezeRequested = false;
    }

    const XMFLOAT3 origin = m_tessFrozen ? m_tessFrozenCameraPosition : cameraPosition;

    // 컬링은 프리즈 중이어도 "지금" 카메라를 그대로 따라간다.
    Frustum frustum;
    if (m_tessFrustumCullingEnabled)
    {
        frustum.ExtractFromViewProjection(viewProj);
    }

    m_tessDrawList.clear();
    m_tessDrawList.reserve(patchCount);
    m_tessEstimatedTriangleCount = 0;

    // displacement 가 켜져 있으면 코너 4개만으로 잰 AABB 가 실제 표면(밀어 올려진 높이)을
    // 다 못 덮을 수 있으므로 y 범위에 heightScale 만큼 여유를 둔다. 그래도 코너 넷에
    // 전혀 걸리지 않는 극단적으로 뾰족한 봉우리 하나는 컬링에서 잘릴 수 있다 -- 패치를
    // 잘게 쪼갤수록(분할 수를 올릴수록) 코너가 촘촘해져서 이 근사가 덜 거칠어진다.
    const float yPad = m_tessDisplacementEnabled ? std::abs(m_tessHeightScale) : 0.0f;

    for (size_t i = 0; i < patchCount; ++i)
    {
        const PatchGrid::Patch& patch = m_patchGrid.patches[i];

        if (m_tessFrustumCullingEnabled)
        {
            XMFLOAT3 mn = patch.bounds.min;
            XMFLOAT3 mx = patch.bounds.max;
            mn.y -= yPad;
            mx.y += yPad;

            if (!frustum.IntersectsAABB(mn, mx))
            {
                continue;
            }
        }

        m_tessDrawList.push_back(static_cast<int>(i));

        // HUD 용 어림값. HS 의 TessEdgeFactor 와 같은 식으로 패치 중심까지의 거리에서
        // 대표 팩터 하나를 뽑고, (factor^2 * 2) 삼각형으로 셈한다 -- integer 파티션에서는
        // 거의 정확하고 fractional 모드에서는 반 팩터 정도 어긋날 수 있는 근사치다.
        const XMFLOAT3& mn = patch.bounds.min;
        const XMFLOAT3& mx = patch.bounds.max;
        const XMFLOAT3 center{ (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f };
        const float dx = center.x - origin.x;
        const float dy = center.y - origin.y;
        const float dz = center.z - origin.z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        float factor = m_tessMaxFactor * std::min(m_tessBaseDistance / std::max(dist, 0.0001f), 1.0f);
        factor = std::clamp(factor, m_tessMinFactor, m_tessMaxFactor);

        m_tessEstimatedTriangleCount += static_cast<size_t>(factor * factor * 2.0f);
    }

    m_tessDrawnPatchCount = m_tessDrawList.size();
}

void TerrainRenderer::DrawTessellatedPatches(ID3D11DeviceContext* context)
{
    context->IASetIndexBuffer(m_patchIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

    for (int patchIndex : m_tessDrawList)
    {
        // 패치 하나 = 컨트롤 포인트 4개. 인덱스 버퍼 안에서 패치 i 는 [i*4, i*4+4) 구간이다.
        context->DrawIndexed(4, static_cast<UINT>(patchIndex) * 4u, 0);
    }
}

void TerrainRenderer::UpdateTessConstantBuffer(ID3D11DeviceContext* context, const XMMATRIX& viewProj,
                                               const XMFLOAT3& origin)
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(m_tessConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    TessConstants* constants = static_cast<TessConstants*>(mapped.pData);

    XMStoreFloat4x4(&constants->viewProj, XMMatrixTranspose(viewProj));

    constants->factorParams = XMFLOAT4(m_tessBaseDistance, m_tessMaxFactor, m_tessMinFactor, 0.0f);

    constants->heightParams = XMFLOAT4(
        m_tessHeightScale,
        m_tessHeightOffset,
        m_tessDisplacementEnabled ? 1.0f : 0.0f,
        m_tessNormalEpsilon);

    constants->origin = XMFLOAT4(
        origin.x, origin.y, origin.z,
        m_tessFactorColorMode ? 1.0f : 0.0f);

    context->Unmap(m_tessConstantBuffer.Get(), 0);
}

void TerrainRenderer::RenderTessellated(ID3D11DeviceContext* context, const XMMATRIX& world,
                                        const XMMATRIX& viewProj, const XMFLOAT3& cameraPosition)
{
    if (m_tessPatchGridDirty)
    {
        RebuildPatchIndexBuffer();
    }

    if (!m_tessPipelineReady || !m_patchIndexBuffer || m_patchGrid.IsEmpty())
    {
        return;   // 셰이더 컴파일 실패, 또는 아직 패치 넷이 없다 (첫 프레임 등)
    }

    UpdateTessPatchSelection(viewProj, cameraPosition);

    if (m_tessDrawList.empty())
    {
        return;
    }

    const XMFLOAT3 origin = m_tessFrozen ? m_tessFrozenCameraPosition : cameraPosition;
    UpdateTessConstantBuffer(context, viewProj, origin);

    // ---------------- 파이프라인 설정 ----------------
    UINT stride = sizeof(GridMesh::Vertex);
    UINT offset = 0;

    context->IASetInputLayout(m_inputLayout.Get());
    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

    ID3D11HullShader* hullShader = m_tessFractionalPartitioning
        ? m_hullShaderFracOdd.Get()
        : m_hullShaderInteger.Get();

    context->VSSetShader(m_tessVertexShader.Get(), nullptr, 0);
    context->HSSetShader(hullShader, nullptr, 0);
    context->DSSetShader(m_domainShader.Get(), nullptr, 0);
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);

    context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    context->HSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    context->HSSetConstantBuffers(1, 1, m_tessConstantBuffer.GetAddressOf());
    context->DSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    context->DSSetConstantBuffers(1, 1, m_tessConstantBuffer.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    context->PSSetConstantBuffers(1, 1, m_tessConstantBuffer.GetAddressOf());

    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_depthState.Get(), 0);

    // 높이맵 텍스처 : PS(t0/s0) 뿐 아니라 DS(t0/s0) 도 같은 텍스처를 다시 읽어
    // displacement 를 한다. 스플래팅(t1/s1)은 7번에서 쓰지 않으므로 확실히 풀어준다.
    {
        ID3D11ShaderResourceView* srv = m_heightMapSRV.Get();
        ID3D11SamplerState* sampler = m_heightMapSampler.Get();

        context->PSSetShaderResources(0, 1, &srv);
        context->PSSetSamplers(0, 1, &sampler);
        context->DSSetShaderResources(0, 1, &srv);
        context->DSSetSamplers(0, 1, &sampler);

        ID3D11ShaderResourceView* nullSrv = nullptr;
        ID3D11SamplerState* nullSampler = nullptr;
        context->PSSetShaderResources(1, 1, &nullSrv);
        context->PSSetSamplers(1, 1, &nullSampler);
    }

    // ---------------- 솔리드 패스 ----------------
    if (m_displayMode != TerrainDisplayMode::Wireframe)
    {
        context->RSSetState(m_solidRasterizer.Get());
        UpdateConstantBuffer(context, world, viewProj, cameraPosition, m_solidColor, true);
        DrawTessellatedPatches(context);
    }

    // ---------------- 와이어프레임 패스 ----------------
    if (m_displayMode != TerrainDisplayMode::Solid)
    {
        context->RSSetState(m_wireRasterizer.Get());
        UpdateConstantBuffer(context, world, viewProj, cameraPosition, m_wireColor, false);
        DrawTessellatedPatches(context);
    }

    // ---------------- 패치 박스 ----------------
    if (m_tessDebugBoxesEnabled)
    {
        // HS/DS 는 선분(LINELIST)과 같이 쓸 수 없다. RenderBoxLines 는 항상
        // m_vertexShader/m_pixelShader, HS/DS = null 인 일반 삼각형 파이프라인을
        // 가정하고 만들어졌으므로, 박스를 그리기 전에 먼저 그 상태로 되돌린다.
        context->HSSetShader(nullptr, nullptr, 0);
        context->DSSetShader(nullptr, nullptr, 0);
        context->VSSetShader(m_vertexShader.Get(), nullptr, 0);

        std::vector<std::pair<XMFLOAT3, XMFLOAT3>> boxes;
        boxes.reserve(m_tessDrawList.size());

        for (int patchIndex : m_tessDrawList)
        {
            const PatchGrid::Patch& patch = m_patchGrid.patches[patchIndex];
            boxes.emplace_back(patch.bounds.min, patch.bounds.max);
        }

        RenderBoxLines(context, world, viewProj, cameraPosition, boxes, m_debugBoxColor);
    }

    // 다음에 그릴 것들을 위해 테셀레이션 스테이지를 확실히 꺼 둔다
    // (일반 삼각형 경로의 Render() 도 매 프레임 HSSetShader(nullptr,...) 를 하지만,
    //  이 함수에서 바로 return 하는 경우를 대비해 여기서도 확실히 해 둔다).
    context->HSSetShader(nullptr, nullptr, 0);
    context->DSSetShader(nullptr, nullptr, 0);
}

void TerrainRenderer::RenderDebugBoxes(ID3D11DeviceContext* context, const XMMATRIX& world,
                                       const XMMATRIX& viewProj, const XMFLOAT3& cameraPosition,
                                       const std::vector<int>& visibleLeaves)
{
    // 5번 쿼드트리 컬링용 얇은 래퍼 -- 보이는 리프의 AABB 만 모아서 공용 박스 렌더러에 넘긴다.
    // (6-1 LOD 기법도 같은 렌더러로 청크 박스를 그린다)
    std::vector<std::pair<XMFLOAT3, XMFLOAT3>> boxes;
    boxes.reserve(visibleLeaves.size());

    for (int leafIndex : visibleLeaves)
    {
        if (leafIndex < 0 || static_cast<size_t>(leafIndex) >= m_quadtree.leaves.size())
        {
            continue;
        }

        const Quadtree::Leaf& leaf = m_quadtree.leaves[leafIndex];
        boxes.emplace_back(leaf.bounds.min, leaf.bounds.max);
    }

    RenderBoxLines(context, world, viewProj, cameraPosition, boxes, m_debugBoxColor);
}

void TerrainRenderer::RenderBoxLines(ID3D11DeviceContext* context, const XMMATRIX& world,
                                     const XMMATRIX& viewProj, const XMFLOAT3& cameraPosition,
                                     const std::vector<std::pair<XMFLOAT3, XMFLOAT3>>& boxes,
                                     const XMFLOAT4& color)
{
    if (boxes.empty())
    {
        return;
    }

    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return;
    }

    ID3D11Device* device = framework->GetRenderer().GetDevice();
    if (device == nullptr)
    {
        return;
    }

    // 리프마다 모서리 8개 + 변 12개(선분 24개)를 CPU 에서 즉석으로 만든다.
    // 매 프레임 보이는 리프 집합이 바뀌므로 정적 버퍼가 아니라 DISCARD 로 다시 채운다.
    // 셰이더/입력 레이아웃은 기존 것을 그대로 쓴다 -- GridMesh::Vertex 모양만 맞추면 되고,
    // gUseLighting = 0 이면 법선/UV 는 픽셀 셰이더가 아예 읽지 않는다.
    static const int kEdgePairs[12][2] =
    {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
        { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
    };

    std::vector<GridMesh::Vertex> boxVertices;
    std::vector<uint32_t> boxIndices;
    boxVertices.reserve(boxes.size() * 8);
    boxIndices.reserve(boxes.size() * 24);

    for (const auto& box : boxes)
    {
        const XMFLOAT3& mn = box.first;
        const XMFLOAT3& mx = box.second;

        const XMFLOAT3 corners[8] =
        {
            { mn.x, mn.y, mn.z }, { mx.x, mn.y, mn.z }, { mx.x, mn.y, mx.z }, { mn.x, mn.y, mx.z },
            { mn.x, mx.y, mn.z }, { mx.x, mx.y, mn.z }, { mx.x, mx.y, mx.z }, { mn.x, mx.y, mx.z },
        };

        const uint32_t base = static_cast<uint32_t>(boxVertices.size());
        for (const XMFLOAT3& corner : corners)
        {
            GridMesh::Vertex vertex{};
            vertex.position = corner;
            vertex.normal = XMFLOAT3(0.0f, 1.0f, 0.0f); // 안 쓰인다 (unlit 패스)
            vertex.uv = XMFLOAT2(0.0f, 0.0f);            // 안 쓰인다 (unlit 패스)
            boxVertices.push_back(vertex);
        }

        for (const auto& edge : kEdgePairs)
        {
            boxIndices.push_back(base + static_cast<uint32_t>(edge[0]));
            boxIndices.push_back(base + static_cast<uint32_t>(edge[1]));
        }
    }

    if (boxVertices.empty() || boxIndices.empty())
    {
        return;
    }

    // ---- 동적 버퍼 (부족하면 여유를 두고 다시 만든다) ----
    const UINT neededVertexCount = static_cast<UINT>(boxVertices.size());
    const UINT neededIndexCount = static_cast<UINT>(boxIndices.size());

    if (!m_debugBoxVertexBuffer || neededVertexCount > m_debugBoxVertexCapacity)
    {
        m_debugBoxVertexCapacity = neededVertexCount + neededVertexCount / 2 + 64;

        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = static_cast<UINT>(sizeof(GridMesh::Vertex) * m_debugBoxVertexCapacity);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        m_debugBoxVertexBuffer.Reset();
        if (FAILED(device->CreateBuffer(&desc, nullptr, &m_debugBoxVertexBuffer)))
        {
            return;
        }
    }

    if (!m_debugBoxIndexBuffer || neededIndexCount > m_debugBoxIndexCapacity)
    {
        m_debugBoxIndexCapacity = neededIndexCount + neededIndexCount / 2 + 64;

        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * m_debugBoxIndexCapacity);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        m_debugBoxIndexBuffer.Reset();
        if (FAILED(device->CreateBuffer(&desc, nullptr, &m_debugBoxIndexBuffer)))
        {
            return;
        }
    }

    D3D11_MAPPED_SUBRESOURCE mappedVB = {};
    if (FAILED(context->Map(m_debugBoxVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVB)))
    {
        return;
    }
    std::memcpy(mappedVB.pData, boxVertices.data(), sizeof(GridMesh::Vertex) * boxVertices.size());
    context->Unmap(m_debugBoxVertexBuffer.Get(), 0);

    D3D11_MAPPED_SUBRESOURCE mappedIB = {};
    if (FAILED(context->Map(m_debugBoxIndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedIB)))
    {
        return;
    }
    std::memcpy(mappedIB.pData, boxIndices.data(), sizeof(uint32_t) * boxIndices.size());
    context->Unmap(m_debugBoxIndexBuffer.Get(), 0);

    // ---- 그리기 (기존 셰이더/입력 레이아웃 재사용, 토폴로지만 선분으로) ----
    UpdateConstantBuffer(context, world, viewProj, cameraPosition, color, false);

    UINT stride = sizeof(GridMesh::Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, m_debugBoxVertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_debugBoxIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    context->RSSetState(m_solidRasterizer.Get()); // FillMode 는 선분에 적용되지 않으니 그대로 재사용

    context->DrawIndexed(neededIndexCount, 0, 0);
}

void TerrainRenderer::Render()
{
    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return;
    }

    Camera* camera = Camera::GetMain();
    if (camera == nullptr)
    {
        return; // 씬에 카메라가 없으면 그릴 수 없다
    }

    // ---------------- 리소스 준비 ----------------
    if (!m_resourcesReady)
    {
        if (m_resourceCreationFailed)
        {
            return; // 이미 실패했으면 매 프레임 다시 시도하지 않는다
        }

        if (!CreateDeviceResources())
        {
            m_resourceCreationFailed = true;
            return;
        }
        m_resourcesReady = true;
    }

    if (m_meshDirty && !RebuildMesh())
    {
        return;
    }

    // 쿼드트리 기능을 쓰는 기법(5번)에서만, 그리고 리프 크기가 바뀌었을 때만 다시 만든다.
    // 기능을 아예 쓰지 않는 1~4번 기법은 m_quadtreeMaxLeafCells 가 0 이라 이 블록에 들어오지 않는다.
    if (m_quadtreeMaxLeafCells > 0 && m_quadtreeDirty)
    {
        RebuildQuadtreeIndexBuffer();
    }

    // 거리 LOD 기능을 쓰는 기법(6-1)에서만, 그리고 청크 크기/레벨 수가 바뀌었을 때만
    // 다시 만든다. 기능을 쓰지 않는 1~5번 기법은 m_lodChunkCells 가 0 이라 들어오지 않는다.
    if (m_lodChunkCells > 0 && m_lodDirty)
    {
        RebuildLodIndexBuffer();
    }

    if (!m_vertexBuffer || !m_indexBuffer || m_indexCount == 0)
    {
        return;
    }

    ID3D11DeviceContext* context = framework->GetRenderer().GetContext();
    if (context == nullptr)
    {
        return;
    }

    // ---------------- 행렬 ----------------
    XMMATRIX world = XMMatrixIdentity();
    if (GetGameObject() != nullptr && GetGameObject()->GetTransform() != nullptr)
    {
        world = GetGameObject()->GetTransform()->GetWorldMatrix();
    }

    const XMMATRIX viewProj = camera->GetViewProjectionMatrix();
    const XMFLOAT3 cameraPosition = camera->GetWorldPosition();

    // ---------------- 7번 하드웨어 테셀레이션 : 완전히 다른 경로 ----------------
    // 패치는 삼각형 메시가 아니므로 아래의 쿼드트리/LOD 경로와 섞이지 않는다.
    // (Technique07 은 SetQuadtreeLeafSize/SetLodChunkSize 를 호출하지 않으므로
    //  평소에는 이 분기까지 오지 않아도 두 기능이 꺼진 채로 안전하게 지나간다.)
    if (m_tessellationEnabled)
    {
        if (!m_tessPipelineReady && !m_tessPipelineFailed)
        {
            CreateTessellationPipelineResources();
        }

        RenderTessellated(context, world, viewProj, cameraPosition);
        context->RSSetState(nullptr);
        return;
    }

    // ---------------- 쿼드트리 컬링 : 이번 프레임에 보이는 리프 목록 ----------------
    // 컬링 스위치(C)가 꺼져 있어도 통계/디버그 박스를 위해 목록 자체는 매 프레임 계산해둔다 --
    // "컬링이 꺼져 있을 때 뭐가 보일 예정이었는지" 를 그대로 보여줄 수 있기 때문이다.
    const bool quadtreeAvailable = (m_quadtreeMaxLeafCells > 0) && !m_quadtree.IsEmpty() && m_quadtreeIndexBuffer;

    std::vector<int> visibleLeaves;
    if (quadtreeAvailable)
    {
        Frustum frustum;
        frustum.ExtractFromViewProjection(viewProj);
        Quadtree::CollectVisible(m_quadtree, frustum, visibleLeaves, nullptr);
    }
    m_quadtreeVisibleLeafCount = visibleLeaves.size();

    // 실제로 리프별로 나눠 그릴지: 쿼드트리가 준비돼 있고 컬링 스위치도 켜져 있을 때만.
    // 꺼져 있으면(기본 스위치는 켜짐) 1~4번 기법과 완전히 같은 단일 Draw 경로로 되돌아간다.
    const bool useQuadtreeDraw = quadtreeAvailable && m_quadtreeCullingEnabled;

    // ---------------- 거리 기반 LOD : 이번 프레임에 그릴 청크와 레벨 ----------------
    // LOD 스위치(L)를 꺼도 청크 단위로 나눠 그리는 것 자체는 유지한다 -- Draw 호출 수를
    // 그대로 둔 채 삼각형 수만 비교해야 LOD 의 이득이 정확히 보이기 때문이다.
    const bool lodAvailable = (m_lodChunkCells > 0) && !m_lodGrid.IsEmpty() && m_lodIndexBuffer;

    if (lodAvailable)
    {
        UpdateLodSelection(viewProj, cameraPosition);
        UploadLodStitchBuffer(context);
    }
    else
    {
        m_lodDrawList.clear();
        m_lodDrawnChunkCount = 0;
        m_lodDrawnTriangleCount = 0;
    }

    // ---------------- 파이프라인 설정 ----------------
    // Direct2D 텍스트 렌더링이 상태를 바꿔놓으므로 매 프레임 전부 다시 지정한다
    UINT stride = sizeof(GridMesh::Vertex);
    UINT offset = 0;

    context->IASetInputLayout(m_inputLayout.Get());
    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    ID3D11Buffer* activeIndexBuffer = m_indexBuffer.Get();
    if (lodAvailable)
    {
        activeIndexBuffer = m_lodIndexBuffer.Get();
    }
    else if (useQuadtreeDraw)
    {
        activeIndexBuffer = m_quadtreeIndexBuffer.Get();
    }

    context->IASetIndexBuffer(activeIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    context->HSSetShader(nullptr, nullptr, 0);
    context->DSSetShader(nullptr, nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);

    context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_depthState.Get(), 0);

    // 높이맵 텍스처 (없으면 확실히 풀어준다 -- 앞 기법이 걸어둔 것이 남아있으면 안 된다)
    {
        ID3D11ShaderResourceView* srv = m_heightMapSRV.Get();
        ID3D11SamplerState* sampler = m_heightMapSampler.Get();

        context->PSSetShaderResources(0, 1, &srv);
        context->PSSetSamplers(0, 1, &sampler);
    }

    // 스플래팅 배열 텍스처 (t1/s1). 마찬가지로 없으면 확실히 풀어준다.
    {
        ID3D11ShaderResourceView* srv = m_splatSRV.Get();
        ID3D11SamplerState* sampler = m_splatSampler.Get();

        context->PSSetShaderResources(1, 1, &srv);
        context->PSSetSamplers(1, 1, &sampler);
    }

    // ---------------- 솔리드 패스 ----------------
    if (m_displayMode != TerrainDisplayMode::Wireframe)
    {
        context->RSSetState(m_solidRasterizer.Get());

        if (lodAvailable)
        {
            DrawLodChunks(context, world, viewProj, cameraPosition, m_solidColor, true, true);
        }
        else if (useQuadtreeDraw)
        {
            UpdateConstantBuffer(context, world, viewProj, cameraPosition, m_solidColor, true);

            for (int leafIndex : visibleLeaves)
            {
                const Quadtree::Leaf& leaf = m_quadtree.leaves[leafIndex];
                context->DrawIndexed(leaf.indexCount, leaf.indexStart, 0);
            }
        }
        else
        {
            UpdateConstantBuffer(context, world, viewProj, cameraPosition, m_solidColor, true);
            context->DrawIndexed(m_indexCount, 0, 0);
        }
    }

    // ---------------- 와이어프레임 패스 ----------------
    if (m_displayMode != TerrainDisplayMode::Solid)
    {
        context->RSSetState(m_wireRasterizer.Get());

        if (lodAvailable)
        {
            // 와이어는 단색이어야 격자 밀도 차이가 잘 보이므로 레벨 색상은 쓰지 않는다.
            DrawLodChunks(context, world, viewProj, cameraPosition, m_wireColor, false, false);
        }
        else if (useQuadtreeDraw)
        {
            UpdateConstantBuffer(context, world, viewProj, cameraPosition, m_wireColor, false);

            for (int leafIndex : visibleLeaves)
            {
                const Quadtree::Leaf& leaf = m_quadtree.leaves[leafIndex];
                context->DrawIndexed(leaf.indexCount, leaf.indexStart, 0);
            }
        }
        else
        {
            UpdateConstantBuffer(context, world, viewProj, cameraPosition, m_wireColor, false);
            context->DrawIndexed(m_indexCount, 0, 0);
        }
    }

    // ---------------- 쿼드트리 디버그 박스 ----------------
    if (m_quadtreeDebugBoxesEnabled && quadtreeAvailable)
    {
        RenderDebugBoxes(context, world, viewProj, cameraPosition, visibleLeaves);
    }

    // ---------------- LOD 청크 박스 ----------------
    if (m_lodDebugBoxesEnabled && lodAvailable)
    {
        std::vector<std::pair<XMFLOAT3, XMFLOAT3>> boxes;
        boxes.reserve(m_lodDrawList.size());

        for (int chunkIndex : m_lodDrawList)
        {
            const TerrainLOD::Chunk& chunk = m_lodGrid.chunks[chunkIndex];
            boxes.emplace_back(chunk.bounds.min, chunk.bounds.max);
        }

        RenderBoxLines(context, world, viewProj, cameraPosition, boxes, m_debugBoxColor);
    }

    // 다음에 그릴 것들을 위해 기본 상태로 돌려놓는다
    context->RSSetState(nullptr);
}
