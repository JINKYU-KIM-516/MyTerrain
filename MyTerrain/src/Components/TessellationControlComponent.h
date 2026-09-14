#pragma once
#include "../GameObject/Component.h"
#include <string>

class TerrainRenderer;
class HeightMapControlComponent;
class UIText;

// 7번 기법(하드웨어 테셀레이션)의 조작 담당.
//
// 같은 씬에 HeightMapControlComponent 가 이미 떠 있으므로(Up/Down/Left/Right/N/C/F5/
// +/-/[/]/Tab 을 그 컴포넌트가 쓴다) 여기서는 그 키들을 피해서 새 키를 쓴다.
// heightScale/heightOffset 은 HeightMapControlComponent 가 조절하는 값이라, 매 프레임
// 그 값을 읽어 TerrainRenderer 에 그대로 전달한다(도메인 셰이더가 displacement 할 때
// 0~1 값을 월드 높이로 바꾸는 데 그 값이 필요하다).
//
//   H        디스플레이스먼트 켬/끔 (끄면 컨트롤 포인트 4개를 쌍선형 보간만 한 매끈한 패치)
//   J        파티션 모드 전환 (integer <-> fractional_odd)
//   V        팩터 시각화 (파랑 = 최소, 빨강 = 최대)
//   F        프리즈 (지금 위치를 팩터 계산 기준점으로 고정 -- 패치 경계까지 날아가서 관찰용)
//   X        절두체 컬링 켬/끔
//   B        패치 경계 박스 표시 켬/끔
//   O / P    최대 팩터 줄이기 / 늘리기 (1 ~ 64)
//   ; / '    기준 거리 줄이기 / 늘리기
//   0        기본값으로 복귀 (이 컴포넌트가 다루는 값만)
class TessellationControlComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime) override;

    void SetTarget(TerrainRenderer* terrain) { m_terrain = terrain; }
    void SetHeightMapControl(HeightMapControlComponent* control) { m_heightMapControl = control; }
    void SetInfoText(UIText* infoText) { m_infoText = infoText; }

private:
    void ApplyAll();
    void SyncHeightMapScale();
    void RefreshInfoText();

    static std::wstring FormatThousands(size_t value);

private:
    TerrainRenderer* m_terrain = nullptr;
    HeightMapControlComponent* m_heightMapControl = nullptr;
    UIText* m_infoText = nullptr;

    static constexpr float kDefaultBaseDistance = 60.0f;
    static constexpr float kMinBaseDistance = 10.0f;
    static constexpr float kMaxBaseDistance = 2000.0f;
    static constexpr float kBaseDistanceFactor = 1.25f;

    static constexpr float kDefaultMinFactor = 1.0f;
    static constexpr float kDefaultMaxFactor = 32.0f;
    static constexpr float kFactorStep = 2.0f;

    static constexpr float kDefaultNormalEpsilon = 1.0f;

    float m_baseDistance = kDefaultBaseDistance;
    float m_minFactor = kDefaultMinFactor;
    float m_maxFactor = kDefaultMaxFactor;

    bool m_displacementEnabled = true;
    bool m_fractionalPartitioning = true;
    bool m_factorColorMode = false;
    bool m_frustumCullingEnabled = true;
    bool m_debugBoxesEnabled = false;
    bool m_frozen = false;

    float m_refreshTimer = 0.0f;
    static constexpr float kRefreshInterval = 0.2f;
};
