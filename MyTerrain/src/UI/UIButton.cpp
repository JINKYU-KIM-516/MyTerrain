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

    // 기본은 클릭 순간(Pressed)에 1회만 반응하고, m_repeatWhileHeld 가 켜져 있으면
    // 누르고 있는 동안(Down) 매 프레임 반응한다 -- 키보드를 누르고 있으면 계속
    // 반응하던 조작(값 연속 조절 등)을 버튼으로 옮길 때 쓴다.
    const bool triggered = m_repeatWhileHeld
        ? input.IsMouseButtonDown(MouseButton::Left)
        : input.IsMouseButtonPressed(MouseButton::Left);

    if (m_hovered && triggered)
    {
        if (m_onClick)
        {
            m_onClick();
        }
    }
}
