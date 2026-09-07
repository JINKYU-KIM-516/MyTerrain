#pragma once
#include "UIText.h"
#include <functional>

// 클릭 가능한 텍스트 버튼.
// UIText를 그대로 사용하되, 텍스트 영역 위에 마우스가 올라오면 밑줄로 강조하고
// 왼쪽 버튼을 누르는 순간 등록된 콜백을 호출한다.
// (텍스트 색은 사양대로 항상 하얀색을 유지한다)
class UIButton : public UIText
{
public:
    using ClickCallback = std::function<void()>;

    void Update(float deltaTime) override;

    void SetOnClick(const ClickCallback& callback) { m_onClick = callback; }

    bool IsHovered() const { return m_hovered; }

    // 클릭 판정 영역을 텍스트 주변으로 조금 넓혀준다 (픽셀)
    void SetPadding(float x, float y) { m_paddingX = x; m_paddingY = y; }

private:
    ClickCallback m_onClick;
    bool m_hovered = false;
    float m_paddingX = 8.0f;
    float m_paddingY = 4.0f;
};
