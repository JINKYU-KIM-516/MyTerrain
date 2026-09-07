#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <string>
#include <vector>

// Direct2D + DirectWrite 기반 2D 텍스트(UI) 렌더러.
//
// D3DRenderer가 만든 스왑체인의 백버퍼를 그대로 D2D 렌더타겟으로 감싸서
// 3D 렌더링이 끝난 뒤 그 위에 UI 텍스트를 덧그린다.
//
// 사용 순서 (Framework의 게임 루프에서 처리됨)
//   BeginDraw() -> DrawLayout()/DrawRectangle() ... -> EndDraw()
//
// 주의: 창 크기가 바뀔 때는 스왑체인 ResizeBuffers 이전에 반드시
//       ReleaseSizeDependentResources()로 백버퍼 참조를 풀어주어야 한다.
class TextRenderer
{
public:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    TextRenderer() = default;
    ~TextRenderer() = default;

    bool Initialize(IDXGISwapChain* swapChain);
    void Shutdown();

    // 창 크기 변경 대응
    void ReleaseSizeDependentResources();
    bool CreateSizeDependentResources(IDXGISwapChain* swapChain);

    bool IsReady() const { return m_renderTarget != nullptr; }

    // ---- 프레임 단위 ----
    void BeginDraw();
    void EndDraw();

    // ---- 텍스트 ----
    // 텍스트 레이아웃을 만든다. (문자열/폰트크기가 바뀔 때만 다시 만들면 된다)
    ComPtr<IDWriteTextLayout> CreateLayout(const std::wstring& text, float fontSize, bool bold,
                                           float maxWidth = 4096.0f, float maxHeight = 4096.0f);

    // 만들어둔 레이아웃을 (x, y) 위치에 그린다. 좌표 단위는 픽셀.
    void DrawLayout(IDWriteTextLayout* layout, float x, float y, const D2D1_COLOR_F& color);

    // 화면(백버퍼) 크기. 픽셀 단위.
    float GetScreenWidth() const;
    float GetScreenHeight() const;

private:
    IDWriteTextFormat* GetTextFormat(float fontSize, bool bold);

private:
    struct FormatCacheEntry
    {
        float fontSize;
        bool  bold;
        ComPtr<IDWriteTextFormat> format;
    };

    ComPtr<ID2D1Factory>          m_d2dFactory;
    ComPtr<IDWriteFactory>        m_dwriteFactory;
    ComPtr<ID2D1RenderTarget>     m_renderTarget;
    ComPtr<ID2D1SolidColorBrush>  m_brush;

    std::vector<FormatCacheEntry> m_formatCache;

    bool m_drawing = false;
};
