#pragma once
#include "../GameObject/Component.h"
#include <string>

class TerrainRenderer;
class UIText;

// 5번 기법(쿼드트리 컬링)의 조작 담당.
//
// 지형 자체(펄린 노이즈 파라미터, 그리드 분할/셀 크기 등)는 PerlinControlComponent 가
// 그대로 맡는다(5번 씬도 2번처럼 펄린 지형 위에서 시작한다). 이 컴포넌트는 그 위에
// "쿼드트리로 절두체 밖 리프를 건너뛰는" 부분만 별도로 얹는다 -- SplatControlComponent 와
// 같은 이유로 GridControlComponent 를 상속하지 않고 독립 컴포넌트로 둔다.
//
//   C        컬링 켬/끔 (끄면 지금까지처럼 지형 전체를 한 번의 Draw 호출로 그린다)
//   B        디버그 박스(보이는 리프의 AABB) 표시 켬/끔
//   , / .    리프 크기 줄이기 / 늘리기 (줄일수록 리프가 잘게 쪼개져 컬링이 세밀해지지만
//            Draw 호출 수가 늘어난다 -- 트레이드오프를 HUD 로 바로 보여준다)
//   0(넘패드 포함)  기본값으로 복귀
class QuadtreeControlComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime) override;

    void SetTarget(TerrainRenderer* terrain) { m_terrain = terrain; }
    void SetInfoText(UIText* infoText) { m_infoText = infoText; }

private:
    void ApplyLeafSize();
    void RefreshInfoText();

private:
    TerrainRenderer* m_terrain = nullptr;
    UIText* m_infoText = nullptr;

    // 리프 한 변의 셀 수를 이 목록 안에서만 오간다 (256 분할 기준으로 딱 맞아떨어지는 값들).
    static constexpr int kLeafSizeSteps[] = { 2, 4, 8, 16, 32, 64, 128 };
    static constexpr int kLeafSizeStepCount = sizeof(kLeafSizeSteps) / sizeof(kLeafSizeSteps[0]);
    static constexpr int kDefaultLeafSizeIndex = 3; // 16

    int  m_leafSizeIndex = kDefaultLeafSizeIndex;
    bool m_cullingEnabled = true;
    bool m_debugBoxesEnabled = true;

    float m_refreshTimer = 0.0f;
    static constexpr float kRefreshInterval = 0.2f;
};
