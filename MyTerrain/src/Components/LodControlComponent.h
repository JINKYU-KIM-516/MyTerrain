#pragma once
#include "../GameObject/Component.h"
#include <string>

class TerrainRenderer;
class UIText;

// 6-1 기법(거리 기반 LOD 지형1)의 조작 담당.
//
// 지형 자체(펄린 노이즈 파라미터, 그리드 분할/셀 크기 등)는 PerlinControlComponent 가
// 그대로 맡는다. 이 컴포넌트는 그 위에 "청크마다 카메라까지의 거리로 해상도를 고른다"는
// 부분만 얹는다 -- QuadtreeControlComponent 와 같은 이유로 독립 컴포넌트로 둔다.
//
//   L        LOD 켬/끔 (끄면 모든 청크를 레벨 0 으로 그린다. Draw 호출 수는 그대로라
//            삼각형 수의 차이만 순수하게 비교할 수 있다)
//   K        레벨 색상 표시 켬/끔 (초록 -> 노랑 -> 주황 -> 빨강 -> 보라)
//   F        LOD 프리즈 : 지금 카메라 위치로 레벨을 고정한다. 컬링은 계속 따라가므로
//            레벨 경계까지 날아가서 이음매(crack)를 코앞에서 관찰할 수 있다
//   J        이웃 청크와의 레벨 차이를 1 이하로 제한 (이음매 완화 -- 없애지는 못한다)
//   C        절두체 컬링 켬/끔
//   B        청크 경계 박스 표시 켬/끔
//   , / .    청크 크기 줄이기 / 늘리기
//   ; / '    기준 거리(레벨 0 이 유지되는 거리) 줄이기 / 늘리기
//   U / I    LOD 레벨 수 줄이기 / 늘리기
//   0        기본값으로 복귀
class LodControlComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime) override;

    void SetTarget(TerrainRenderer* terrain) { m_terrain = terrain; }
    void SetInfoText(UIText* infoText) { m_infoText = infoText; }

private:
    void ApplyAll();
    void RefreshInfoText();

    static std::wstring FormatThousands(size_t value);

private:
    TerrainRenderer* m_terrain = nullptr;
    UIText* m_infoText = nullptr;

    // 청크 한 변의 셀 수. 256 분할 기준으로 딱 나눠떨어지는 2의 거듭제곱만 쓴다
    // (스텝으로 나눠떨어져야 레벨을 올려도 사각형이 고르게 유지된다).
    static constexpr int kChunkSizeSteps[] = { 4, 8, 16, 32, 64 };
    static constexpr int kChunkSizeStepCount = sizeof(kChunkSizeSteps) / sizeof(kChunkSizeSteps[0]);
    static constexpr int kDefaultChunkSizeIndex = 2;   // 16

    // 기본값 60 : 256x256(셀 1) 지형을 기본 카메라 위치에서 봤을 때 네 레벨이 모두
    // 화면에 나타나도록 고른 값이다 (레벨 경계가 60 / 120 / 240 이 된다).
    static constexpr float kDefaultBaseDistance = 60.0f;
    static constexpr float kMinBaseDistance = 20.0f;
    static constexpr float kMaxBaseDistance = 2000.0f;
    static constexpr float kBaseDistanceFactor = 1.25f;

    static constexpr int kDefaultLevelCount = 4;
    static constexpr int kMinLevelCount = 1;
    static constexpr int kMaxLevelCount = 5;   // TerrainLOD::kMaxLevels 와 같아야 한다

    int   m_chunkSizeIndex = kDefaultChunkSizeIndex;
    int   m_levelCount = kDefaultLevelCount;
    float m_baseDistance = kDefaultBaseDistance;

    bool m_lodEnabled = true;
    bool m_colorMode = true;          // 기법에 들어오자마자 레벨이 눈에 보이도록 켜둔다
    bool m_frozen = false;
    bool m_neighborClamp = false;
    bool m_cullingEnabled = true;
    bool m_debugBoxesEnabled = false; // 청크가 수백 개라 기본은 꺼둔다

    float m_refreshTimer = 0.0f;
    static constexpr float kRefreshInterval = 0.2f;
};
