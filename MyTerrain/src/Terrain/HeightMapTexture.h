#pragma once
#include "HeightMap.h"
#include <d3d11.h>
#include <wrl/client.h>

// 높이맵을 GPU 텍스처(R16_UNORM)로 올려두는 작은 헬퍼.
//
// 3번 기법 자체는 CPU 에서 높이를 샘플링해 정점에 구워 넣으므로 이 텍스처가 없어도 돌아간다.
// 그럼에도 올려두는 이유는 두 가지다.
//
//  1) 검증 : 픽셀 셰이더가 이 텍스처를 읽어 고도별로 색을 칠하게 하면(C 키),
//            색 띠가 지형의 실제 기복과 어긋나는지 눈으로 바로 확인할 수 있다.
//            어긋나면 CPU 샘플링과 GPU 샘플링의 좌표 규약이 다르다는 뜻이다.
//  2) 준비 : 7번 하드웨어 테셀레이션에서는 도메인 셰이더가 이 텍스처를 직접 읽어
//            정점을 밀어 올린다(vertex displacement). 그때 이 클래스를 그대로 쓴다.
//
// R16_UNORM 을 쓰는 이유 : 높이는 채널 하나면 충분하고, 16비트여야 계단 현상이 없다.
// 셰이더에서는 .r 이 0~1 실수로 읽힌다.
class HeightMapTexture
{
public:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    // 이미지를 텍스처로 올린다. 이전 텍스처는 버리고 새로 만든다.
    bool Upload(const HeightMap::Image& image);

    // 경계 처리 방식(= 샘플러의 주소 지정 모드)을 바꾼다. 같은 값이면 아무 일도 하지 않는다.
    bool SetWrapMode(HeightMap::WrapMode wrap);

    void Reset();

    bool IsReady() const { return m_srv != nullptr && m_sampler != nullptr; }

    ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }
    ID3D11SamplerState* GetSampler() const { return m_sampler.Get(); }

    int GetWidth()  const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    bool CreateSampler(ID3D11Device* device, HeightMap::WrapMode wrap);

private:
    ComPtr<ID3D11Texture2D>          m_texture;
    ComPtr<ID3D11ShaderResourceView> m_srv;
    ComPtr<ID3D11SamplerState>       m_sampler;

    int m_width = 0;
    int m_height = 0;

    HeightMap::WrapMode m_wrap = HeightMap::WrapMode::Count;   // Count = 아직 만들지 않음
};
