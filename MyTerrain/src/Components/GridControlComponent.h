#pragma once
#include "../GameObject/Component.h"
#include <string>

class TerrainRenderer;
class UIText;

// 지형(그리드) 파라미터를 키 입력으로 조절하고,
// 현재 상태를 화면 좌상단 텍스트로 보여주는 컴포넌트.
//
//   + / -  (또는 넘패드 +, -)   가로/세로 분할 수를 2배 / 절반으로
//   [ / ]                        셀 크기를 절반 / 2배로
//   Tab                          표시 모드 전환 (솔리드+와이어 / 와이어 / 솔리드)
//
// 분할 수를 2배씩 바꾸는 이유는, 이후 LOD·쿼드트리 기법에서 다루기 좋은
// 2의 거듭제곱 크기를 유지하기 위해서다.
//
// 기법마다 HUD 에 보여줄 내용이 다르므로 RefreshInfoText 는 virtual 로 열어두었다.
// (2번 펄린 노이즈 지형의 PerlinControlComponent 가 이 클래스를 상속해서 쓴다)
class GridControlComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime) override;

    void SetTarget(TerrainRenderer* terrain) { m_terrain = terrain; }
    void SetInfoText(UIText* infoText) { m_infoText = infoText; }

    void SetDivisionRange(int minDivisions, int maxDivisions);
    void SetCellSizeRange(float minCellSize, float maxCellSize);

protected:
    virtual void RefreshInfoText();

    // 파생 클래스에서도 쓰는 숫자 -> 문자열 헬퍼
    static std::wstring FormatThousands(size_t value);   // 1234567 -> "1,234,567"
    static std::wstring FormatFixed(float value, int decimals);

protected:
    TerrainRenderer* m_terrain = nullptr;
    UIText* m_infoText = nullptr;

    int   m_minDivisions = 2;
    int   m_maxDivisions = 512;    // 512 x 512 = 약 26만 정점 / 52만 삼각형
    float m_minCellSize = 0.125f;
    float m_maxCellSize = 16.0f;

    // FPS 표시가 너무 자주 깜빡이지 않도록 일정 주기로만 갱신한다
    float m_refreshTimer = 0.0f;
    static constexpr float kRefreshInterval = 0.25f;
};
