#pragma once
#include "../GameObject/Component.h"
#include "../Terrain/SplatTexture.h"
#include <string>

class TerrainRenderer;
class UIText;

// 4번 기법(텍스처 스플래팅)의 조작 담당.
//
// 높이/파일 조작은 3번과 완전히 같은 HeightMapControlComponent 가 그대로 맡는다
// (4번 씬도 3번처럼 높이맵 지형 위에서 시작한다). 이 컴포넌트는 "정점 높이·경사도로
// 텍스처 4장을 섞는" 부분만 별도로 얹는다 -- HeightMapControlComponent 의 ParamId
// 순환(↑/↓/←/→)에 새 항목을 끼워 넣지 않고 전용 키를 쓰는 이유는, 그 열거형을
// 건드리지 않고도(=3번 코드를 하나도 고치지 않고도) 독립적으로 얹을 수 있기 때문이다.
// HUD 도 별도의 UIText 를 받아 따로 표시한다.
//
//   V        스플래팅 켬/끔 (끄면 3번과 같은 고도 색상/체커로 돌아간다)
//   I / K    텍스처 타일링 배율 올리기 / 내리기
//   U / J    경사 임계값 구간을 넓히기 / 좁히기 (급경사일수록 바위로 전이되는 구간)
//   0(넘패드 포함)  스플래팅 파라미터를 기본값으로 (HeightMapControlComponent 의 0 키와는 별개)
class SplatControlComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime) override;
    void Destroy() override;

    void SetTarget(TerrainRenderer* terrain) { m_terrain = terrain; }
    void SetInfoText(UIText* infoText) { m_infoText = infoText; }

private:
    void ApplySplatParams();
    void RefreshInfoText();

private:
    TerrainRenderer* m_terrain = nullptr;
    UIText* m_infoText = nullptr;

    SplatTexture m_textures;
    std::wstring m_loadError;

    // 0 키로 되돌아올 기본값
    static constexpr float kDefaultTiling = 0.08f;
    static constexpr float kDefaultSlopeStart = 0.35f;
    static constexpr float kDefaultSlopeEnd = 0.65f;

    float m_tiling = kDefaultTiling;
    float m_slopeStart = kDefaultSlopeStart;
    float m_slopeEnd = kDefaultSlopeEnd;
    bool  m_splatMode = true;

    float m_refreshTimer = 0.0f;
    static constexpr float kRefreshInterval = 0.25f;
};
