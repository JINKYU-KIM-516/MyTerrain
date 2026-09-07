#include "UIButton.h"
#include "../Framework/InputManager.h"

void UIButton::Update(float deltaTime)
{
    // 먼저 화면상의 위치/크기를 최신화한다
    UIText::Update(deltaTime);

    InputManager& input = InputManager::GetInstance();

    const float mouseX = static_cast<float>(input.GetMouseX());
    const float mouseY = static_cast<float>(input.GetMouseY());

    const bool inside =
        mouseX >= m_screenRect.left - m_paddingX &&
        mouseX <= m_screenRect.right + m_paddingX &&
        mouseY >= m_screenRect.top - m_paddingY &&
        mouseY <= m_screenRect.bottom + m_paddingY;

    if (inside != m_hovered)
    {
        m_hovered = inside;
        SetUnderline(m_hovered);
    }

    if (m_hovered && input.IsMouseButtonPressed(MouseButton::Left))
    {
        if (m_onClick)
        {
            m_onClick();
        }
    }
}
