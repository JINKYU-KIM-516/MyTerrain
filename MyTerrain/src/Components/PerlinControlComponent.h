#pragma once
#include "GridControlComponent.h"
#include "../Terrain/GridMesh.h"
#include "../Terrain/PerlinNoise.h"

// 2번 기법(펄린 노이즈 지형)의 조작 담당.
//
// GridControlComponent 를 상속하므로 분할 수(+/-), 셀 크기([ / ]), 표시 모드(Tab) 는
// 그대로 쓰고, 여기에 노이즈 파라미터 조절과 HUD 표시를 얹는다.
//
//   위 / 아래        조절할 파라미터 선택
//   왼쪽 / 오른쪽    선택한 파라미터 값 조절 (누르고 있으면 연속)
//   N                합성 방식 전환 (fBm -> Ridged -> Billow)
//   M                시드 무작위
//   0                기법이 정해둔 기본값으로 복귀
//
// 파라미터가 바뀌면 그 자리에서 메시를 다시 만들고(TerrainRenderer::RequestRebuild),
// 재생성에 걸린 시간과 실제로 나온 높이 범위를 HUD 에 보여준다.
// "옥타브를 하나 올리면 얼마나 비싸지는가" 를 눈으로 보게 하는 것이 목적이다.
class PerlinControlComponent : public GridControlComponent
{
public:
    // HUD 에 나열되는 순서와 같다
    enum class ParamId
    {
        Scale = 0,
        Octaves,
        Lacunarity,
        Persistence,
        Amplitude,
        Seed,
        Type,
        Count
    };

    void Start() override;
    void Update(float deltaTime) override;

    // 씬에서 초기값을 지정한다. 이 값이 0 키로 돌아올 기본값이 된다.
    void SetParams(const Noise::Params& params);
    const Noise::Params& GetParams() const { return m_params; }

    // TerrainRenderer 에 넘길 높이 함수 (Start 에서 자동으로 연결된다)
    GridMesh::HeightFunc MakeHeightFunction();

protected:
    void RefreshInfoText() override;

private:
    float SampleHeight(float worldX, float worldZ);

    // 좌/우 키를 읽어 -1 / 0 / +1 을 돌려준다. 누르고 있으면 일정 간격으로 반복된다.
    int  ReadAdjustDirection(float deltaTime);
    void AdjustSelected(int direction);

    // 시드 반영 + 메시 재생성 요청
    void ApplyParamChange();

private:
    Noise::Perlin m_perlin;
    Noise::Params m_params;
    Noise::Params m_defaultParams;

    ParamId      m_selected = ParamId::Scale;
    unsigned int m_appliedSeed = 0;

    // ---- 좌/우 키 리핏 ----
    int   m_repeatDirection = 0;
    float m_repeatTimer = 0.0f;
    bool  m_repeatStarted = false;
    static constexpr float kRepeatDelay = 0.35f;      // 처음 반복까지 기다리는 시간
    static constexpr float kRepeatInterval = 0.09f;   // 그 뒤 반복 간격

    // ---- 높이 범위 통계 ----
    // 생성 중(=Render 안에서 높이 함수가 불릴 때)에는 sample 쪽에 쌓고,
    // 생성이 끝난 다음 프레임에 last 쪽으로 옮겨서 표시한다. (표시가 깜빡이지 않도록)
    float m_sampleMinHeight = 0.0f;
    float m_sampleMaxHeight = 0.0f;
    bool  m_sampleValid = false;

    float m_lastMinHeight = 0.0f;
    float m_lastMaxHeight = 0.0f;
    bool  m_lastValid = false;
};
