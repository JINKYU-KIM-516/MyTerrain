#include "InfiniteTerrainRenderer.h"
#include "Frustum.h"
#include "../Framework/Framework.h"
#include "../Framework/Camera.h"
#include "../Framework/ShaderUtil.h"
#include "../GameObject/GameObject.h"

#include <algorithm>
#include <chrono>
#include <limits>

using namespace DirectX;

namespace
{
    constexpr wchar_t kShaderFile[] = L"InfiniteTerrain.hlsl";

    // 새로 만들어진 청크를 강조해 보여주는 시간(초)과 색.
    // 이 기법은 "언제 만들어지고 언제 사라지는지" 가 보이지 않으면 관찰할 것이 없다.
    constexpr float kHighlightSeconds = 0.9f;
    const XMFLOAT4 kHighlightColor{ 1.0f, 0.62f, 0.20f, 1.0f };

    // 청크 색상 모드에서 쓸 색. 청크 좌표를 해시해서 고르므로 이웃끼리 같은 색이
    // 잘 나오지 않는다(격자 무늬가 아니라 "청크 하나하나"가 보이게 하는 것이 목적).
    const XMFLOAT4& ChunkPaletteColor(const ChunkGrid::Coord& coord)
    {
        static const XMFLOAT4 kColors[] =
        {
            { 0.36f, 0.62f, 0.36f, 1.0f },
            { 0.46f, 0.58f, 0.30f, 1.0f },
            { 0.32f, 0.54f, 0.46f, 1.0f },
            { 0.52f, 0.50f, 0.32f, 1.0f },
            { 0.38f, 0.48f, 0.58f, 1.0f },
            { 0.50f, 0.42f, 0.50f, 1.0f },
        };
        constexpr unsigned int kCount = static_cast<unsigned int>(sizeof(kColors) / sizeof(kColors[0]));

        const unsigned int ux = static_cast<unsigned int>(coord.x);
        const unsigned int uz = static_cast<unsigned int>(coord.z);
        const unsigned int hash = (ux * 73856093u) ^ (uz * 19349663u);

        return kColors[hash % kCount];
    }

    float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }
}

const wchar_t* ToDisplayName(ChunkDisplayMode mode)
{
    switch (mode)
    {
    case ChunkDisplayMode::SolidWireframe: return L"솔리드 + 와이어프레임";
    case ChunkDisplayMode::Wireframe:      return L"와이어프레임";
    case ChunkDisplayMode::Solid:          return L"솔리드";
    default:                               return L"알 수 없음";
    }
}

void InfiniteTerrainRenderer::Start()
{
    // 실제 리소스 생성은 첫 Update 때 한다 (디바이스가 준비된 시점을 보장하기 위해)
    UpdateFogDistances();
}

void InfiniteTerrainRenderer::Destroy()
{
    ReleaseAllChunks();
    m_vertexBufferPool.clear();

    m_indexBuffer.Reset();
    m_constantBuffer.Reset();
    m_inputLayout.Reset();
    m_vertexShader.Reset();
    m_pixelShader.Reset();
    m_solidRasterizer.Reset();
    m_wireRasterizer.Reset();
    m_depthState.Reset();

    m_indexCount = 0;
    m_vertexBufferBytes = 0;
    m_resourcesReady = false;
}

//=====================================================================
// 설정
//=====================================================================

void InfiniteTerrainRenderer::SetHeightFunction(const GridMesh::HeightFunc& heightFunc)
{
    m_heightFunc = heightFunc;

    // 지형의 정의가 바뀌었으므로 이미 구워둔 청크는 전부 무효다.
    RebuildAll();
}

void InfiniteTerrainRenderer::SetChunkResolution(int divisions, float cellSize)
{
    divisions = std::clamp(divisions, 8, 512);
    cellSize = std::max(cellSize, 0.0001f);

    if (m_divisions == divisions && m_cellSize == cellSize)
    {
        return;
    }

    m_divisions = divisions;
    m_cellSize = cellSize;

    // 정점 수와 인덱스가 통째로 달라지므로 공용 인덱스 버퍼와 버퍼 풀까지 버려야 한다.
    m_layoutDirty = true;
}

void InfiniteTerrainRenderer::SetKeepRadius(int radius)
{
    radius = std::clamp(radius, 1, 6);
    if (m_keepRadius == radius)
    {
        return;
    }

    m_keepRadius = radius;
    UpdateFogDistances();

    // 반경을 줄였을 때 바깥 청크는 다음 UpdateStreaming 에서 해제 반경 판정에 걸려
    // 자연스럽게 빠진다 -- 여기서 따로 지울 필요가 없다.
}

void InfiniteTerrainRenderer::SetMaxBuildsPerFrame(int count)
{
    m_maxBuildsPerFrame = std::clamp(count, 1, 16);
}

void InfiniteTerrainRenderer::CycleDisplayMode()
{
    const int next = (static_cast<int>(m_displayMode) + 1) % static_cast<int>(ChunkDisplayMode::Count);
    m_displayMode = static_cast<ChunkDisplayMode>(next);
}

void InfiniteTerrainRenderer::RebuildAll()
{
    ReleaseAllChunks();
}

void InfiniteTerrainRenderer::UpdateFogDistances()
{
    // 유지 영역은 한 변이 (2*radius+1) 청크인 정사각형이다. 그 정사각형의 "반쪽 폭"이
    // 카메라에서 가장 가까운 경계까지의 거리이므로, 그보다 살짝 앞에서 안개가 끝나야
    // 청크가 사라지는 순간이 가려진다.
    const float loadDistance = (static_cast<float>(m_keepRadius) + 0.5f) * GetChunkWorldSize();

    m_fogEnd = loadDistance * 0.98f;
    m_fogStart = loadDistance * 0.45f;
}

//=====================================================================
// 프레임 루프
//=====================================================================

void InfiniteTerrainRenderer::Update(float deltaTime)
{
    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return;
    }

    Camera* camera = Camera::GetMain();
    if (camera == nullptr)
    {
        return;
    }

    if (!m_resourcesReady)
    {
        if (m_resourceCreationFailed)
        {
            return;   // 이미 실패했으면 매 프레임 다시 시도하지 않는다
        }

        if (!CreateDeviceResources())
        {
            m_resourceCreationFailed = true;
            return;
        }
        m_resourcesReady = true;
    }

    if (m_layoutDirty)
    {
        ApplyLayoutChange();
    }

    for (auto& pair : m_chunks)
    {
        pair.second.age += deltaTime;
    }

    UpdateStreaming(deltaTime, camera->GetWorldPosition());
}

void InfiniteTerrainRenderer::UpdateStreaming(float deltaTime, const XMFLOAT3& cameraPosition)
{
    (void)deltaTime;

    m_builtLastFrame = 0;
    m_lastFrameBuildMs = 0.0;

    const float chunkWorldSize = GetChunkWorldSize();

    m_centerCoord = ChunkGrid::WorldToChunk(cameraPosition.x, cameraPosition.z, chunkWorldSize);
    m_hasCenter = true;

    if (m_streamingPaused)
    {
        // 카메라만 움직여서 지금 로드 경계가 어디인지 눈으로 확인하는 모드.
        // 중심 좌표는 계속 갱신해 둔다(HUD 에서 "중심이 이만큼 벗어났다"가 보이도록).
        return;
    }

    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return;
    }

    ID3D11Device* device = framework->GetRenderer().GetDevice();
    ID3D11DeviceContext* context = framework->GetRenderer().GetContext();
    if (device == nullptr || context == nullptr)
    {
        return;
    }

    // ---------------- 1) 유지 범위를 벗어난 청크 해제 ----------------
    // 해제 반경을 생성 반경보다 한 칸 크게 잡는다(히스테리시스). 이것이 없으면
    // 카메라가 청크 경계 위에서 조금만 흔들려도 같은 청크를 만들었다 버렸다 한다.
    const int releaseRadius = GetReleaseRadius();

    m_releaseScratch.clear();
    for (const auto& pair : m_chunks)
    {
        if (ChunkGrid::ChebyshevDistance(pair.second.coord, m_centerCoord) > releaseRadius)
        {
            m_releaseScratch.push_back(pair.first);
        }
    }

    for (uint64_t key : m_releaseScratch)
    {
        auto it = m_chunks.find(key);
        if (it == m_chunks.end())
        {
            continue;
        }

        ReleaseChunk(it->second);
        m_chunks.erase(it);
        ++m_totalReleased;
    }

    // ---------------- 2) 지금 있어야 할 청크 목록 (가까운 순) ----------------
    ChunkGrid::CollectDesired(m_centerCoord, m_keepRadius, cameraPosition, chunkWorldSize, m_desired);

    // ---------------- 3) 예산만큼만 생성 ----------------
    // 목록이 가까운 순으로 정렬돼 있으므로, 앞에서부터 예산이 떨어질 때까지 만들면
    // 그것이 곧 "카메라에 가까운 청크부터" 라는 우선순위가 된다. 나머지는 다음 프레임에
    // 다시 같은 방식으로 뽑히므로 별도의 큐 자료구조를 들고 있을 필요가 없다
    // (카메라가 움직이면 우선순위도 자동으로 다시 계산된다).
    int budget = std::max(m_maxBuildsPerFrame, 0);
    bool buildFailed = false;
    size_t missing = 0;

    const auto frameBuildStart = std::chrono::steady_clock::now();

    for (const ChunkGrid::Coord& coord : m_desired)
    {
        if (m_chunks.find(ChunkGrid::MakeKey(coord)) != m_chunks.end())
        {
            continue;
        }

        if (!buildFailed && budget > 0)
        {
            if (BuildChunk(coord, device, context))
            {
                --budget;
                ++m_builtLastFrame;
                continue;
            }

            // 한 번 실패하면 이 프레임에는 더 시도하지 않는다 (디바이스 문제일 가능성이 크다)
            buildFailed = true;
        }

        ++missing;
    }

    m_queuedCount = missing;

    if (m_builtLastFrame > 0)
    {
        const auto frameBuildEnd = std::chrono::steady_clock::now();
        m_lastFrameBuildMs = std::chrono::duration<double, std::milli>(frameBuildEnd - frameBuildStart).count();
    }
}

bool InfiniteTerrainRenderer::BuildChunk(const ChunkGrid::Coord& coord,
                                         ID3D11Device* device, ID3D11DeviceContext* context)
{
    if (device == nullptr || context == nullptr)
    {
        return false;
    }

    const float chunkWorldSize = GetChunkWorldSize();
    const XMFLOAT3 center = ChunkGrid::ChunkCenter(coord, chunkWorldSize);

    const auto buildStart = std::chrono::steady_clock::now();

    // 정점 위치는 원점 중심으로 만들고, 높이만 월드 좌표에서 평가한다.
    // 이웃 청크의 맞닿은 정점은 같은 월드 좌표를 넣게 되므로 높이가 정확히 같아진다.
    const GridMesh::MeshData mesh = GridMesh::GenerateChunk(
        m_divisions, m_divisions, m_cellSize, m_heightFunc, center.x, center.z);

    if (mesh.vertices.empty() || mesh.indices.empty())
    {
        return false;
    }

    if (!EnsureIndexBuffer(mesh, device))
    {
        return false;
    }

    const UINT vertexBytes = static_cast<UINT>(sizeof(GridMesh::Vertex) * mesh.vertices.size());

    Chunk chunk;
    chunk.coord = coord;
    chunk.age = 0.0f;

    if (!m_vertexBufferPool.empty() && m_vertexBufferBytes == vertexBytes)
    {
        // 풀에서 꺼내 덮어쓴다 -- 버퍼 생성/파괴가 매 프레임 반복되는 것을 막는다.
        chunk.vertexBuffer = m_vertexBufferPool.back();
        m_vertexBufferPool.pop_back();

        context->UpdateSubresource(chunk.vertexBuffer.Get(), 0, nullptr, mesh.vertices.data(), 0, 0);
    }
    else
    {
        if (m_vertexBufferBytes != vertexBytes)
        {
            // 크기가 달라졌으면 풀에 남아있는 버퍼는 전부 못 쓴다.
            // (그대로 두면 다음 청크가 크기가 맞지 않는 버퍼를 꺼내 쓰게 된다)
            m_vertexBufferPool.clear();
        }

        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.ByteWidth = vertexBytes;
        vbDesc.Usage = D3D11_USAGE_DEFAULT;   // 나중에 덮어써야 하므로 IMMUTABLE 이 아니다
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vbData = {};
        vbData.pSysMem = mesh.vertices.data();

        if (FAILED(device->CreateBuffer(&vbDesc, &vbData, &chunk.vertexBuffer)))
        {
            return false;
        }

        m_vertexBufferBytes = vertexBytes;
    }

    // ---- 월드 공간 AABB (절두체 컬링용) ----
    // 가로/세로는 청크 크기 그대로지만, 높이는 실제로 구워진 정점에서 재야 한다.
    float minY = (std::numeric_limits<float>::max)();
    float maxY = -(std::numeric_limits<float>::max)();

    for (const GridMesh::Vertex& vertex : mesh.vertices)
    {
        minY = (std::min)(minY, vertex.position.y);
        maxY = (std::max)(maxY, vertex.position.y);
    }

    const float half = chunkWorldSize * 0.5f;
    chunk.boundsMin = XMFLOAT3(center.x - half, minY, center.z - half);
    chunk.boundsMax = XMFLOAT3(center.x + half, maxY, center.z + half);

    const auto buildEnd = std::chrono::steady_clock::now();

    m_lastBuildMs = std::chrono::duration<double, std::milli>(buildEnd - buildStart).count();
    m_avgBuildMs = (m_avgBuildMs * static_cast<double>(m_totalBuilt) + m_lastBuildMs)
                 / static_cast<double>(m_totalBuilt + 1);
    ++m_totalBuilt;

    m_chunks.emplace(ChunkGrid::MakeKey(coord), std::move(chunk));
    return true;
}

void InfiniteTerrainRenderer::ReleaseChunk(Chunk& chunk)
{
    if (!chunk.vertexBuffer)
    {
        return;
    }

    // 풀이 무한정 커지지 않도록 상한을 둔다. 유지 개수보다 조금만 더 들고 있으면
    // 카메라가 한 방향으로 계속 움직이는 최악의 경우에도 재사용률이 떨어지지 않는다.
    const size_t poolLimit = GetDesiredChunkCount() + 8;

    if (m_vertexBufferPool.size() < poolLimit)
    {
        m_vertexBufferPool.push_back(chunk.vertexBuffer);
    }

    chunk.vertexBuffer.Reset();
}

void InfiniteTerrainRenderer::ReleaseAllChunks()
{
    for (auto& pair : m_chunks)
    {
        ReleaseChunk(pair.second);
        ++m_totalReleased;
    }

    m_chunks.clear();
    m_queuedCount = 0;
    m_drawnChunkCount = 0;
    m_drawnTriangleCount = 0;
}

void InfiniteTerrainRenderer::ApplyLayoutChange()
{
    // 분할 수/셀 크기가 바뀌면 정점 수와 인덱스가 통째로 달라진다.
    // 공용 인덱스 버퍼와 버퍼 풀까지 전부 버리고 처음부터 다시 만든다.
    ReleaseAllChunks();

    m_vertexBufferPool.clear();
    m_vertexBufferBytes = 0;

    m_indexBuffer.Reset();
    m_indexCount = 0;

    UpdateFogDistances();

    m_layoutDirty = false;
}

//=====================================================================
// D3D 리소스
//=====================================================================

bool InfiniteTerrainRenderer::EnsureIndexBuffer(const GridMesh::MeshData& mesh, ID3D11Device* device)
{
    if (m_indexBuffer)
    {
        return true;
    }

    // 모든 청크의 인덱스가 완전히 같으므로 한 벌만 만들어 전부가 공유한다.
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * mesh.indices.size());
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = mesh.indices.data();

    if (FAILED(device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer)))
    {
        return false;
    }

    m_indexCount = static_cast<UINT>(mesh.indices.size());
    return true;
}

bool InfiniteTerrainRenderer::CreateDeviceResources()
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

    // ---------------- 셰이더 ----------------
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

    if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                          nullptr, &m_vertexShader)))
    {
        return false;
    }

    if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                         nullptr, &m_pixelShader)))
    {
        return false;
    }

    // ---------------- 입력 레이아웃 (GridMesh::Vertex 와 순서가 같아야 한다) ----------------
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        // 10번은 쓰지 않지만 GridMesh::Vertex 에 들어있는 필드라 레이아웃에는 있어야 한다.
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,    0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if (FAILED(device->CreateInputLayout(layout, ARRAYSIZE(layout),
                                         vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                         &m_inputLayout)))
    {
        return false;
    }

    // ---------------- 상수 버퍼 ----------------
    // 청크마다 월드 행렬과 색이 다르므로 청크 하나당 한 번씩 다시 올린다.
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(ChunkConstants);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer)))
    {
        return false;
    }

    // ---------------- 래스터라이저 ----------------
    D3D11_RASTERIZER_DESC solidDesc = {};
    solidDesc.FillMode = D3D11_FILL_SOLID;
    solidDesc.CullMode = D3D11_CULL_NONE;
    solidDesc.FrontCounterClockwise = FALSE;
    solidDesc.DepthClipEnable = TRUE;

    if (FAILED(device->CreateRasterizerState(&solidDesc, &m_solidRasterizer)))
    {
        return false;
    }

    D3D11_RASTERIZER_DESC wireDesc = solidDesc;
    wireDesc.FillMode = D3D11_FILL_WIREFRAME;
    wireDesc.DepthBias = -2000;
    wireDesc.SlopeScaledDepthBias = -1.0f;
    wireDesc.DepthBiasClamp = 0.0f;

    if (FAILED(device->CreateRasterizerState(&wireDesc, &m_wireRasterizer)))
    {
        return false;
    }

    // ---------------- 깊이 상태 ----------------
    // Direct2D(텍스트)가 파이프라인 상태를 바꿔놓기 때문에 매 프레임 직접 지정한다
    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
    depthDesc.StencilEnable = FALSE;

    if (FAILED(device->CreateDepthStencilState(&depthDesc, &m_depthState)))
    {
        return false;
    }

    return true;
}

//=====================================================================
// 렌더링
//=====================================================================

XMFLOAT4 InfiniteTerrainRenderer::ResolveChunkColor(const Chunk& chunk) const
{
    XMFLOAT4 color = m_chunkColorMode ? ChunkPaletteColor(chunk.coord) : m_solidColor;

    if (m_highlightNewChunks && chunk.age < kHighlightSeconds)
    {
        // 갓 만들어졌을 때 가장 진하고, kHighlightSeconds 에 걸쳐 원래 색으로 돌아온다.
        const float t = 1.0f - (chunk.age / kHighlightSeconds);

        color.x = Lerp(color.x, kHighlightColor.x, t);
        color.y = Lerp(color.y, kHighlightColor.y, t);
        color.z = Lerp(color.z, kHighlightColor.z, t);
    }

    return color;
}

void InfiniteTerrainRenderer::UpdateConstantBuffer(ID3D11DeviceContext* context,
                                                   const Chunk& chunk,
                                                   const XMMATRIX& viewProj,
                                                   const XMFLOAT3& cameraPosition,
                                                   const XMFLOAT4& baseColor,
                                                   bool useLighting)
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    ChunkConstants* constants = static_cast<ChunkConstants*>(mapped.pData);

    // 청크의 월드 행렬은 평행이동뿐이다 -- 정점은 원점 중심으로 구워져 있고,
    // 청크의 위치는 오직 이 행렬로만 표현된다.
    const XMFLOAT3 center = ChunkGrid::ChunkCenter(chunk.coord, GetChunkWorldSize());
    const XMMATRIX world = XMMatrixTranslation(center.x, 0.0f, center.z);

    // HLSL 의 기본 행렬 규약(열 우선)에 맞추기 위해 전치해서 넘긴다
    XMStoreFloat4x4(&constants->world, XMMatrixTranspose(world));
    XMStoreFloat4x4(&constants->worldViewProj, XMMatrixTranspose(world * viewProj));

    constants->baseColor = baseColor;

    XMVECTOR lightDir = XMVector3Normalize(XMLoadFloat3(&m_lightDirection));
    XMStoreFloat3(&constants->lightDirection, lightDir);

    constants->useLighting = useLighting ? 1.0f : 0.0f;
    constants->cameraPosition = cameraPosition;
    constants->cellSize = m_cellSize * m_checkerScale;

    constants->fogParams = XMFLOAT4(m_fogStart, m_fogEnd, 0.0f, m_fogEnabled ? 1.0f : 0.0f);
    constants->fogColor = m_fogColor;

    context->Unmap(m_constantBuffer.Get(), 0);
}

void InfiniteTerrainRenderer::Render()
{
    m_drawnChunkCount = 0;
    m_drawnTriangleCount = 0;

    if (!m_resourcesReady || !m_indexBuffer || m_indexCount == 0 || m_chunks.empty())
    {
        return;
    }

    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return;
    }

    Camera* camera = Camera::GetMain();
    if (camera == nullptr)
    {
        return;
    }

    ID3D11DeviceContext* context = framework->GetRenderer().GetContext();
    if (context == nullptr)
    {
        return;
    }

    const XMMATRIX viewProj = camera->GetViewProjectionMatrix();
    const XMFLOAT3 cameraPosition = camera->GetWorldPosition();

    // ---------------- 이번 프레임에 그릴 청크 고르기 ----------------
    // 컬링은 "그리기"만 건너뛴다 -- 생성/해제(스트리밍)와는 완전히 별개다.
    Frustum frustum;
    frustum.ExtractFromViewProjection(viewProj);

    m_drawList.clear();
    m_drawList.reserve(m_chunks.size());

    for (const auto& pair : m_chunks)
    {
        const Chunk& chunk = pair.second;
        if (!chunk.vertexBuffer)
        {
            continue;
        }

        if (m_frustumCullingEnabled && !frustum.IntersectsAABB(chunk.boundsMin, chunk.boundsMax))
        {
            continue;
        }

        m_drawList.push_back(&chunk);
    }

    m_drawnChunkCount = m_drawList.size();
    m_drawnTriangleCount = m_drawnChunkCount * GetTrianglesPerChunk();

    if (m_drawList.empty())
    {
        return;
    }

    // ---------------- 파이프라인 설정 ----------------
    // Direct2D 텍스트 렌더링이 상태를 바꿔놓으므로 매 프레임 전부 다시 지정한다
    const UINT stride = sizeof(GridMesh::Vertex);
    const UINT offset = 0;

    context->IASetInputLayout(m_inputLayout.Get());
    context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
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

    // 앞 기법이 걸어둔 텍스처가 남아있지 않도록 확실히 풀어준다
    {
        ID3D11ShaderResourceView* nullSrv[2] = { nullptr, nullptr };
        ID3D11SamplerState* nullSampler[2] = { nullptr, nullptr };

        context->PSSetShaderResources(0, 2, nullSrv);
        context->PSSetSamplers(0, 2, nullSampler);
    }

    // ---------------- 솔리드 패스 ----------------
    if (m_displayMode != ChunkDisplayMode::Wireframe)
    {
        context->RSSetState(m_solidRasterizer.Get());

        for (const Chunk* chunk : m_drawList)
        {
            context->IASetVertexBuffers(0, 1, chunk->vertexBuffer.GetAddressOf(), &stride, &offset);
            UpdateConstantBuffer(context, *chunk, viewProj, cameraPosition, ResolveChunkColor(*chunk), true);
            context->DrawIndexed(m_indexCount, 0, 0);
        }
    }

    // ---------------- 와이어프레임 패스 ----------------
    if (m_displayMode != ChunkDisplayMode::Solid)
    {
        context->RSSetState(m_wireRasterizer.Get());

        for (const Chunk* chunk : m_drawList)
        {
            context->IASetVertexBuffers(0, 1, chunk->vertexBuffer.GetAddressOf(), &stride, &offset);
            UpdateConstantBuffer(context, *chunk, viewProj, cameraPosition, m_wireColor, false);
            context->DrawIndexed(m_indexCount, 0, 0);
        }
    }

    // 다음에 그릴 것들을 위해 기본 상태로 돌려놓는다
    context->RSSetState(nullptr);
}
