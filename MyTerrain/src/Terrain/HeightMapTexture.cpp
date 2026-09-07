#include "HeightMapTexture.h"
#include "../Framework/Framework.h"

#include <algorithm>
#include <vector>

namespace
{
    D3D11_TEXTURE_ADDRESS_MODE ToAddressMode(HeightMap::WrapMode wrap)
    {
        switch (wrap)
        {
        case HeightMap::WrapMode::Wrap:   return D3D11_TEXTURE_ADDRESS_WRAP;
        case HeightMap::WrapMode::Mirror: return D3D11_TEXTURE_ADDRESS_MIRROR;
        case HeightMap::WrapMode::Clamp:
        default:                          return D3D11_TEXTURE_ADDRESS_CLAMP;
        }
    }

    ID3D11Device* GetDevice()
    {
        Framework* framework = Framework::GetInstance();
        return (framework != nullptr) ? framework->GetRenderer().GetDevice() : nullptr;
    }
}

void HeightMapTexture::Reset()
{
    m_texture.Reset();
    m_srv.Reset();
    m_sampler.Reset();

    m_width = 0;
    m_height = 0;
    m_wrap = HeightMap::WrapMode::Count;
}

bool HeightMapTexture::CreateSampler(ID3D11Device* device, HeightMap::WrapMode wrap)
{
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;   // CPU 쪽 바이리니어와 맞춘다
    samplerDesc.AddressU = ToAddressMode(wrap);
    samplerDesc.AddressV = ToAddressMode(wrap);
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    ComPtr<ID3D11SamplerState> sampler;
    if (FAILED(device->CreateSamplerState(&samplerDesc, &sampler)))
    {
        return false;
    }

    m_sampler = sampler;
    m_wrap = wrap;
    return true;
}

bool HeightMapTexture::SetWrapMode(HeightMap::WrapMode wrap)
{
    if (m_wrap == wrap && m_sampler)
    {
        return true;
    }

    ID3D11Device* device = GetDevice();
    if (device == nullptr)
    {
        return false;
    }

    return CreateSampler(device, wrap);
}

bool HeightMapTexture::Upload(const HeightMap::Image& image)
{
    if (!image.IsValid())
    {
        Reset();
        return false;
    }

    ID3D11Device* device = GetDevice();
    if (device == nullptr)
    {
        return false;
    }

    const int width = image.GetWidth();
    const int height = image.GetHeight();

    // 0~1 실수를 16비트 정수로 되돌린다.
    // 원본이 8비트였더라도 16비트 형식으로 올린다. 8비트 계단은 값 자체에 이미
    // 들어있으므로 그대로 보이고, 텍스처 형식을 하나로 통일할 수 있다.
    std::vector<uint16_t> texels(static_cast<size_t>(width) * height);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const float value = std::clamp(image.SampleTexel(x, y, HeightMap::WrapMode::Clamp), 0.0f, 1.0f);
            texels[static_cast<size_t>(y) * width + x] =
                static_cast<uint16_t>(value * 65535.0f + 0.5f);
        }
    }

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = static_cast<UINT>(width);
    textureDesc.Height = static_cast<UINT>(height);
    textureDesc.MipLevels = 1;          // 밉맵은 아직 쓰지 않는다 (LOD 기법에서 다시 다룰 것)
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R16_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initialData = {};
    initialData.pSysMem = texels.data();
    initialData.SysMemPitch = static_cast<UINT>(width * sizeof(uint16_t));

    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(device->CreateTexture2D(&textureDesc, &initialData, &texture)))
    {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(device->CreateShaderResourceView(texture.Get(), &srvDesc, &srv)))
    {
        return false;
    }

    m_texture = texture;
    m_srv = srv;
    m_width = width;
    m_height = height;

    // 샘플러가 아직 없으면 기본(Clamp)으로 하나 만들어둔다
    if (!m_sampler && !CreateSampler(device, HeightMap::WrapMode::Clamp))
    {
        return false;
    }

    return true;
}
