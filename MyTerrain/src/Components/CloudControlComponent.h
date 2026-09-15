#pragma once
#include "../GameObject/Component.h"

class CloudRenderer;
class SkyRenderer;
class UIText;

// 9번 기법의 조작 담당 -- 구름의 밀도/워프 세기/바람 속도를 키로 조절하고, 현재
// 상태를 HUD 로 보여준다. 매 프레임 SkyRenderer(8번)의 태양 방향/고도를 읽어다
// CloudRenderer 에 그대로 넘겨서 하늘과 구름이 같은 시간대를 공유하게 만드는 것도
// 이 컴포넌트의 역할이다 -- SkyRenderer 는 값을 읽기만 할 뿐 전혀 건드리지 않는다.
//
// 조작은 전부 화면 버튼으로 한다: 구름 표시 켬/끄기 / 커버리지 임계값 조절(낮을수록
// 구름이 많아진다) / 도메인 워핑 세기 조절 / 바람 속도 조절 / 기본값 복귀(다른
// 컴포넌트들의 기본값 복귀 버튼과는 별개).
class CloudControlComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime) override;

    void SetTarget(CloudRenderer* cloud) { m_cloud = cloud; }
    void SetSky(SkyRenderer* sky) { m_sky = sky; }
    void SetInfoText(UIText* infoText) { m_infoText = infoText; }

    // ---- 버튼용 동작 (예전에는 각각 L / G / H / V / B / O / P / 0 키였다) ----
    void ToggleVisible();
    void IncreaseCoverage();       // 임계값을 높여 구름을 줄인다 (예전 G)
    void DecreaseCoverage();       // 임계값을 낮춰 구름을 늘린다 (예전 H)
    void DecreaseWarpStrength();
    void IncreaseWarpStrength();
    void DecreaseWindSpeed();
    void IncreaseWindSpeed();
    void ResetToDefault();

private:
    void RefreshInfoText();

private:
    CloudRenderer* m_cloud = nullptr;
    SkyRenderer* m_sky = nullptr;
    UIText* m_infoText = nullptr;

    bool m_visible = true;

    float m_refreshTimer = 0.0f;
    static constexpr float kRefreshInterval = 0.1f;
};
