#include "SplatTexture.h"
#include "../Framework/Framework.h"
#include "../Framework/ShaderUtil.h"

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>

#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

namespace
{
    bool DirectoryExists(const std::wstring& path)
    {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    // HeightMap::ResolveDirectory / ShaderUtil::ResolveShaderPath 와 똑같은 방식으로
    // exe 위치와 현재 작업 폴더를 기준으로 위로 훑는다.
    std::wstring GetCurrentDirectoryPath()
    {
        wchar_t buffer[MAX_PATH] = {};
        const DWORD length = GetCurrentDirectoryW(MAX_PATH, buffer);
        if (length == 0)
        {
            return L"";
        }

        std::wstring result(buffer, length);
        if (!result.empty() && result.back() != L'\\')
        {
            result += L'\\';
        }
        return result;
    }

    ID3D11Device* GetDevice()
    {
        Framework* framework = Framework::GetInstance();
        return (framework != nullptr) ? framework->GetRenderer().GetDevice() : nullptr;
    }

    // 슬롯 순서와 파일 이름을 한곳에 묶어둔다 -- 배열 인덱스 = SplatTexture::Slot 값
    const wchar_t* const kLayerFileNames[] =
    {
        L"splat_sand.png",
        L"splat_grass.png",
        L"splat_rock.png",
        L"splat_snow.png",
    };
    constexpr int kLayerCount = static_cast<int>(std::size(kLayerFileNames));
}

std::wstring SplatTexture::ResolveDirectory()
{
    const std::wstring exeDir = ShaderUtil::GetExecutableDirectory();
    const std::wstring cwd = GetCurrentDirectoryPath();

    const std::wstring roots[] =
    {
        exeDir,
        exeDir + L"..\\",
        exeDir + L"..\\..\\",
        exeDir + L"..\\..\\..\\",
        cwd,
        cwd + L"..\\",
        cwd + L"..\\..\\",
    };

    const std::wstring subPaths[] =
    {
        L"textures\\",
        L"MyTerrain\\textures\\",
    };

    for (const std::wstring& root : roots)
    {
        if (root.empty())
        {
            continue;
        }

        for (const std::wstring& sub : subPaths)
        {
            const std::wstring candidate = root + sub;
            if (DirectoryExists(candidate))
            {
                return candidate;
            }
        }
    }

    return L"";
}

// ---------------- WIC 로더 (32bppRGBA 로 변환) ----------------
//
// HeightMap::Image::LoadWic 와 뼈대는 같지만(팩토리 생성 -> 디코더 -> 프레임 -> 변환 ->
// CopyPixels), 그레이스케일이 아니라 색이 있는 디퓨즈 텍스처를 그대로 받아야 하므로
// 별도로 둔다. 감마 걱정이 없는 것도 차이점이다 -- 높이 값이 아니라 눈에 보이는
// 색 자체가 목적이라, WIC 가 sRGB 로 다루든 말든 화면에 나오는 결과가 기준이다.
bool SplatTexture::LoadRgba(const std::wstring& path, RgbaImage& out, std::wstring* outError)
{
    const HRESULT coResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool needUninitialize = SUCCEEDED(coResult);

    struct ComScope
    {
        bool active;
        ~ComScope() { if (active) CoUninitialize(); }
    } comScope{ needUninitialize };

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        if (outError) *outError = L"WIC 팩토리를 만들 수 없습니다.";
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                            WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr))
    {
        if (outError)
        {
            *outError = L"스플래팅 텍스처를 열 수 없습니다: " + path;
        }
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
    {
        if (outError) *outError = L"이미지 프레임을 읽을 수 없습니다: " + path;
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    hr = frame->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0)
    {
        if (outError) *outError = L"이미지 크기가 올바르지 않습니다: " + path;
        return false;
    }

    ComPtr<IWICBitmapSource> converted;
    hr = WICConvertBitmapSource(GUID_WICPixelFormat32bppRGBA, frame.Get(), &converted);
    if (FAILED(hr))
    {
        if (outError) *outError = L"RGBA 로 변환하지 못했습니다: " + path;
        return false;
    }

    const UINT stride = width * 4;
    out.pixels.resize(static_cast<size_t>(stride) * height);
    out.width = static_cast<int>(width);
    out.height = static_cast<int>(height);

    hr = converted->CopyPixels(nullptr, stride, static_cast<UINT>(out.pixels.size()), out.pixels.data());
    if (FAILED(hr))
    {
        if (outError) *outError = L"픽셀을 복사하지 못했습니다: " + path;
        return false;
    }

    return true;
}

void SplatTexture::Reset()
{
    m_texture.Reset();
    m_srv.Reset();
    m_sampler.Reset();
    m_width = 0;
    m_height = 0;
}

bool SplatTexture::LoadFixedSet(std::wstring* outError)
{
    if (IsReady())
    {
        return true;
    }

    ID3D11Device* device = GetDevice();
    if (device == nullptr)
    {
        if (outError) *outError = L"디바이스가 아직 준비되지 않았습니다.";
        return false;
    }

    const std::wstring directory = ResolveDirectory();
    if (directory.empty())
    {
        if (outError) *outError = L"textures 폴더를 찾지 못했습니다.";
        return false;
    }

    std::array<RgbaImage, kLayerCount> images;

    for (int i = 0; i < kLayerCount; ++i)
    {
        const std::wstring path = directory + kLayerFileNames[i];
        if (!LoadRgba(path, images[i], outError))
        {
            return false;
        }
    }

    // Texture2DArray 는 모든 슬라이스의 해상도가 같아야 한다
    const int width = images[0].width;
    const int height = images[0].height;

    for (int i = 1; i < kLayerCount; ++i)
    {
        if (images[i].width != width || images[i].height != height)
        {
            if (outError)
            {
                *outError = L"스플래팅 텍스처 4장의 해상도가 서로 다릅니다 (" +
                    std::wstring(kLayerFileNames[0]) + L" 은 " + std::to_wstring(width) + L"x" + std::to_wstring(height) +
                    L", " + std::wstring(kLayerFileNames[i]) + L" 은 " + std::to_wstring(images[i].width) + L"x" + std::to_wstring(images[i].height) + L")";
            }
            return false;
        }
    }

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = static_cast<UINT>(width);
    textureDesc.Height = static_cast<UINT>(height);
    textureDesc.MipLevels = 1;              // 밉맵은 아직 쓰지 않는다 (LOD 기법에서 다시 다룰 것)
    textureDesc.ArraySize = kLayerCount;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    std::array<D3D11_SUBRESOURCE_DATA, kLayerCount> initialData{};
    for (int i = 0; i < kLayerCount; ++i)
    {
        initialData[i].pSysMem = images[i].pixels.data();
        initialData[i].SysMemPitch = static_cast<UINT>(width * 4);
    }

    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(device->CreateTexture2D(&textureDesc, initialData.data(), &texture)))
    {
        if (outError) *outError = L"스플래팅 배열 텍스처를 만들지 못했습니다.";
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.ArraySize = kLayerCount;
    srvDesc.Texture2DArray.FirstArraySlice = 0;

    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(device->CreateShaderResourceView(texture.Get(), &srvDesc, &srv)))
    {
        if (outError) *outError = L"스플래팅 SRV 를 만들지 못했습니다.";
        return false;
    }

    // Wrap 고정: 타일링 UV(worldPos.xz * tiling)는 [0,1] 을 한참 벗어나며 반복되어야 한다.
    // 높이맵처럼 Clamp/Mirror 를 고를 이유가 없다.
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    ComPtr<ID3D11SamplerState> sampler;
    if (FAILED(device->CreateSamplerState(&samplerDesc, &sampler)))
    {
        if (outError) *outError = L"스플래팅 샘플러를 만들지 못했습니다.";
        return false;
    }

    m_texture = texture;
    m_srv = srv;
    m_sampler = sampler;
    m_width = width;
    m_height = height;

    return true;
}
