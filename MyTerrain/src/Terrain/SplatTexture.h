#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <cstdint>

// 텍스처 스플래팅(4번 기법)에 쓰는 디퓨즈 텍스처 4장(모래/잔디/바위/눈)을
// 하나의 Texture2DArray 로 올리는 헬퍼.
//
// 높이맵(HeightMapTexture)과 달리 "아무 이미지나" 읽지 않는다. 네 레이어가
// 정확히 무엇인지가 픽셀 셰이더의 높이/경사 구간 로직과 직접 엮여 있으므로,
// textures/ 폴더 안의 고정된 파일 이름 4개만 찾는다.
//
//   슬롯 0 = 모래(splat_sand.png)   1 = 잔디(splat_grass.png)
//   슬롯 1 = 바위(splat_rock.png)   3 = 눈(splat_snow.png)
//
// Texture2DArray 는 모든 슬라이스의 해상도가 같아야 하므로, 네 이미지 크기가
// 다르면 LoadFixedSet 이 실패하고 이유를 outError 에 담는다.
//
// 밉맵은 아직 쓰지 않는다(HeightMapTexture 와 같은 이유 -- LOD 기법에서 다시 다룰 것).
// 그 대신 샘플러 주소 지정은 항상 Wrap 이다: 타일링 UV(worldPos.xz * tiling)가
// [0,1] 범위를 한참 벗어나며 반복되어야 하므로, 높이맵처럼 Clamp/Mirror 를
// 선택할 이유가 없다.
class SplatTexture
{
public:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    enum class Slot
    {
        Sand = 0,
        Grass,
        Rock,
        Snow,
        Count
    };

    // textures/ 폴더에서 고정된 이름 4개를 읽어 GPU 배열 텍스처로 올린다.
    // 이미 올라와 있으면 다시 만들지 않고 true 를 돌려준다.
    bool LoadFixedSet(std::wstring* outError = nullptr);

    void Reset();
    bool IsReady() const { return m_srv != nullptr && m_sampler != nullptr; }

    ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }
    ID3D11SamplerState* GetSampler() const { return m_sampler.Get(); }

    int GetWidth()  const { return m_width; }
    int GetHeight() const { return m_height; }

    // textures/ 폴더의 전체 경로(뒤에 '\' 포함). 못 찾으면 빈 문자열.
    // heightmaps 폴더를 찾는 방식(ShaderUtil::ResolveShaderPath)과 같다.
    static std::wstring ResolveDirectory();

private:
    struct RgbaImage
    {
        std::vector<uint8_t> pixels;   // width * height * 4 (RGBA8, 상단부터)
        int width = 0;
        int height = 0;
    };

    // WIC 로 이미지를 읽어 32bppRGBA 로 변환한다. HeightMap::Image 의 그레이스케일
    // 로더와는 별도다 -- 여기는 색이 있는 디퓨즈 텍스처를 그대로 읽어야 한다.
    static bool LoadRgba(const std::wstring& path, RgbaImage& out, std::wstring* outError);

private:
    ComPtr<ID3D11Texture2D>          m_texture;
    ComPtr<ID3D11ShaderResourceView> m_srv;
    ComPtr<ID3D11SamplerState>       m_sampler;

    int m_width = 0;
    int m_height = 0;
};
