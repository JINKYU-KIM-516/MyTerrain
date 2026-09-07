#include "UIText.h"
#include "../Framework/Framework.h"

void UIText::SetText(const std::wstring& text)
{
    if (m_text != text)
    {
        m_text = text;
        m_layoutDirty = true;
    }
}

void UIText::SetFontSize(float size)
{
    if (m_fontSize != size)
    {
        m_fontSize = size;
        m_layoutDirty = true;
    }
}

void UIText::SetBold(bool bold)
{
    if (m_bold != bold)
    {
        m_bold = bold;
        m_layoutDirty = true;
    }
}

void UIText::SetUnderline(bool underline)
{
    if (m_underline == underline)
    {
        return;
    }

    m_underline = underline;

    if (m_layout)
    {
        DWRITE_TEXT_RANGE range{ 0, static_cast<UINT32>(m_text.length()) };
        m_layout->SetUnderline(m_underline ? TRUE : FALSE, range);
    }
}

void UIText::EnsureLayout()
{
    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return;
    }

    TextRenderer& textRenderer = framework->GetTextRenderer();
    if (!textRenderer.IsReady())
    {
        // 창 크기 변경 중에는 렌더타겟이 잠시 없을 수 있다
        return;
    }

    if (m_layoutDirty || !m_layout)
    {
        m_layout = textRenderer.CreateLayout(m_text, m_fontSize, m_bold);
        m_layoutDirty = false;

        if (m_layout)
        {
            DWRITE_TEXT_RANGE range{ 0, static_cast<UINT32>(m_text.length()) };
            m_layout->SetUnderline(m_underline ? TRUE : FALSE, range);

            DWRITE_TEXT_METRICS metrics{};
            if (SUCCEEDED(m_layout->GetMetrics(&metrics)))
            {
                m_textWidth = metrics.width;
                m_textHeight = metrics.height;
            }
        }
    }
}

void UIText::RefreshLayout()
{
    EnsureLayout();

    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return;
    }

    TextRenderer& textRenderer = framework->GetTextRenderer();
    const float screenWidth = textRenderer.GetScreenWidth();
    const float screenHeight = textRenderer.GetScreenHeight();

    float left = 0.0f;
    float top = m_offsetY;

    switch (m_anchor)
    {
    case UIAnchor::TopLeft:
        left = m_offsetX;
        break;

    case UIAnchor::TopCenter:
        left = (screenWidth - m_textWidth) * 0.5f + m_offsetX;
        break;

    case UIAnchor::TopRight:
        // 오프셋 X는 화면 오른쪽 끝에서 떨어진 여백으로 해석한다
        left = screenWidth - m_textWidth - m_offsetX;
        break;

    case UIAnchor::Center:
        left = (screenWidth - m_textWidth) * 0.5f + m_offsetX;
        top = (screenHeight - m_textHeight) * 0.5f + m_offsetY;
        break;

    // 아래쪽 앵커는 오프셋 Y를 "화면 아래쪽 끝에서 떨어진 여백"으로 해석한다
    case UIAnchor::BottomLeft:
        left = m_offsetX;
        top = screenHeight - m_textHeight - m_offsetY;
        break;

    case UIAnchor::BottomCenter:
        left = (screenWidth - m_textWidth) * 0.5f + m_offsetX;
        top = screenHeight - m_textHeight - m_offsetY;
        break;

    case UIAnchor::BottomRight:
        left = screenWidth - m_textWidth - m_offsetX;
        top = screenHeight - m_textHeight - m_offsetY;
        break;
    }

    m_screenRect = D2D1::RectF(left, top, left + m_textWidth, top + m_textHeight);
}

void UIText::Update(float deltaTime)
{
    (void)deltaTime;
    RefreshLayout();
}

void UIText::RenderUI()
{
    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return;
    }

    // 씬이 만들어진 첫 프레임에는 Update보다 Render가 먼저 올 수 있으므로 한 번 더 보정
    if (!m_layout || m_layoutDirty)
    {
        RefreshLayout();
    }

    framework->GetTextRenderer().DrawLayout(m_layout.Get(), m_screenRect.left, m_screenRect.top, m_color);
}
