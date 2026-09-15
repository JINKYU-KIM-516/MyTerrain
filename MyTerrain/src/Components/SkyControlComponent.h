#pragma once
#include "../GameObject/Component.h"

class SkyRenderer;
class UIText;

// 8번 기법의 조작 담당 -- 스카이돔의 시간대(태양 위치/하늘색)와 태양 모양을 키로
// 조절하고 현재 상태를 HUD 로 보여준다.
//
// 조작은 전부 화면 버튼으로 한다: 시간 되감기/감기(누르고 있으면 연속) / 자동 재생
// 켬/끔(켜면 시간이 실시간으로 흐른다) / 프리셋 순환(자정 -> 일출 -> 정오 -> 일몰) /
// 태양 각크기 조절 / 태양 글로우 조절 / 기본값(정오, 자동 재생 끔) 복귀.
class SkyControlComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime) override;

    void SetTarget(SkyRenderer* sky) { m_sky = sky; }
    void SetInfoText(UIText* infoText) { m_infoText = infoText; }

    // ---- 버튼용 동작 (예전에는 각각 ,/. / T / Y / I / K / U / J / 0 키였다) ----
    void StepTimeBackward();   // 버튼도 SetRepeatWhileHeld(true) 로 등록해서 누르고 있으면 연속으로 감긴다
    void StepTimeForward();
    void ToggleAutoPlay();
    void CyclePreset();
    void DecreaseSunSize();
    void IncreaseSunSize();
    void IncreaseGlowExponent();   // 글로우가 좁고 진해진다 ("약하게")
    void DecreaseGlowExponent();   // 글로우가 넓고 은은해진다 ("강하게")
    void ResetToDefault();

private:
    void RefreshInfoText();

private:
    SkyRenderer* m_sky = nullptr;
    UIText* m_infoText = nullptr;

    bool m_autoPlay = false;
    int  m_presetIndex = 2;   // 0=자정, 1=일출, 2=정오, 3=일몰 (프리셋 순환 버튼. 초기값은 정오)

    static constexpr float kScrubStep = 0.01f;       // 시간 스크럽 버튼 한 번에 움직이는 시간 (하루의 1%)
    static constexpr float kAutoPlaySpeed = 0.02f;   // 자동 재생 속도 (초당 하루의 2% = 50초에 하루)

    float m_refreshTimer = 0.0f;
    static constexpr float kRefreshInterval = 0.1f;
};
