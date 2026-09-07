#include "TextRenderer.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace
{
    // 한글 표시를 위한 기본 폰트 (Windows 기본 설치 폰트)
    constexpr wchar_t kFontFamily[] = L"Malgun Gothic";
    constexpr wchar_t kLocale[] = L"ko-kr";
}

bool TextRenderer::Initialize(IDXGISwapChain* swapChain)
{
    if (swapChain == nullptr)
    {
        return false;
    }

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2dFactory.GetAddressOf());
    if (FAILED(hr))
    {
        return false;
    }

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
    if (FAILED(hr))
    {
        return false;
    }

    return CreateSizeDependentResources(swapChain);
}

void TextRenderer::Shutdown()
{
    m_formatCache.clear();
    m_brush.Reset();
    m_renderTarget.Reset();
    m_dwriteFactory.Reset();
    m_d2dFactory.Reset();
}

void TextRenderer::ReleaseSizeDependentResources()
{
    // 백버퍼를 참조하고 있는 D2D 렌더타겟을 먼저 해제해야
    // 스왑체인의 ResizeBuffers가 성공한다.
    m_brush.Reset();
    m_renderTarget.Reset();
}

bool TextRenderer::CreateSizeDependentResources(IDXGISwapChain* swapChain)
{
    if (!m_d2dFactory || swapChain == nullptr)
    {
        return false;
    }

    ComPtr<IDXGISurface> backBufferSurface;
    HRESULT hr = swapChain->GetBuffer(0, __uuidof(IDXGISurface),
                                      reinterpret_cast<void**>(backBufferSurface.GetAddressOf()));
    if (FAILED(hr))
    {
        return false;
    }

    // 백버퍼 포맷(B8G8R8A8)과 동일하게 맞춰야 D2D 렌더타겟 생성이 가능하다.
    // 알파 모드는 드라이버/스왑체인에 따라 지원 여부가 달라 두 가지를 순서대로 시도한다.
    const D2D1_ALPHA_MODE alphaModes[] = { D2D1_ALPHA_MODE_PREMULTIPLIED, D2D1_ALPHA_MODE_IGNORE };

    for (D2D1_ALPHA_MODE alphaMode : alphaModes)
    {
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, alphaMode),
            96.0f, 96.0f);

        hr = m_d2dFactory->CreateDxgiSurfaceRenderTarget(backBufferSurface.Get(), &props, &m_renderTarget);
        if (SUCCEEDED(hr))
        {
            break;
        }
    }

    if (!m_renderTarget)
    {
        return false;
    }

    m_renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_brush);
    if (FAILED(hr))
    {
        m_renderTarget.Reset();
        return false;
    }

    return true;
}

void TextRenderer::BeginDraw()
{
    if (!m_renderTarget || m_drawing)
    {
        return;
    }

    m_renderTarget->BeginDraw();
    m_drawing = true;
}

void TextRenderer::EndDraw()
{
    if (!m_renderTarget || !m_drawing)
    {
        return;
    }

    m_renderTarget->EndDraw();
    m_drawing = false;
}

IDWriteTextFormat* TextRenderer::GetTextFormat(float fontSize, bool bold)
{
    if (!m_dwriteFactory)
    {
        return nullptr;
    }

    for (const FormatCacheEntry& entry : m_formatCache)
    {
        if (entry.bold == bold && entry.fontSize == fontSize)
        {
            return entry.format.Get();
        }
    }

    ComPtr<IDWriteTextFormat> format;
    HRESULT hr = m_dwriteFactory->CreateTextFormat(
        kFontFamily,
        nullptr,
        bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSize,
        kLocale,
        &format);

    if (FAILED(hr))
    {
        return nullptr;
    }

    m_formatCache.push_back({ fontSize, bold, format });
    return m_formatCache.back().format.Get();
}

TextRenderer::ComPtr<IDWriteTextLayout> TextRenderer::CreateLayout(const std::wstring& text, float fontSize,
                                                                   bool bold, float maxWidth, float maxHeight)
{
    ComPtr<IDWriteTextLayout> layout;

    IDWriteTextFormat* format = GetTextFormat(fontSize, bold);
    if (format == nullptr || !m_dwriteFactory)
    {
        return layout;
    }

    m_dwriteFactory->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.length()),
        format,
        maxWidth,
        maxHeight,
        &layout);

    return layout;
}

void TextRenderer::DrawLayout(IDWriteTextLayout* layout, float x, float y, const D2D1_COLOR_F& color)
{
    if (!m_renderTarget || !m_brush || layout == nullptr || !m_drawing)
    {
        return;
    }

    m_brush->SetColor(color);
    m_renderTarget->DrawTextLayout(D2D1::Point2F(x, y), layout, m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
}

float TextRenderer::GetScreenWidth() const
{
    if (!m_renderTarget)
    {
        return 0.0f;
    }
    return m_renderTarget->GetSize().width;
}

float TextRenderer::GetScreenHeight() const
{
    if (!m_renderTarget)
    {
        return 0.0f;
    }
    return m_renderTarget->GetSize().height;
}
