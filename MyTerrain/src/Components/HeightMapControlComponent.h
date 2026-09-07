#pragma once
#include "GridControlComponent.h"
#include "../Terrain/GridMesh.h"
#include "../Terrain/HeightMap.h"
#include "../Terrain/HeightMapTexture.h"

#include <string>
#include <vector>

// 3번 기법(높이맵 지형)의 조작 담당.
//
// GridControlComponent 를 상속하므로 분할 수(+/-), 셀 크기([ / ]), 표시 모드(Tab) 는
// 그대로 쓰고, 여기에 높이맵 파일 교체와 펼침 파라미터 조절을 얹는다.
// 2번 PerlinControlComponent 와 뼈대가 같다 -- 다른 것은 "높이를 어디서 가져오는가" 뿐이다.
//
//   위 / 아래        조절할 항목 선택
//   왼쪽 / 오른쪽    선택한 항목 값 조절 (누르고 있으면 연속)
//   N                다음 높이맵 파일
//   C                고도 색상 모드 (GPU 텍스처를 읽어 고도별로 색칠)
//   F5               폴더 다시 훑고 현재 파일 다시 읽기 (편집 프로그램에서 저장한 뒤 확인용)
//   0                기본값 복귀
//
// HUD 에는 이미지의 해상도·비트 심도·계조 단계 수와, 텍셀 간격이 셀 크기와 견주어
// 어느 쪽이 촘촘한지를 함께 띄운다. 높이맵 지형에서 결과를 좌우하는 것이 결국
// "이미지 해상도 대 정점 밀도" 이기 때문이다.
class HeightMapControlComponent : public GridControlComponent
{
public:
    // HUD 에 나열되는 순서와 같다
    enum class ParamId
    {
        File = 0,
        HeightScale,
        HeightOffset,
        WorldSize,
        Wrap,
        FlipZ,
        Count
    };

    void Start() override;
    void Update(float deltaTime) override;
    void Destroy() override;

    // 씬에서 초기값을 지정한다. 이 값이 0 키로 돌아올 기본값이 된다.
    void SetParams(const HeightMap::Params& params);
    const HeightMap::Params& GetParams() const { return m_params; }

    // TerrainRenderer 에 넘길 높이 함수 (Start 에서 자동으로 연결된다)
    GridMesh::HeightFunc MakeHeightFunction();

protected:
    void RefreshInfoText() override;

private:
    float SampleHeight(float worldX, float worldZ);

    void AdjustSelected(int direction);

    // 파라미터 변경을 지형/텍스처에 반영하고 메시 재생성을 요청한다
    void ApplyParamChange();

    void RescanFiles();
    void LoadFileAt(int index);
    void CycleFile(int direction);

    // 높이맵을 GPU 텍스처로 올린다 (디바이스가 아직 없으면 다음 프레임에 다시 시도)
    void UploadTexture();

private:
    HeightMap::Image   m_image;
    HeightMapTexture   m_texture;

    HeightMap::Params  m_params;
    HeightMap::Params  m_defaultParams;

    std::vector<std::wstring> m_files;
    int m_fileIndex = -1;

    std::wstring m_loadError;      // 비어 있지 않으면 HUD 에 빨간 줄 대신 문구로 표시
    bool m_texturePending = false; // 올려야 할 텍스처가 남아있는지

    ParamId m_selected = ParamId::File;

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
