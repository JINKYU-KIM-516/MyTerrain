#pragma once
#include "../GameObject/Component.h"
#include <string>

class TerrainRenderer;
class UIText;

// 6-2 기법(고급 거리 LOD - 스티칭 & 지오머핑)의 조작 담당.
//
// 6-1 이 남겨둔 두 문제를 각각 켜고 끄면서 비교하는 것이 이 화면의 목적이다.
//   - 이음매(crack)  -> H (스티칭)
//   - 팝핑(popping)  -> G (지오머핑)
//
//   H        스티칭 켬/끔 (끄면 6-1 처럼 레벨 경계에 틈이 보인다)
//   G        지오머핑 켬/끔 (끄면 레벨이 바뀌는 순간 지형이 툭 튄다)
//   V        morph 계수 시각화 (파랑 = 아직 안 움직임, 빨강 = 다음 레벨과 같아짐)
//   O / P    morph 구간 폭 줄이기 / 늘리기 (0 ~ 0.5)
//   K        레벨 색상 표시 켬/끔
//   F        LOD 프리즈 (지금 카메라 위치로 레벨 고정 -- 경계까지 날아가서 관찰용)
//   C        절두체 컬링 켬/끔
//   B        청크 경계 박스 표시 켬/끔
//   , / .    청크 크기 줄이기 / 늘리기
//   ; / '    기준 거리 줄이기 / 늘리기
//   U / I    LOD 레벨 수 줄이기 / 늘리기
//   0        기본값으로 복귀
//
// 이웃 레벨 차이 1 제한은 스티칭의 전제라 여기서는 끌 수 없다 (렌더러가 강제한다).
class GeoLodControlComponent : public Component
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

    static constexpr int kChunkSizeSteps[] = { 4, 8, 16, 32, 64 };
    static constexpr int kChunkSizeStepCount = sizeof(kChunkSizeSteps) / sizeof(kChunkSizeSteps[0]);
    static constexpr int kDefaultChunkSizeIndex = 2;   // 16

    // 6-1 은 60 이지만 6-2 는 90 에서 시작한다. 지오머핑이 팝핑을 제대로 흡수하려면
    // 기준 거리가 청크 대각선보다 넉넉히 커야 하기 때문이다 (HUD 에 권장값을 함께 띄운다).
    static constexpr float kDefaultBaseDistance = 90.0f;
    static constexpr float kMinBaseDistance = 20.0f;
    static constexpr float kMaxBaseDistance = 2000.0f;
    static constexpr float kBaseDistanceFactor = 1.25f;

    static constexpr int kDefaultLevelCount = 4;
    static constexpr int kMinLevelCount = 1;
    static constexpr int kMaxLevelCount = 5;

    // morph 구간 폭. 0.5 를 넘기면 거친 청크가 세밀한 이웃과 붙어 있는 동안에도
    // 움직이기 시작해서 경계에 실오라기 같은 틈이 생기므로 렌더러가 0.5 로 자른다.
    static constexpr float kDefaultMorphWidth = 0.25f;
    static constexpr float kMorphWidthStep = 0.05f;

    int   m_chunkSizeIndex = kDefaultChunkSizeIndex;
    int   m_levelCount = kDefaultLevelCount;
    float m_baseDistance = kDefaultBaseDistance;
    float m_morphWidth = kDefaultMorphWidth;

    bool m_stitchEnabled = true;
    bool m_morphEnabled = true;
    bool m_morphColorMode = false;
    bool m_colorMode = false;          // 6-2 는 "이음매가 안 보인다" 를 보는 화면이라 기본은 끔
    bool m_frozen = false;
    bool m_cullingEnabled = true;
    bool m_debugBoxesEnabled = false;

    float m_refreshTimer = 0.0f;
    static constexpr float kRefreshInterval = 0.2f;
};
