#pragma once
#include "../GameObject/Component.h"
#include "SkyDome.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <algorithm>

// 9. 동적 왜곡 구름 (Perturbed Clouds) 을 그리는 컴포넌트.
//
// SkyRenderer(8번)와 마찬가지로 TerrainRenderer/BasicTerrain.hlsl 과 완전히 독립된
// 별도의 셰이더(Cloud.hlsl) / 파이프라인 상태 / 상수버퍼(b0)를 쓴다. SkyRenderer 도
// 전혀 건드리지 않는다 -- 매 프레임 SkyRenderer 의 태양 방향/고도를 "읽어오기만"
// 할 뿐이다(연결은 CloudControlComponent 가 맡는다).
//
// 지오메트리는 SkyDome::Generate 를 그대로 재사용한다(반지름만 스카이보다 작게) --
// 8번 문서/9번 설명 문서가 미리 적어둔 재사용 경로 그대로다. 새 지오메트리 모듈이
// 필요 없었다.
//
//   [카메라 추종]  SkyRenderer 와 동일하게 Render() 가 매 프레임 카메라 위치로
//                  월드 행렬을 다시 만든다.
//   [깊이]         Sky.hlsl 과 같은 z=w 트릭으로 NDC 깊이를 항상 1.0으로 고정하고,
//                  DepthWriteMask=ZERO, DepthFunc=LESS_EQUAL 을 쓴다. 스카이를 먼저
//                  그리고 구름을 그 다음에 그리면(Technique09_Clouds 참고) 깊이 비교가
//                  통과해서 구름이 하늘 위에 그려진다.
//   [블렌딩]       이 프로젝트 최초로 알파 블렌딩(BlendState)을 켠다. fbm 밀도를
//                  커버리지 임계값으로 걸러 알파로 쓴다 (Cloud.hlsl 참고).
//   [절차적 구름]  텍스처 없이 픽셀 셰이더에서 방향 벡터를 3D 좌표로 삼아 값노이즈
//                  기반 fbm 을 2-pass 도메인 워핑해서 구름 모양을 만든다.
//   [시간대 연동]  SetSunDirection / SetSunElevationDegrees 로 매 프레임 값을 받아
//                  주/야 밝기와 태양 쪽 림 라이트를 계산한다 (색 키프레임 테이블은
//                  두지 않고 고도 하나로 단순화했다).
class CloudRenderer : public Component
{
public:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    void Start() override;
    void Render() override;
    void Destroy() override;

    // SkyRenderer::SetRadius 와 같은 이유로 어떤 값을 넣어도 시각적으로 거의 같아
    // 보인다(깊이가 항상 1.0으로 고정되므로). 근평면보다만 크면 된다.
    void  SetRadius(float radius) { m_radius = std::max(radius, 1.0f); }
    float GetRadius() const { return m_radius; }

    void SetDomeSegments(int latitudeSegments, int longitudeSegments);

    // ---------------- 구름 모양 파라미터 ----------------
    // 임계값. 낮을수록 fbm 밀도가 더 쉽게 임계값을 넘어서 구름이 많아진다.
    void  SetCoverage(float coverage01);
    float GetCoverage() const { return m_coverage; }

    // 임계값 주변을 부드럽게 걸러내는 폭(가장자리 소프트니스).
    void  SetSoftness(float softness);
    float GetSoftness() const { return m_softness; }

    // 노이즈 좌표 스케일. 커질수록 구름 덩어리가 잘게 쪼개진다.
    void  SetNoiseScale(float scale);
    float GetNoiseScale() const { return m_noiseScale; }

    // 도메인 워핑 강도. 0이면 순수 fbm(규칙적인 뭉게구름), 커질수록 뒤틀리고 유기적인 모양.
    void  SetWarpStrength(float strength);
    float GetWarpStrength() const { return m_warpStrength; }

    // 바람 방향(xz 평면, 도 단위)과 속도(초당 노이즈 좌표 이동량).
    void  SetWindDirectionDegrees(float degrees);
    float GetWindDirectionDegrees() const { return m_windDirectionDegrees; }
    void  SetWindSpeed(float speed);
    float GetWindSpeed() const { return m_windSpeed; }

    // 태양 쪽 가장자리를 밝히는 림 라이트 지수. 클수록 좁고 진하게.
    void  SetRimPower(float power);
    float GetRimPower() const { return m_rimPower; }

    // ---------------- 8번 SkyRenderer 와의 연동 ----------------
    // 매 프레임 SkyRenderer::GetSunDirection() / GetSunElevationDegrees() 를 그대로
    // 읽어다 넘기면(CloudControlComponent::Update 참고), 하늘의 시간대가 바뀔 때
    // 구름의 밝기와 림 라이트 색도 같이 따라간다. SkyRenderer 는 전혀 건드리지 않는다.
    void SetSunDirection(const DirectX::XMFLOAT3& direction) { m_sunDirection = direction; }
    void SetSunElevationDegrees(float degrees);

    bool IsReady() const { return m_resourcesReady; }

private:
    bool CreateDeviceResources();
    bool RebuildMesh();
    void UpdateConstantBuffer(ID3D11DeviceContext* context, const DirectX::XMMATRIX& worldViewProj, float timeSeconds);

private:
    // Cloud.hlsl 의 cbuffer CBCloud 와 메모리 배치가 같아야 한다 (128바이트).
    struct CloudConstants
    {
        DirectX::XMFLOAT4X4 worldViewProj;
        DirectX::XMFLOAT4   colorAndCoverage;
        DirectX::XMFLOAT4   sunDirAndBrightness;
        DirectX::XMFLOAT4   warpParams;
        DirectX::XMFLOAT4   windAndRim;
    };

    static_assert(sizeof(CloudConstants) == 128, "CloudConstants 크기가 Cloud.hlsl 의 CBCloud 와 어긋난다");

    float m_radius = 900.0f;
    int   m_latitudeSegments = 24;
    int   m_longitudeSegments = 48;
    bool  m_meshDirty = true;

    size_t m_indexCount = 0;

    float m_coverage = 0.55f;
    float m_softness = 0.08f;
    float m_noiseScale = 2.2f;
    float m_warpStrength = 1.6f;
    float m_windDirectionDegrees = 35.0f;
    float m_windSpeed = 0.05f;
    float m_rimPower = 4.0f;

    DirectX::XMFLOAT3 m_sunDirection{ 0.0f, 1.0f, 0.0f };
    float m_dayFactor = 1.0f; // 0=밤, 1=낮 -- SetSunElevationDegrees 가 계산해 채운다

    DirectX::XMFLOAT3 m_cloudColor{ 0.92f, 0.94f, 0.98f }; // 낮 구름 기본색

    ComPtr<ID3D11VertexShader>      m_vertexShader;
    ComPtr<ID3D11PixelShader>       m_pixelShader;
    ComPtr<ID3D11InputLayout>       m_inputLayout;
    ComPtr<ID3D11Buffer>            m_vertexBuffer;
    ComPtr<ID3D11Buffer>            m_indexBuffer;
    ComPtr<ID3D11Buffer>            m_constantBuffer;
    ComPtr<ID3D11RasterizerState>   m_rasterizerState;
    ComPtr<ID3D11DepthStencilState> m_depthState;
    ComPtr<ID3D11BlendState>        m_blendState; // 8번엔 없던 것 -- 이 프로젝트 최초의 알파 블렌딩

    bool m_resourcesReady = false;
    bool m_resourceCreationFailed = false;
};
