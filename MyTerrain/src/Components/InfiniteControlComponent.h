#pragma once
#include "../GameObject/Component.h"
#include "../Terrain/GridMesh.h"
#include "../Terrain/PerlinNoise.h"
#include <string>

class InfiniteTerrainRenderer;
class UIText;

// 10번 기법(무한 지형 청크)의 조작 담당.
//
// 이 기법은 화면만 봐서는 1~2번 펄린 지형과 구분이 안 된다 -- 실제로 일어나는 일
// (청크가 언제 만들어지고 언제 사라지는지, 그 비용이 얼마인지)은 전부 숫자와 색으로만
// 드러난다. 그래서 이 컴포넌트가 하는 일의 절반은 HUD 를 채우는 것이다.
//
// 높이 함수는 여기서 소유한다. 2번의 PerlinControlComponent 와 같은 방식이지만,
// 넘겨주는 좌표가 "청크 로컬"이 아니라 "월드"라는 점이 다르다(그래서 청크 경계가
// 저절로 이어진다). 시드를 바꾸면 지형의 정의 자체가 달라지므로 올라와 있는 청크를
// 전부 버리고 다시 만들어야 한다 -- 그 장면이 스트리밍을 가장 잘 보여준다.
//
//   Tab : 표시 모드 전환 (솔리드 / 와이어프레임 / 둘 다)
//   그 외 조작은 전부 화면 오른쪽 아래 버튼
class InfiniteControlComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime) override;

    void SetTarget(InfiniteTerrainRenderer* terrain) { m_terrain = terrain; }
    void SetInfoText(UIText* infoText) { m_infoText = infoText; }

    // 씬에서 초기값을 지정한다. 이 값이 "기본값 복귀"로 돌아올 기본값이 된다.
    void SetParams(const Noise::Params& params);
    const Noise::Params& GetParams() const { return m_params; }

    // InfiniteTerrainRenderer 에 넘길 높이 함수 (Start 에서 자동으로 연결된다).
    // 월드 좌표를 그대로 받는다.
    GridMesh::HeightFunc MakeHeightFunction();

    // ---- 버튼용 동작 ----
    void IncreaseRadius();
    void DecreaseRadius();
    void IncreaseDivisions();     // 청크 크기는 그대로 두고 촘촘하게
    void DecreaseDivisions();     // 청크 크기는 그대로 두고 성기게
    void IncreaseBudget();        // 프레임당 생성 상한 +1
    void DecreaseBudget();
    void ToggleStreaming();       // 스트리밍 일시정지 (로드 경계 관찰용)
    void ToggleFrustumCulling();
    void ToggleChunkColor();
    void ToggleHighlight();       // 새 청크 강조
    void ToggleFog();
    void CycleDisplayMode();
    void JumpFar();               // 멀리 순간이동 -- 생성 큐가 차는 것을 보기 위한 버튼
    void RandomizeSeed();
    void ResetToDefault();

private:
    void ApplyResolution();
    void RefreshInfoText();

    static std::wstring FormatThousands(size_t value);

private:
    InfiniteTerrainRenderer* m_terrain = nullptr;
    UIText* m_infoText = nullptr;

    Noise::Perlin m_perlin;
    Noise::Params m_params;
    Noise::Params m_defaultParams;

    // 청크 한 변의 월드 크기는 고정하고 분할 수만 바꾼다.
    // 그래야 "같은 크기의 땅을 얼마나 촘촘하게 굽는가" 만 비교된다
    // (분할 수를 바꿀 때 청크 크기까지 같이 변하면 유지 반경의 의미도 달라져버린다).
    static constexpr float kChunkWorldSize = 128.0f;

    static constexpr int kDefaultDivisions = 128;
    static constexpr int kMinDivisions = 32;
    static constexpr int kMaxDivisions = 256;

    static constexpr int kDefaultRadius = 2;
    static constexpr int kDefaultBudget = 2;

    int m_divisions = kDefaultDivisions;

    float m_refreshTimer = 0.0f;
    static constexpr float kRefreshInterval = 0.2f;
};
