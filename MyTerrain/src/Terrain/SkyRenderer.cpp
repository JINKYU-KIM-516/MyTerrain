#include "SkyRenderer.h"
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
    constexpr wchar_t kShaderFile[] = L"Sky.hlsl";

    // ---- 시간대 키프레임 ----
    // 태양의 "고도"는 키프레임과 별개로 m_timeOfDay 하나의 사인 곡선에서 계산한다
    // (RecomputeFromTimeOfDay 참고 -- 자정에 가장 낮고 정오에 가장 높다).
    // 여기서는 색만 t 로 구간을 찾아 선형 보간한다.
    struct SkyKeyframe
    {
        float t;
        XMFLOAT3 horizon;
        XMFLOAT3 zenith;
        XMFLOAT3 sunColor;
    };

    const SkyKeyframe kKeyframes[] =
    {
        { 0.00f, { 0.020f, 0.020f, 0.060f }, { 0.000f, 0.000f, 0.020f }, { 0.30f, 0.35f, 0.55f } }, // 자정
        { 0.22f, { 0.050f, 0.050f, 0.120f }, { 0.020f, 0.020f, 0.080f }, { 0.40f, 0.40f, 0.55f } }, // 여명 직전
        { 0.27f, { 0.950f, 0.550f, 0.350f }, { 0.250f, 0.200f, 0.450f }, { 1.00f, 0.55f, 0.25f } }, // 일출
        { 0.50f, { 0.650f, 0.800f, 0.950f }, { 0.250f, 0.450f, 0.850f }, { 1.00f, 0.95f, 0.85f } }, // 정오
        { 0.73f, { 0.950f, 0.500f, 0.300f }, { 0.250f, 0.180f, 0.400f }, { 1.00f, 0.45f, 0.20f } }, // 일몰
        { 0.78f, { 0.050f, 0.050f, 0.120f }, { 0.020f, 0.020f, 0.080f }, { 0.40f, 0.40f, 0.55f } }, // 황혼 직후
        { 1.00f, { 0.020f, 0.020f, 0.060f }, { 0.000f, 0.000f, 0.020f }, { 0.30f, 0.35f, 0.55f } }, // 자정(순환)
    };
    constexpr int kKeyframeCount = static_cast<int>(sizeof(kKeyframes) / sizeof(kKeyframes[0]));

    XMFLOAT3 Lerp3(const XMFLOAT3& a, const XMFLOAT3& b, float t)
    {
        return XMFLOAT3(
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t);
    }
}

void SkyRenderer::Start()
{
    // 실제 GPU 리소스 생성은 TerrainRenderer 와 같은 이유로 첫 Render 때 수행한다
    // (디바이스가 준비된 시점을 보장하기 위해). 여기서는 시간대 파생값만 채워둔다.
    RecomputeFromTimeOfDay();
}

void SkyRenderer::Destroy()
{
    m_vertexShader.Reset();
    m_pixelShader.Reset();
    m_inputLayout.Reset();
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_constantBuffer.Reset();
    m_rasterizerState.Reset();
    m_depthState.Reset();

    m_resourcesReady = false;
    m_resourceCreationFailed = false;
    m_meshDirty = true;
}

void SkyRenderer::SetDomeSegments(int latitudeSegments, int longitudeSegments)
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

void SkyRenderer::SetTimeOfDay(float t01)
{
    // 0~1 순환 구간으로 감아준다 (음수 방향으로 스크럽해도, 1을 넘겨 자동재생해도 자연스럽게).
    t01 = std::fmod(t01, 1.0f);
    if (t01 < 0.0f)
    {
        t01 += 1.0f;
    }

    m_timeOfDay = t01;
    RecomputeFromTimeOfDay();
}

void SkyRenderer::SetSunAngularRadius(float degrees)
{
    m_sunAngularRadiusDegrees = std::clamp(degrees, 0.1f, 20.0f);
}

void SkyRenderer::SetSunGlowExponent(float exponent)
{
    m_sunGlowExponent = std::clamp(exponent, 2.0f, 256.0f);
}

float SkyRenderer::GetSunElevationDegrees() const
{
    return XMConvertToDegrees(std::asin(std::clamp(m_sunDirection.y, -1.0f, 1.0f)));
}

void SkyRenderer::RecomputeFromTimeOfDay()
{
    // ---- 태양 고도: t 하나로 매끄럽게 -90도(자정) ~ +80도(정오)를 오간다 ----
    // t=0.25(대략 일출) -> 0도, t=0.5(정오) -> 최고 고도, t=0.75(대략 일몰) -> 0도,
    // t=0/1(자정) -> 최저 고도. 사인 한 주기로 이 네 지점을 모두 만족시킨다.
    constexpr float kMaxElevationDeg = 80.0f;   // 정오에도 살짝 기울여서(진짜 정수리는 피한다)
    constexpr float kAzimuthDeg = 35.0f;        // 태양이 남북 방향으로도 살짝 걸치도록

    const float elevationRad = std::sin((m_timeOfDay - 0.25f) * XM_2PI) * XMConvertToRadians(kMaxElevationDeg);
    const float azimuthRad = XMConvertToRadians(kAzimuthDeg);

    m_sunDirection = XMFLOAT3(
        std::cos(elevationRad) * std::sin(azimuthRad),
        std::sin(elevationRad),
        std::cos(elevationRad) * std::cos(azimuthRad));

    // ---- 색상: 키프레임을 t 로 구간을 찾아 선형 보간 ----
    int index = 0;
    while (index < kKeyframeCount - 2 && m_timeOfDay > kKeyframes[index + 1].t)
    {
        ++index;
    }

    const SkyKeyframe& a = kKeyframes[index];
    const SkyKeyframe& b = kKeyframes[index + 1];
    const float span = std::max(b.t - a.t, 0.0001f);
    const float localT = std::clamp((m_timeOfDay - a.t) / span, 0.0f, 1.0f);

    m_horizonColor = Lerp3(a.horizon, b.horizon, localT);
    m_zenithColor = Lerp3(a.zenith, b.zenith, localT);
    m_sunColor = Lerp3(a.sunColor, b.sunColor, localT);
}

bool SkyRenderer::CreateDeviceResources()
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
    cbDesc.ByteWidth = sizeof(SkyConstants);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer);
    if (FAILED(hr)) return false;

    // ---------------- 래스터라이저 상태 ----------------
    // 카메라가 반구 안쪽에서 바라보므로, 삼각형을 감는 방향을 굳이 안쪽에서 보이게
    // 맞추는 대신 컬링 자체를 꺼버린다 (TerrainRenderer 의 솔리드 래스터라이저가
    // "평면을 아래에서도 볼 수 있도록" 컬링을 꺼두는 것과 같은 이유).
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.FrontCounterClockwise = FALSE;
    rasterDesc.DepthClipEnable = TRUE;

    hr = device->CreateRasterizerState(&rasterDesc, &m_rasterizerState);
    if (FAILED(hr)) return false;

    // ---------------- 깊이 상태 ----------------
    // 정점 셰이더가 NDC 깊이를 항상 1.0(가장 먼 값)으로 밀어붙이므로, 비교를
    // LESS_EQUAL 로 둬야 클리어된 깊이(1.0)와 같을 때도 통과해서 빈 배경을 채운다.
    // 쓰기는 꺼서 스카이가 이미 그려진 지형/오브젝트의 깊이를 덮어쓰지 않게 한다.
    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    depthDesc.StencilEnable = FALSE;

    hr = device->CreateDepthStencilState(&depthDesc, &m_depthState);
    if (FAILED(hr)) return false;

    return true;
}

bool SkyRenderer::RebuildMesh()
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

void SkyRenderer::UpdateConstantBuffer(ID3D11DeviceContext* context, const XMMATRIX& worldViewProj)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
    {
        return;
    }

    SkyConstants constants{};
    XMStoreFloat4x4(&constants.worldViewProj, XMMatrixTranspose(worldViewProj));
    constants.horizonColor = XMFLOAT4(m_horizonColor.x, m_horizonColor.y, m_horizonColor.z, 0.0f);
    constants.zenithColor = XMFLOAT4(m_zenithColor.x, m_zenithColor.y, m_zenithColor.z, 0.0f);

    const float cosThreshold = std::cos(XMConvertToRadians(m_sunAngularRadiusDegrees));
    constants.sunDirectionAndSize = XMFLOAT4(m_sunDirection.x, m_sunDirection.y, m_sunDirection.z, cosThreshold);
    constants.sunColorAndGlow = XMFLOAT4(m_sunColor.x, m_sunColor.y, m_sunColor.z, m_sunGlowExponent);

    std::memcpy(mapped.pData, &constants, sizeof(constants));
    context->Unmap(m_constantBuffer.Get(), 0);
}

void SkyRenderer::Render()
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

    // ---------------- 카메라를 절대 벗어나지 않는다 ----------------
    // 매 프레임 카메라 위치로 옮기고 반지름만큼 키운다. GameObject 의 Transform 은
    // 건드리지 않고, 이 Render() 안에서만 쓰는 임시 월드 행렬이다.
    const XMFLOAT3 cameraPosition = camera->GetWorldPosition();
    const XMMATRIX world = XMMatrixScaling(m_radius, m_radius, m_radius) *
                           XMMatrixTranslation(cameraPosition.x, cameraPosition.y, cameraPosition.z);

    const XMMATRIX viewProj = camera->GetViewProjectionMatrix();
    const XMMATRIX worldViewProj = world * viewProj;

    // ---------------- 파이프라인 설정 ----------------
    // Direct2D 텍스트 렌더링이 상태를 바꿔놓으므로(TerrainRenderer 와 같은 이유)
    // 매 프레임 전부 다시 지정한다.
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

    UpdateConstantBuffer(context, worldViewProj);
    context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_depthState.Get(), 0);
    context->RSSetState(m_rasterizerState.Get());

    context->DrawIndexed(static_cast<UINT>(m_indexCount), 0, 0);

    // 다음 컴포넌트가 이 컴포넌트의 상태를 물려받지 않도록 되돌려 놓는다
    // (TerrainRenderer 도 테셀레이션 경로 뒤에서 같은 습관을 쓴다).
    context->RSSetState(nullptr);
    context->OMSetDepthStencilState(nullptr, 0);
}
