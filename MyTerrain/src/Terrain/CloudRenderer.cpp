#include "CloudRenderer.h"
#include "../Framework/Framework.h"
#include "../Framework/Camera.h"
#include "../Framework/ShaderUtil.h"
#include "../GameObject/GameObject.h"
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace DirectX;

namespace
{
    constexpr wchar_t kShaderFile[] = L"Cloud.hlsl";
}

void CloudRenderer::Start()
{
    // 실제 GPU 리소스 생성은 SkyRenderer 와 같은 이유로 첫 Render 때 수행한다
    // (디바이스가 준비된 시점을 보장하기 위해).
}

void CloudRenderer::Destroy()
{
    m_vertexShader.Reset();
    m_pixelShader.Reset();
    m_inputLayout.Reset();
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_constantBuffer.Reset();
    m_rasterizerState.Reset();
    m_depthState.Reset();
    m_blendState.Reset();

    m_resourcesReady = false;
    m_resourceCreationFailed = false;
    m_meshDirty = true;
}

void CloudRenderer::SetDomeSegments(int latitudeSegments, int longitudeSegments)
{
    latitudeSegments = std::max(latitudeSegments, 1);
    longitudeSegments = std::max(longitudeSegments, 3);

    if (m_latitudeSegments == latitudeSegments && m_longitudeSegments == longitudeSegments)
    {
        return;
    }

    m_latitudeSegments = latitudeSegments;
    m_longitudeSegments = longitudeSegments;
    m_meshDirty = true;
}

void CloudRenderer::SetCoverage(float coverage01)
{
    m_coverage = std::clamp(coverage01, 0.05f, 0.95f);
}

void CloudRenderer::SetSoftness(float softness)
{
    m_softness = std::clamp(softness, 0.01f, 0.5f);
}

void CloudRenderer::SetNoiseScale(float scale)
{
    m_noiseScale = std::clamp(scale, 0.3f, 8.0f);
}

void CloudRenderer::SetWarpStrength(float strength)
{
    m_warpStrength = std::clamp(strength, 0.0f, 5.0f);
}

void CloudRenderer::SetWindDirectionDegrees(float degrees)
{
    m_windDirectionDegrees = std::fmod(degrees, 360.0f);
    if (m_windDirectionDegrees < 0.0f)
    {
        m_windDirectionDegrees += 360.0f;
    }
}

void CloudRenderer::SetWindSpeed(float speed)
{
    m_windSpeed = std::clamp(speed, 0.0f, 2.0f);
}

void CloudRenderer::SetRimPower(float power)
{
    m_rimPower = std::clamp(power, 0.5f, 16.0f);
}

void CloudRenderer::SetSunElevationDegrees(float degrees)
{
    // 태양 고도 -10도(아직 어둠) ~ +15도(완전히 밝음) 구간에서 부드럽게 낮/밤 밝기를
    // 전환한다. SkyRenderer 의 7-키프레임 색 보간처럼 정교하지는 않지만, 구름은
    // "하늘이 밝은 동안만 하얗고 그 외엔 어둡다" 정도로 단순화해도 충분하다.
    constexpr float kNightElevation = -10.0f;
    constexpr float kDayElevation = 15.0f;

    const float t = std::clamp((degrees - kNightElevation) / (kDayElevation - kNightElevation), 0.0f, 1.0f);
    m_dayFactor = t * t * (3.0f - 2.0f * t); // smoothstep
}

bool CloudRenderer::CreateDeviceResources()
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

    // ---------------- 입력 레이아웃 (SkyDome::Vertex 와 순서가 같아야 한다) ----------------
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = device->CreateInputLayout(layout, ARRAYSIZE(layout),
                                   vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                   &m_inputLayout);
    if (FAILED(hr)) return false;

    // ---------------- 상수 버퍼 ----------------
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(CloudConstants);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer);
    if (FAILED(hr)) return false;

    // ---------------- 래스터라이저 상태 (SkyRenderer 와 같은 이유로 컬링을 끈다) ----------------
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.FrontCounterClockwise = FALSE;
    rasterDesc.DepthClipEnable = TRUE;

    hr = device->CreateRasterizerState(&rasterDesc, &m_rasterizerState);
    if (FAILED(hr)) return false;

    // ---------------- 깊이 상태 (SkyRenderer 와 동일한 이유) ----------------
    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    depthDesc.StencilEnable = FALSE;

    hr = device->CreateDepthStencilState(&depthDesc, &m_depthState);
    if (FAILED(hr)) return false;

    // ---------------- 블렌드 상태 (이 프로젝트 최초의 알파 블렌딩) ----------------
    // 표준 비-프리멀티플라이드 알파: srcColor*srcAlpha + dstColor*(1-srcAlpha).
    // 구름 알파(커버리지)가 0인 픽셀은 완전히 투명해서 이미 그려진 하늘이 그대로 보인다.
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = device->CreateBlendState(&blendDesc, &m_blendState);
    if (FAILED(hr)) return false;

    return true;
}

bool CloudRenderer::RebuildMesh()
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

    // SkyDome::Generate 를 그대로 재사용한다 -- 방향 벡터만 있으면 되는 반구 메시는
    // 스카이와 구름이 완전히 같은 모양이라, 반지름만 다르게 주면 된다.
    const SkyDome::MeshData mesh = SkyDome::Generate(m_latitudeSegments, m_longitudeSegments);
    m_indexCount = mesh.indices.size();

    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();

    // ---- 정점 버퍼 ----
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = static_cast<UINT>(sizeof(SkyDome::Vertex) * mesh.vertices.size());
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = mesh.vertices.data();

    HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, &m_vertexBuffer);
    if (FAILED(hr)) return false;

    // ---- 인덱스 버퍼 ----
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * mesh.indices.size());
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = mesh.indices.data();

    hr = device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer);
    if (FAILED(hr)) return false;

    m_meshDirty = false;
    return true;
}

void CloudRenderer::UpdateConstantBuffer(ID3D11DeviceContext* context, const XMMATRIX& worldViewProj, float timeSeconds)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
    {
        return;
    }

    CloudConstants constants{};
    XMStoreFloat4x4(&constants.worldViewProj, XMMatrixTranspose(worldViewProj));

    constants.colorAndCoverage = XMFLOAT4(m_cloudColor.x, m_cloudColor.y, m_cloudColor.z, m_coverage);
    constants.sunDirAndBrightness = XMFLOAT4(m_sunDirection.x, m_sunDirection.y, m_sunDirection.z, m_dayFactor);
    constants.warpParams = XMFLOAT4(m_noiseScale, m_warpStrength, timeSeconds, m_softness);

    const float windRad = XMConvertToRadians(m_windDirectionDegrees);
    constants.windAndRim = XMFLOAT4(std::cos(windRad), std::sin(windRad), m_windSpeed, m_rimPower);

    std::memcpy(mapped.pData, &constants, sizeof(constants));
    context->Unmap(m_constantBuffer.Get(), 0);
}

void CloudRenderer::Render()
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

    // ---------------- 리소스 준비 ----------------
    if (!m_resourcesReady)
    {
        if (m_resourceCreationFailed)
        {
            return;
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

    // ---------------- 카메라를 절대 벗어나지 않는다 (SkyRenderer 와 동일) ----------------
    const XMFLOAT3 cameraPosition = camera->GetWorldPosition();
    const XMMATRIX world = XMMatrixScaling(m_radius, m_radius, m_radius) *
                           XMMatrixTranslation(cameraPosition.x, cameraPosition.y, cameraPosition.z);

    const XMMATRIX viewProj = camera->GetViewProjectionMatrix();
    const XMMATRIX worldViewProj = world * viewProj;

    // 애니메이션 시간은 프레임워크의 누적 경과시간을 그대로 쓴다 -- 구름은 항상 흘러야
    // 하므로 SkyControlComponent 의 시간 스크럽/자동재생과는 독립적으로 진행된다.
    const float timeSeconds = framework->GetTime().GetTotalTime();

    // ---------------- 파이프라인 설정 ----------------
    UINT stride = sizeof(SkyDome::Vertex);
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

    UpdateConstantBuffer(context, worldViewProj, timeSeconds);
    context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

    context->OMSetBlendState(m_blendState.Get(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_depthState.Get(), 0);
    context->RSSetState(m_rasterizerState.Get());

    context->DrawIndexed(static_cast<UINT>(m_indexCount), 0, 0);

    // 다음 컴포넌트가 이 컴포넌트의 상태(특히 블렌드 상태)를 물려받지 않도록 되돌려 놓는다.
    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    context->RSSetState(nullptr);
    context->OMSetDepthStencilState(nullptr, 0);
}
