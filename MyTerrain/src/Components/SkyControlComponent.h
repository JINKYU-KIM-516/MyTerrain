#pragma once
#include "../GameObject/Component.h"

class SkyRenderer;
class UIText;

// 8번 기법의 조작 담당 -- 스카이돔의 시간대(태양 위치/하늘색)와 태양 모양을 키로
// 조절하고 현재 상태를 HUD 로 보여준다.
//
// HeightMapControlComponent(3번)가 Up/Down/Left/Right/N/C/F5/0 을 쓰고
// GridControlComponent 가 +/-/[/]/Tab 을 쓰므로, 여기서는 그 컴포넌트들이 전혀
// 쓰지 않는 키만 골랐다 (Technique08_Sky.cpp 가 두 컴포넌트를 함께 얹는다).
//
//   , / .        시간 되감기 / 감기 (누르고 있으면 연속)
//   T            자동 재생 켬/끔 (켜면 시간이 실시간으로 흐른다)
//   Y            프리셋 순환 (자정 -> 일출 -> 정오 -> 일몰)
//   I / K        태양 각크기 줄이기 / 늘리기
//   U / J        태양 글로우 약하게(좁게) / 강하게(넓게)
//   0            기본값(정오, 자동 재생 끔) 복귀
class SkyControlComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime) override;

    void SetTarget(SkyRenderer* sky) { m_sky = sky; }
    void SetInfoText(UIText* infoText) { m_infoText = infoText; }

private:
    void RefreshInfoText();

private:
    SkyRenderer* m_sky = nullptr;
    UIText* m_infoText = nullptr;

    bool m_autoPlay = false;
    int  m_presetIndex = 2;   // 0=자정, 1=일출, 2=정오, 3=일몰 (Y 로 순환. 초기값은 정오)

    // ---- , / . 리핏 상태 (GridControlComponent::ReadAdjustDirection 과 같은 패턴) ----
    int   m_repeatDirection = 0;
    float m_repeatTimer = 0.0f;
    bool  m_repeatStarted = false;
    static constexpr float kRepeatDelay = 0.35f;
    static constexpr float kRepeatInterval = 0.06f;

    static constexpr float kScrubStep = 0.01f;       // ,/. 한 번에 움직이는 시간 (하루의 1%)
    static constexpr float kAutoPlaySpeed = 0.02f;   // 자동 재생 속도 (초당 하루의 2% = 50초에 하루)

    float m_refreshTimer = 0.0f;
    static constexpr float kRefreshInterval = 0.1f;
};
