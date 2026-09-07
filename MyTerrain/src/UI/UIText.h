#pragma once
#include "../GameObject/Component.h"
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>

// UI 요소를 화면의 어느 지점을 기준으로 배치할지 나타낸다.
// 화면 크기가 바뀌어도 기준점이 유지되도록 매 프레임 위치를 다시 계산한다.
enum class UIAnchor
{
    TopLeft,      // 왼쪽 상단
    TopCenter,    // 상단 중앙 (가로 중앙 정렬)
    TopRight,     // 오른쪽 상단
    Center        // 화면 정중앙
};

// 화면에 텍스트 한 줄을 그리는 UI 컴포넌트.
// - 색상 기본값은 하얀색 (배경이 파란색이므로)
// - 앵커 + 오프셋(픽셀)으로 위치를 지정한다
class UIText : public Component
{
public:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    void Update(float deltaTime) override;
    void RenderUI() override;

    // ---------------- 설정 ----------------
    void SetText(const std::wstring& text);
    const std::wstring& GetText() const { return m_text; }

    void SetFontSize(float size);
    void SetBold(bool bold);

    void SetColor(float r, float g, float b, float a = 1.0f) { m_color = D2D1::ColorF(r, g, b, a); }
    void SetColor(const D2D1_COLOR_F& color) { m_color = color; }

    void SetAnchor(UIAnchor anchor) { m_anchor = anchor; }
    void SetOffset(float x, float y) { m_offsetX = x; m_offsetY = y; }

    // 밑줄 (메뉴 항목에 마우스를 올렸을 때 강조용)
    void SetUnderline(bool underline);

    // ---------------- 조회 ----------------
    // 현재 프레임에서 이 텍스트가 차지하는 화면 영역(픽셀). 클릭 판정에 사용한다.
    const D2D1_RECT_F& GetScreenRect() const { return m_screenRect; }

    float GetTextWidth() const { return m_textWidth; }
    float GetTextHeight() const { return m_textHeight; }

    // 위치/크기 정보를 즉시 최신화한다 (Update에서 자동 호출됨)
    void RefreshLayout();

protected:
    void EnsureLayout();

protected:
    std::wstring m_text;
    float m_fontSize = 24.0f;
    bool  m_bold = false;
    bool  m_underline = false;

    D2D1_COLOR_F m_color = D2D1::ColorF(D2D1::ColorF::White);

    UIAnchor m_anchor = UIAnchor::TopCenter;
    float m_offsetX = 0.0f;
    float m_offsetY = 0.0f;

    ComPtr<IDWriteTextLayout> m_layout;
    bool m_layoutDirty = true;

    float m_textWidth = 0.0f;
    float m_textHeight = 0.0f;
    D2D1_RECT_F m_screenRect = { 0.0f, 0.0f, 0.0f, 0.0f };
};
