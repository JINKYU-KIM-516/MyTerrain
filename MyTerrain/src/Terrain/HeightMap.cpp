#include "HeightMap.h"
#include "../Framework/ShaderUtil.h"

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

namespace
{
    bool DirectoryExists(const std::wstring& path)
    {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    std::wstring ToLower(std::wstring text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        return text;
    }

    std::wstring GetExtensionLower(const std::wstring& path)
    {
        const size_t dot = path.find_last_of(L'.');
        if (dot == std::wstring::npos)
        {
            return L"";
        }
        return ToLower(path.substr(dot + 1));
    }

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

    // 정수를 wrap 규칙에 따라 [0, size) 안으로 접는다
    int FoldIndex(int value, int size, HeightMap::WrapMode wrap)
    {
        if (size <= 1)
        {
            return 0;
        }

        switch (wrap)
        {
        case HeightMap::WrapMode::Wrap:
        {
            const int m = value % size;
            return (m < 0) ? (m + size) : m;
        }

        case HeightMap::WrapMode::Mirror:
        {
            const int period = size * 2;
            int m = value % period;
            if (m < 0)
            {
                m += period;
            }
            return (m < size) ? m : (period - 1 - m);
        }

        case HeightMap::WrapMode::Clamp:
        default:
            return std::clamp(value, 0, size - 1);
        }
    }
}

namespace HeightMap
{
    const wchar_t* ToName(WrapMode mode)
    {
        switch (mode)
        {
        case WrapMode::Clamp:  return L"Clamp (가장자리 늘리기)";
        case WrapMode::Wrap:   return L"Wrap (타일링)";
        case WrapMode::Mirror: return L"Mirror (거울 반복)";
        default:               return L"알 수 없음";
        }
    }

    std::wstring GetFileNameOnly(const std::wstring& path)
    {
        const size_t slash = path.find_last_of(L"\\/");
        return (slash == std::wstring::npos) ? path : path.substr(slash + 1);
    }

    // ============================================================
    // Image
    // ============================================================

    void Image::Clear()
    {
        m_width = 0;
        m_height = 0;
        m_sourceBits = 0;
        m_sourceKind.clear();
        m_fileName.clear();
        m_samples.clear();
        m_minValue = 0.0f;
        m_maxValue = 0.0f;
        m_distinctLevels = 0;
    }

    // 정수 배열을 0~1 로 정규화하면서 통계(최소/최대/계조 단계 수)를 함께 모은다.
    // 계조 단계 수를 원본 정수에서 세는 이유는, 실수로 바꾼 뒤에는
    // 8비트에서 온 값인지 16비트에서 온 값인지 구별할 수 없기 때문이다.
    void Image::Adopt(const std::vector<uint16_t>& raw, int width, int height, int maxValue)
    {
        m_width = width;
        m_height = height;

        const float inv = 1.0f / static_cast<float>(std::max(maxValue, 1));

        m_samples.resize(raw.size());

        std::vector<bool> seen(static_cast<size_t>(maxValue) + 1, false);
        int distinct = 0;

        uint16_t rawMin = 0xFFFFu;
        uint16_t rawMax = 0u;

        for (size_t i = 0; i < raw.size(); ++i)
        {
            const uint16_t value = raw[i];
            m_samples[i] = static_cast<float>(value) * inv;

            rawMin = std::min(rawMin, value);
            rawMax = std::max(rawMax, value);

            if (value <= maxValue && !seen[value])
            {
                seen[value] = true;
                ++distinct;
            }
        }

        if (raw.empty())
        {
            rawMin = 0;
            rawMax = 0;
        }

        m_minValue = static_cast<float>(rawMin) * inv;
        m_maxValue = static_cast<float>(rawMax) * inv;
        m_distinctLevels = distinct;
    }

    bool Image::LoadFromFile(const std::wstring& path, std::wstring* outError)
    {
        Clear();

        const std::wstring extension = GetExtensionLower(path);
        const bool isRaw = (extension == L"raw" || extension == L"r16" || extension == L"r8" || extension == L"bin");

        const bool ok = isRaw ? LoadRaw(path, outError) : LoadWic(path, outError);

        if (ok)
        {
            m_fileName = GetFileNameOnly(path);
        }
        else
        {
            Clear();
        }

        return ok;
    }

    // ---------------- RAW (헤더 없는 원시 높이 배열) ----------------
    //
    // 헤더가 없으므로 파일 크기만으로 해상도를 역산한다.
    // 16비트 정사각형으로 딱 떨어지면 16비트, 아니면 8비트로 본다.
    // (1024x1024 16비트 = 2,097,152 바이트 처럼 대부분 한 가지로만 떨어진다)
    bool Image::LoadRaw(const std::wstring& path, std::wstring* outError)
    {
        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            if (outError) *outError = L"파일을 열 수 없습니다: " + path;
            return false;
        }

        LARGE_INTEGER fileSize = {};
        if (!GetFileSizeEx(file, &fileSize) || fileSize.QuadPart <= 0)
        {
            CloseHandle(file);
            if (outError) *outError = L"파일 크기를 읽을 수 없습니다: " + path;
            return false;
        }

        const size_t byteCount = static_cast<size_t>(fileSize.QuadPart);

        // 해상도 역산
        int side = 0;
        int bits = 0;

        {
            const size_t count16 = byteCount / 2;
            const int guess16 = static_cast<int>(std::lround(std::sqrt(static_cast<double>(count16))));
            if (byteCount % 2 == 0 && guess16 > 0 &&
                static_cast<size_t>(guess16) * guess16 * 2 == byteCount)
            {
                side = guess16;
                bits = 16;
            }
            else
            {
                const int guess8 = static_cast<int>(std::lround(std::sqrt(static_cast<double>(byteCount))));
                if (guess8 > 0 && static_cast<size_t>(guess8) * guess8 == byteCount)
                {
                    side = guess8;
                    bits = 8;
                }
            }
        }

        if (side <= 0)
        {
            CloseHandle(file);
            if (outError)
            {
                *outError = L"RAW 해상도를 알아낼 수 없습니다 (정사각형이 아닌 듯합니다): " + GetFileNameOnly(path);
            }
            return false;
        }

        std::vector<uint8_t> bytes(byteCount);
        DWORD readBytes = 0;
        const BOOL readOk = ReadFile(file, bytes.data(), static_cast<DWORD>(byteCount), &readBytes, nullptr);
        CloseHandle(file);

        if (!readOk || readBytes != byteCount)
        {
            if (outError) *outError = L"RAW 파일을 끝까지 읽지 못했습니다: " + GetFileNameOnly(path);
            return false;
        }

        const size_t texelCount = static_cast<size_t>(side) * side;
        std::vector<uint16_t> raw(texelCount);

        if (bits == 16)
        {
            // 리틀엔디언 (World Machine / Gaea / Unity 의 기본)
            for (size_t i = 0; i < texelCount; ++i)
            {
                raw[i] = static_cast<uint16_t>(bytes[i * 2] | (static_cast<uint16_t>(bytes[i * 2 + 1]) << 8));
            }
            Adopt(raw, side, side, 65535);
            m_sourceBits = 16;
            m_sourceKind = L"16비트 RAW";
        }
        else
        {
            for (size_t i = 0; i < texelCount; ++i)
            {
                raw[i] = bytes[i];
            }
            Adopt(raw, side, side, 255);
            m_sourceBits = 8;
            m_sourceKind = L"8비트 RAW";
        }

        return true;
    }

    // ---------------- WIC (PNG / BMP / TIFF / JPG) ----------------
    //
    // 8비트 원본은 8bppGray 로, 16비트 이상 원본은 16bppGray 로 변환해서 읽는다.
    // 심도를 바꾸지 않고 그레이스케일로만 변환하는 이유는, WIC 가 8bpp <-> 16bpp
    // 그레이 변환에서 감마를 건드릴 수 있기 때문이다. 높이 값이 감마로 휘면 지형이 달라진다.
    bool Image::LoadWic(const std::wstring& path, std::wstring* outError)
    {
        // 앱의 다른 부분이 이미 COM 을 초기화했을 수 있다.
        // 성공(S_OK/S_FALSE)했을 때만 짝을 맞춰 CoUninitialize 를 부른다.
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
                *outError = L"이미지를 열 수 없습니다 (형식을 지원하지 않거나 손상됨): " + GetFileNameOnly(path);
            }
            return false;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr))
        {
            if (outError) *outError = L"이미지 프레임을 읽을 수 없습니다: " + GetFileNameOnly(path);
            return false;
        }

        UINT width = 0;
        UINT height = 0;
        hr = frame->GetSize(&width, &height);
        if (FAILED(hr) || width == 0 || height == 0)
        {
            if (outError) *outError = L"이미지 크기가 올바르지 않습니다: " + GetFileNameOnly(path);
            return false;
        }

        // ---- 원본의 채널당 비트 수 알아내기 ----
        int sourceBits = 8;
        {
            WICPixelFormatGUID pixelFormat = {};
            if (SUCCEEDED(frame->GetPixelFormat(&pixelFormat)))
            {
                ComPtr<IWICComponentInfo> componentInfo;
                if (SUCCEEDED(factory->CreateComponentInfo(pixelFormat, &componentInfo)))
                {
                    ComPtr<IWICPixelFormatInfo> formatInfo;
                    if (SUCCEEDED(componentInfo.As(&formatInfo)))
                    {
                        UINT bitsPerPixel = 0;
                        UINT channelCount = 0;
                        if (SUCCEEDED(formatInfo->GetBitsPerPixel(&bitsPerPixel)) &&
                            SUCCEEDED(formatInfo->GetChannelCount(&channelCount)) &&
                            channelCount > 0)
                        {
                            sourceBits = static_cast<int>(bitsPerPixel / channelCount);
                        }
                    }
                }
            }
        }

        const bool useSixteen = (sourceBits >= 16);

        ComPtr<IWICBitmapSource> converted;
        hr = WICConvertBitmapSource(useSixteen ? GUID_WICPixelFormat16bppGray : GUID_WICPixelFormat8bppGray,
                                    frame.Get(), &converted);
        if (FAILED(hr))
        {
            if (outError) *outError = L"그레이스케일로 변환하지 못했습니다: " + GetFileNameOnly(path);
            return false;
        }

        const size_t texelCount = static_cast<size_t>(width) * height;
        std::vector<uint16_t> raw(texelCount);

        if (useSixteen)
        {
            const UINT stride = width * 2;
            std::vector<uint8_t> buffer(static_cast<size_t>(stride) * height);

            hr = converted->CopyPixels(nullptr, stride, static_cast<UINT>(buffer.size()), buffer.data());
            if (FAILED(hr))
            {
                if (outError) *outError = L"픽셀을 복사하지 못했습니다: " + GetFileNameOnly(path);
                return false;
            }

            for (size_t i = 0; i < texelCount; ++i)
            {
                raw[i] = static_cast<uint16_t>(buffer[i * 2] | (static_cast<uint16_t>(buffer[i * 2 + 1]) << 8));
            }

            Adopt(raw, static_cast<int>(width), static_cast<int>(height), 65535);
            m_sourceBits = 16;
        }
        else
        {
            const UINT stride = width;
            std::vector<uint8_t> buffer(static_cast<size_t>(stride) * height);

            hr = converted->CopyPixels(nullptr, stride, static_cast<UINT>(buffer.size()), buffer.data());
            if (FAILED(hr))
            {
                if (outError) *outError = L"픽셀을 복사하지 못했습니다: " + GetFileNameOnly(path);
                return false;
            }

            for (size_t i = 0; i < texelCount; ++i)
            {
                raw[i] = buffer[i];
            }

            Adopt(raw, static_cast<int>(width), static_cast<int>(height), 255);
            m_sourceBits = 8;
        }

        // 형식 이름 (HUD 표시용)
        const std::wstring extension = GetExtensionLower(path);
        std::wstring formatName = L"이미지";
        if (extension == L"png")                            formatName = L"PNG";
        else if (extension == L"bmp")                       formatName = L"BMP";
        else if (extension == L"tif" || extension == L"tiff") formatName = L"TIFF";
        else if (extension == L"jpg" || extension == L"jpeg") formatName = L"JPG (손실 압축)";

        m_sourceKind = std::to_wstring(m_sourceBits) + L"비트 " + formatName;

        return true;
    }

    // ---------------- 샘플링 ----------------

    float Image::SampleTexel(int x, int y, WrapMode wrap) const
    {
        if (!IsValid())
        {
            return 0.0f;
        }

        x = FoldIndex(x, m_width, wrap);
        y = FoldIndex(y, m_height, wrap);

        return m_samples[static_cast<size_t>(y) * m_width + x];
    }

    // 텍셀 중심 규약: u=0 은 첫 텍셀의 왼쪽 경계, 텍셀 중심은 (i + 0.5) / width 에 있다.
    // 그래서 표본 위치는 u * width - 0.5 가 된다.
    float Image::SampleBilinear(float u, float v, WrapMode wrap) const
    {
        if (!IsValid())
        {
            return 0.0f;
        }

        const float px = u * static_cast<float>(m_width) - 0.5f;
        const float py = v * static_cast<float>(m_height) - 0.5f;

        const float fx = std::floor(px);
        const float fy = std::floor(py);

        const int x0 = static_cast<int>(fx);
        const int y0 = static_cast<int>(fy);

        const float tx = px - fx;
        const float ty = py - fy;

        const float h00 = SampleTexel(x0,     y0,     wrap);
        const float h10 = SampleTexel(x0 + 1, y0,     wrap);
        const float h01 = SampleTexel(x0,     y0 + 1, wrap);
        const float h11 = SampleTexel(x0 + 1, y0 + 1, wrap);

        const float top = h00 + (h10 - h00) * tx;
        const float bottom = h01 + (h11 - h01) * tx;

        return top + (bottom - top) * ty;
    }

    // ---------------- 월드 좌표 -> 높이 ----------------

    float Evaluate(const Image& image, float worldX, float worldZ, const Params& params)
    {
        if (!image.IsValid())
        {
            return params.heightOffset;
        }

        const float inverseSize = 1.0f / std::max(params.worldSize, 0.0001f);

        const float u = worldX * inverseSize + 0.5f;
        const float v = 0.5f + worldZ * inverseSize * (params.flipZ ? -1.0f : 1.0f);

        const float value = image.SampleBilinear(u, v, params.wrap);

        return value * params.heightScale + params.heightOffset;
    }

    // ---------------- 파일 찾기 ----------------

    std::wstring ResolveDirectory()
    {
        const std::wstring exeDir = ShaderUtil::GetExecutableDirectory();
        const std::wstring cwd = GetCurrentDirectoryPath();

        // shaders 폴더를 찾는 방식과 같다 (ShaderUtil::ResolveShaderPath 참고)
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
            L"heightmaps\\",
            L"MyTerrain\\heightmaps\\",
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

    std::vector<std::wstring> ListFiles()
    {
        std::vector<std::wstring> files;

        const std::wstring directory = ResolveDirectory();
        if (directory.empty())
        {
            return files;
        }

        static const std::set<std::wstring> kExtensions =
        {
            L"png", L"bmp", L"tif", L"tiff", L"jpg", L"jpeg",
            L"raw", L"r16", L"r8", L"bin",
        };

        WIN32_FIND_DATAW findData = {};
        HANDLE find = FindFirstFileW((directory + L"*").c_str(), &findData);
        if (find == INVALID_HANDLE_VALUE)
        {
            return files;
        }

        do
        {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                continue;
            }

            const std::wstring name = findData.cFileName;
            if (kExtensions.count(GetExtensionLower(name)) == 0)
            {
                continue;
            }

            files.push_back(directory + name);
        }
        while (FindNextFileW(find, &findData));

        FindClose(find);

        std::sort(files.begin(), files.end(),
                  [](const std::wstring& a, const std::wstring& b)
                  {
                      return ToLower(GetFileNameOnly(a)) < ToLower(GetFileNameOnly(b));
                  });

        return files;
    }
}
