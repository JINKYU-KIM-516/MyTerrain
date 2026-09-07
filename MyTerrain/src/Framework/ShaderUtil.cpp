#include "ShaderUtil.h"
#include <d3dcompiler.h>
#include <vector>
#include <set>

#pragma comment(lib, "d3dcompiler.lib")

namespace
{
    bool FileExists(const std::wstring& path)
    {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
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
}

namespace ShaderUtil
{
    std::wstring GetExecutableDirectory()
    {
        wchar_t buffer[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        if (length == 0)
        {
            return L"";
        }

        std::wstring path(buffer, length);
        const size_t slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
        {
            return L"";
        }

        return path.substr(0, slash + 1);
    }

    std::wstring ResolveShaderPath(const std::wstring& fileName)
    {
        const std::wstring exeDir = GetExecutableDirectory();
        const std::wstring cwd = GetCurrentDirectoryPath();

        // 후보 기준 폴더
        //   exe        : <sln>\bin\x64\Debug\        (빌드 결과물 위치)
        //   exe\..\..\.. : <sln>\                    (솔루션 루트)
        //   cwd        : <sln>\MyTerrain\            (Visual Studio 디버깅 기본 작업 폴더)
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

        // 각 기준 폴더 아래에서 찾아볼 상대 경로
        const std::wstring subPaths[] =
        {
            L"shaders\\",
            L"MyTerrain\\shaders\\",
        };

        for (const std::wstring& root : roots)
        {
            if (root.empty())
            {
                continue;
            }

            for (const std::wstring& sub : subPaths)
            {
                const std::wstring candidate = root + sub + fileName;
                if (FileExists(candidate))
                {
                    return candidate;
                }
            }
        }

        return L"";
    }

    ComPtr<ID3DBlob> CompileFromFile(const std::wstring& fileName, const char* entryPoint,
                                     const char* target, std::wstring* outError)
    {
        ComPtr<ID3DBlob> byteCode;

        const std::wstring path = ResolveShaderPath(fileName);
        if (path.empty())
        {
            if (outError)
            {
                *outError = L"셰이더 파일을 찾을 수 없습니다: shaders\\" + fileName;
            }
            return nullptr;
        }

        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        ComPtr<ID3DBlob> errorBlob;
        const HRESULT hr = D3DCompileFromFile(
            path.c_str(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint,
            target,
            flags,
            0,
            &byteCode,
            &errorBlob);

        if (FAILED(hr))
        {
            if (outError)
            {
                std::wstring message = L"셰이더 컴파일 실패: " + path + L"\n\n";

                if (errorBlob)
                {
                    // 컴파일러 오류 메시지는 ANSI 문자열이다
                    const char* text = static_cast<const char*>(errorBlob->GetBufferPointer());
                    const int size = static_cast<int>(errorBlob->GetBufferSize());
                    const int wideLength = MultiByteToWideChar(CP_ACP, 0, text, size, nullptr, 0);

                    if (wideLength > 0)
                    {
                        std::vector<wchar_t> wide(static_cast<size_t>(wideLength));
                        MultiByteToWideChar(CP_ACP, 0, text, size, wide.data(), wideLength);
                        message.append(wide.data(), static_cast<size_t>(wideLength));
                    }
                }

                *outError = message;
            }
            return nullptr;
        }

        return byteCode;
    }

    void ReportErrorOnce(const std::wstring& message)
    {
        static std::set<std::wstring> s_reported;

        if (s_reported.count(message) > 0)
        {
            return;
        }
        s_reported.insert(message);

        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
        MessageBoxW(nullptr, message.c_str(), L"Shader Error", MB_OK | MB_ICONERROR);
    }
}
