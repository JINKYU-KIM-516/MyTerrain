#pragma once
#include <string>
#include <vector>
#include <cstdint>

// 이미지 파일에 그려둔 "높이맵(heightmap)" 을 읽어 지형 높이로 바꿔주는 모듈.
//
// 2번 펄린 노이즈는 높이를 실행 중에 계산했지만, 3번은 미리 그려둔 값을 읽어온다.
// GridMesh::Generate 에 넘길 높이 함수의 알맹이만 바뀌는 셈이다.
//
//   펄린 : height = Perlin::Evaluate(x, z, params)
//   높이맵 : height = HeightMap::Evaluate(image, x, z, params)
//
// 지원 형식
//   - WIC(Windows Imaging Component) 로 읽는 이미지 : PNG / BMP / TIFF / JPG
//     Windows 에 내장되어 있어 외부 라이브러리가 필요 없고, 16비트 그레이스케일 PNG 도 그대로 읽는다.
//   - 헤더가 없는 RAW : .raw / .r16 / .r8
//     World Machine, Gaea, Unity 가 주고받는 형식. 가로세로가 같은 정사각형이라고 보고
//     파일 크기로부터 해상도를 역산한다 (16비트 리틀엔디언 우선, 실패하면 8비트).
//
// 비트 심도에 대하여
//   8비트는 높이 단계가 256개뿐이라 계단 현상(terracing)이 생긴다. 높이 자체보다
//   법선이 문제인데, 계단의 윗면은 완전한 평면이고 옆면은 수직이라 솔리드 셰이딩에서
//   등고선 무늬로 드러난다. 지형용으로는 16비트(65536단계)를 쓰는 것이 맞다.
//   JPG 는 손실 압축이라 8x8 블록마다 미세한 높이 점프가 생기므로 쓰지 않는 것이 좋다.
namespace HeightMap
{
    // 높이맵 바깥 영역을 어떻게 채울지
    enum class WrapMode
    {
        Clamp = 0,   // 가장자리 값을 늘린다 (기본)
        Wrap,        // 타일링해서 반복 (이미지가 seamless 여야 이음매가 안 보인다)
        Mirror,      // 거울처럼 접어서 반복 (seamless 가 아니어도 이음매가 덜 보인다)
        Count
    };

    const wchar_t* ToName(WrapMode mode);

    // 높이맵 한 장. 값은 전부 0~1 로 정규화해서 들고 있는다.
    class Image
    {
    public:
        // 확장자를 보고 RAW / WIC 중 하나로 읽는다. 실패하면 false 와 함께 이유를 담는다.
        bool LoadFromFile(const std::wstring& path, std::wstring* outError = nullptr);
        void Clear();

        bool IsValid() const { return m_width > 0 && m_height > 0 && !m_samples.empty(); }

        int GetWidth()  const { return m_width; }
        int GetHeight() const { return m_height; }

        // 원본 파일의 채널당 비트 수 (8 또는 16). 계단 현상을 예측하는 근거가 된다.
        int GetSourceBits() const { return m_sourceBits; }

        // L"16비트 PNG" 처럼 사람이 읽을 형식 이름
        const std::wstring& GetSourceKind() const { return m_sourceKind; }
        const std::wstring& GetFileName()   const { return m_fileName; }

        // 실제로 들어있는 값의 범위(0~1). 1.0 에 한참 못 미치면 계조를 다 안 쓰고 있는 이미지다.
        float GetMinValue() const { return m_minValue; }
        float GetMaxValue() const { return m_maxValue; }

        // 서로 다른 높이 값이 몇 종류나 들어있는지. 8비트 이미지는 아무리 커도 256 을 넘지 못한다.
        int GetDistinctLevels() const { return m_distinctLevels; }

        // 정수 텍셀 하나를 읽는다 (범위를 벗어난 좌표는 wrap 규칙으로 접는다)
        float SampleTexel(int x, int y, WrapMode wrap) const;

        // u, v = 0~1 (텍셀 중심 규약). 텍셀 경계에서 기울기가 꺾이는 C0 연속이다.
        float SampleBilinear(float u, float v, WrapMode wrap) const;

    private:
        bool LoadRaw(const std::wstring& path, std::wstring* outError);
        bool LoadWic(const std::wstring& path, std::wstring* outError);

        // 8비트/16비트 정수 배열을 0~1 로 정규화하면서 통계도 함께 모은다
        void Adopt(const std::vector<uint16_t>& raw, int width, int height, int maxValue);

    private:
        int m_width = 0;
        int m_height = 0;
        int m_sourceBits = 0;

        std::wstring m_sourceKind;
        std::wstring m_fileName;

        std::vector<float> m_samples;   // 크기 = width * height, 값 0~1

        float m_minValue = 0.0f;
        float m_maxValue = 0.0f;
        int   m_distinctLevels = 0;
    };

    // 높이맵 한 장을 지형으로 펼치는 방법
    struct Params
    {
        // 높이맵 한 장이 XZ 평면에서 덮는 정사각형 크기(월드 단위).
        // 지형 전체 크기와 같게 두면 이미지가 지형에 딱 맞게 펼쳐진다.
        float worldSize = 256.0f;

        // 값 0~1 을 곱할 높이(월드 단위). 즉 최고점의 높이가 된다.
        float heightScale = 60.0f;

        // 전체를 위아래로 옮긴다. -heightScale/2 로 두면 지형이 원점을 기준으로 오르내린다.
        float heightOffset = 0.0f;

        // 이미지의 위쪽 행(v=0)을 월드 +Z(멀리) 에 둘지 여부.
        // 카메라가 -Z 에서 +Z 를 보고 있으므로, 켜두면 지도를 남쪽에서 보는 것과 같은 배치가 된다.
        bool flipZ = true;

        WrapMode wrap = WrapMode::Clamp;
    };

    // 월드 좌표 -> 최종 높이(y). GridMesh::HeightFunc 에 그대로 연결하면 된다.
    float Evaluate(const Image& image, float worldX, float worldZ, const Params& params);

    // ---------------- 파일 찾기 ----------------
    // heightmaps 폴더의 전체 경로(뒤에 '\' 포함). 못 찾으면 빈 문자열.
    // shaders 폴더와 같은 방식으로 exe 위치와 작업 폴더를 기준으로 위로 훑는다.
    std::wstring ResolveDirectory();

    // heightmaps 폴더 안의 높이맵 파일 전체 경로 목록 (이름 오름차순)
    std::vector<std::wstring> ListFiles();

    // 경로에서 파일 이름만 떼어낸다
    std::wstring GetFileNameOnly(const std::wstring& path);
}
