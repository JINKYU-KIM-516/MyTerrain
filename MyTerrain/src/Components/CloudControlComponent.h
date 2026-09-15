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
// FreeCameraController(W/A/S/D/E/Q/Shift/R/마우스/휠), HeightMapControlComponent
// (Up/Down/Left/Right/N/C/F5), GridControlComponent(+/-/[/]/Tab),
// SkyControlComponent(, . T Y I K U J) 가 이미 쓰는 키를 전부 피해서 골랐다
// (Technique09_Clouds.cpp 가 이 컴포넌트들을 모두 함께 얹는다).
//
//   L        구름 표시 켬/끄기
//   G / H    커버리지 임계값 낮추기 / 높이기 (낮을수록 구름이 많아진다)
//   V / B    도메인 워핑 세기 줄이기 / 늘리기
//   O / P    바람 속도 느리게 / 빠르게
//   0        기본값 복귀 (다른 컴포넌트들의 0 키 리셋과 함께 눌린다)
class CloudControlComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime) override;

    void SetTarget(CloudRenderer* cloud) { m_cloud = cloud; }
    void SetSky(SkyRenderer* sky) { m_sky = sky; }
    void SetInfoText(UIText* infoText) { m_infoText = infoText; }

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
