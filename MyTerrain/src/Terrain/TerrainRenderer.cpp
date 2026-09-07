#include "TerrainRenderer.h"
#include "../Framework/Framework.h"
#include "../Framework/Camera.h"
#include "../Framework/ShaderUtil.h"
#include "../GameObject/GameObject.h"
#include <algorithm>
#include <chrono>

using namespace DirectX;

namespace
{
    constexpr wchar_t kShaderFile[] = L"BasicTerrain.hlsl";
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

    m_meshDirty = false;
    return true;
}

void TerrainRenderer::UpdateConstantBuffer(ID3D11DeviceContext* context,
                                           const XMMATRIX& world,
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

    context->Unmap(m_constantBuffer.Get(), 0);
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

    if (!m_vertexBuffer || !m_indexBuffer || m_indexCount == 0)
    {
        return;
    }

    ID3D11DeviceContext* context = framework->GetRenderer().GetContext();
    if (context == nullptr)
    {
        return;
    }

    // ---------------- 파이프라인 설정 ----------------
    // Direct2D 텍스트 렌더링이 상태를 바꿔놓으므로 매 프레임 전부 다시 지정한다
    UINT stride = sizeof(GridMesh::Vertex);
    UINT offset = 0;

    context->IASetInputLayout(m_inputLayout.Get());
    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
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

    // 높이맵 텍스처 (없으면 확실히 풀어준다 -- 앞 기법이 걸어둔 것이 남아있으면 안 된다)
    {
        ID3D11ShaderResourceView* srv = m_heightMapSRV.Get();
        ID3D11SamplerState* sampler = m_heightMapSampler.Get();

        context->PSSetShaderResources(0, 1, &srv);
        context->PSSetSamplers(0, 1, &sampler);
    }

    // ---------------- 행렬 ----------------
    XMMATRIX world = XMMatrixIdentity();
    if (GetGameObject() != nullptr && GetGameObject()->GetTransform() != nullptr)
    {
        world = GetGameObject()->GetTransform()->GetWorldMatrix();
    }

    const XMMATRIX viewProj = camera->GetViewProjectionMatrix();
    const XMFLOAT3 cameraPosition = camera->GetWorldPosition();

    // ---------------- 솔리드 패스 ----------------
    if (m_displayMode != TerrainDisplayMode::Wireframe)
    {
        UpdateConstantBuffer(context, world, viewProj, cameraPosition, m_solidColor, true);
        context->RSSetState(m_solidRasterizer.Get());
        context->DrawIndexed(m_indexCount, 0, 0);
    }

    // ---------------- 와이어프레임 패스 ----------------
    if (m_displayMode != TerrainDisplayMode::Solid)
    {
        UpdateConstantBuffer(context, world, viewProj, cameraPosition, m_wireColor, false);
        context->RSSetState(m_wireRasterizer.Get());
        context->DrawIndexed(m_indexCount, 0, 0);
    }

    // 다음에 그릴 것들을 위해 기본 상태로 돌려놓는다
    context->RSSetState(nullptr);
}
