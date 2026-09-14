#pragma once
#include "../GameObject/Component.h"
#include "SkyDome.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <algorithm>

// 8. 스카이맵 (SkyDome) 을 그리는 컴포넌트.
//
// TerrainRenderer 와 완전히 독립된 별도의 셰이더(Sky.hlsl) / 파이프라인 상태 /
// 상수버퍼(b0)를 쓴다 -- 이 컴포넌트가 씬에 있든 없든 1~7번 기법은 TerrainRenderer.h/.cpp,
// BasicTerrain.hlsl 를 단 한 줄도 건드리지 않으므로 전혀 영향받지 않는다.
//
//   [카메라 추종]  Render() 가 매 프레임 카메라 위치로 월드 행렬을 다시 만든다
//                  (Scale(반지름) * Translation(카메라 위치)). GameObject 의 Transform
//                  자체는 건드리지 않는다 -- 이 컴포넌트만 카메라를 그림자처럼 따라다닌다.
//   [깊이]         정점 셰이더가 클립 공간 z 를 w 로 덮어써 NDC 깊이가 항상 1.0(가장 먼
//                  값)이 되게 하고, 깊이 비교를 LESS_EQUAL 로 둬서 클리어된 깊이(1.0)와도
//                  같아 통과하게 한다. 깊이 쓰기는 꺼서 스카이가 다른 오브젝트를 가리지
//                  않는다.
//   [절차적 하늘]  텍스처 없이 픽셀 셰이더에서 지평선/천정 색 보간 + 태양 원반/글로우를
//                  계산한다 (Sky.hlsl 참고).
//   [시간대]       SetTimeOfDay(0~1) 로 자정 -> 일출 -> 정오 -> 일몰 -> 자정을 한 바퀴
//                  돈다. 하늘 색과 태양 위치/색이 미리 정해둔 키프레임 사이를 보간해서
//                  나온다 (SkyRenderer.cpp 의 kKeyframes).
class SkyRenderer : public Component
{
public:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    void Start() override;
    void Render() override;
    void Destroy() override;

    // 반구를 실제로 얼마나 크게 그릴지 (월드 단위). 카메라가 항상 중심에 있으므로
    // 어떤 값을 넣어도 시각적으로는 거의 같아 보인다 (깊이가 항상 1.0 으로 고정되기
    // 때문에 반지름이 정말 커도/작아도 결과가 달라지지 않는다) -- 근평면보다만 크면 된다.
    void  SetRadius(float radius) { m_radius = std::max(radius, 1.0f); }
    float GetRadius() const { return m_radius; }

    void SetDomeSegments(int latitudeSegments, int longitudeSegments);

    // ---------------- 시간대 ----------------
    // t = 0.0 자정, 0.25 부근 일출, 0.5 정오, 0.75 부근 일몰, 1.0 = 0.0 과 같음 (순환).
    // 범위를 벗어난 값도 자연스럽게 감아 받는다.
    void  SetTimeOfDay(float t01);
    float GetTimeOfDay() const { return m_timeOfDay; }

    // 태양 방향(카메라 기준, 하늘을 향한 단위벡터)과 고도(도). HUD 표시용.
    DirectX::XMFLOAT3 GetSunDirection() const { return m_sunDirection; }
    float GetSunElevationDegrees() const;

    // 태양 각반경(도). 커질수록 태양 원반이 커진다.
    void  SetSunAngularRadius(float degrees);
    float GetSunAngularRadiusDegrees() const { return m_sunAngularRadiusDegrees; }

    // 태양 주변 글로우(번짐) 지수. 작을수록 넓고 은은하게, 클수록 좁고 진하게 번진다.
    void  SetSunGlowExponent(float exponent);
    float GetSunGlowExponent() const { return m_sunGlowExponent; }

    bool IsReady() const { return m_resourcesReady; }

private:
    bool CreateDeviceResources();
    bool RebuildMesh();
    void UpdateConstantBuffer(ID3D11DeviceContext* context, const DirectX::XMMATRIX& worldViewProj);

    // 시간대 m_timeOfDay 로부터 태양 방향과 하늘/태양 색을 다시 계산해 캐시에 저장한다.
    void RecomputeFromTimeOfDay();

private:
    // Sky.hlsl 의 cbuffer CBSky 와 메모리 배치가 같아야 한다 (128바이트)
    struct SkyConstants
    {
        DirectX::XMFLOAT4X4 worldViewProj;
        DirectX::XMFLOAT4   horizonColor;
        DirectX::XMFLOAT4   zenithColor;
        DirectX::XMFLOAT4   sunDirectionAndSize;   // xyz = 태양 방향, w = 각반경 cos 임계값
        DirectX::XMFLOAT4   sunColorAndGlow;       // rgb = 태양 색, w = 글로우 지수
    };

    // Sky.hlsl 의 cbuffer CBSky 와 바이트 단위로 어긋나면 즉시 컴파일 에러로 잡아낸다
    // (64 + 16*4 = 128, 16의 배수).
    static_assert(sizeof(SkyConstants) == 128, "SkyConstants 크기가 Sky.hlsl 의 CBSky 와 어긋난다");

    float m_radius = 1000.0f;
    int   m_latitudeSegments = 24;
    int   m_longitudeSegments = 48;
    bool  m_meshDirty = true;

    size_t m_indexCount = 0;

    float m_timeOfDay = 0.5f;   // 기본값: 정오

    DirectX::XMFLOAT3 m_horizonColor{ 0.65f, 0.80f, 0.95f };
    DirectX::XMFLOAT3 m_zenithColor{ 0.25f, 0.45f, 0.85f };
    DirectX::XMFLOAT3 m_sunDirection{ 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 m_sunColor{ 1.0f, 0.95f, 0.85f };

    float m_sunAngularRadiusDegrees = 1.5f;
    float m_sunGlowExponent = 32.0f;

    ComPtr<ID3D11VertexShader>      m_vertexShader;
    ComPtr<ID3D11PixelShader>       m_pixelShader;
    ComPtr<ID3D11InputLayout>       m_inputLayout;
    ComPtr<ID3D11Buffer>            m_vertexBuffer;
    ComPtr<ID3D11Buffer>            m_indexBuffer;
    ComPtr<ID3D11Buffer>            m_constantBuffer;
    ComPtr<ID3D11RasterizerState>   m_rasterizerState;
    ComPtr<ID3D11DepthStencilState> m_depthState;

    bool m_resourcesReady = false;
    bool m_resourceCreationFailed = false;
};
