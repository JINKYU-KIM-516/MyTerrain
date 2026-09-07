#include "InputManager.h"
#include <windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM

InputManager& InputManager::GetInstance()
{
    static InputManager instance;
    return instance;
}

void InputManager::BeginFrame()
{
    // 이전 프레임 상태 저장
    m_prevKeys = m_currKeys;
    m_prevMouseButtons = m_currMouseButtons;

    // 프레임 단위 누적값 초기화 (WM_MOUSEMOVE / WM_MOUSEWHEEL 로 다시 채워짐)
    m_mouseDeltaX = 0;
    m_mouseDeltaY = 0;
    m_mouseWheelDelta = 0;
}

void InputManager::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wParam < static_cast<WPARAM>(kKeyCount))
        {
            m_currKeys[wParam] = true;
        }
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wParam < static_cast<WPARAM>(kKeyCount))
        {
            m_currKeys[wParam] = false;
        }
        break;

    case WM_LBUTTONDOWN:
        m_currMouseButtons[static_cast<int>(MouseButton::Left)] = true;
        break;
    case WM_LBUTTONUP:
        m_currMouseButtons[static_cast<int>(MouseButton::Left)] = false;
        break;

    case WM_RBUTTONDOWN:
        m_currMouseButtons[static_cast<int>(MouseButton::Right)] = true;
        break;
    case WM_RBUTTONUP:
        m_currMouseButtons[static_cast<int>(MouseButton::Right)] = false;
        break;

    case WM_MBUTTONDOWN:
        m_currMouseButtons[static_cast<int>(MouseButton::Middle)] = true;
        break;
    case WM_MBUTTONUP:
        m_currMouseButtons[static_cast<int>(MouseButton::Middle)] = false;
        break;

    case WM_MOUSEMOVE:
    {
        const long newX = GET_X_LPARAM(lParam);
        const long newY = GET_Y_LPARAM(lParam);

        m_mouseDeltaX += (newX - m_mouseX);
        m_mouseDeltaY += (newY - m_mouseY);

        m_mouseX = newX;
        m_mouseY = newY;
        break;
    }

    case WM_MOUSEWHEEL:
        m_mouseWheelDelta += GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        break;

    default:
        break;
    }
}

bool InputManager::IsKeyDown(int virtualKeyCode) const
{
    if (virtualKeyCode < 0 || virtualKeyCode >= kKeyCount) return false;
    return m_currKeys[virtualKeyCode];
}

bool InputManager::IsKeyPressed(int virtualKeyCode) const
{
    if (virtualKeyCode < 0 || virtualKeyCode >= kKeyCount) return false;
    return m_currKeys[virtualKeyCode] && !m_prevKeys[virtualKeyCode];
}

bool InputManager::IsKeyReleased(int virtualKeyCode) const
{
    if (virtualKeyCode < 0 || virtualKeyCode >= kKeyCount) return false;
    return !m_currKeys[virtualKeyCode] && m_prevKeys[virtualKeyCode];
}

bool InputManager::IsMouseButtonDown(MouseButton button) const
{
    const int idx = static_cast<int>(button);
    if (idx < 0 || idx >= kMouseButtonCount) return false;
    return m_currMouseButtons[idx];
}

bool InputManager::IsMouseButtonPressed(MouseButton button) const
{
    const int idx = static_cast<int>(button);
    if (idx < 0 || idx >= kMouseButtonCount) return false;
    return m_currMouseButtons[idx] && !m_prevMouseButtons[idx];
}

bool InputManager::IsMouseButtonReleased(MouseButton button) const
{
    const int idx = static_cast<int>(button);
    if (idx < 0 || idx >= kMouseButtonCount) return false;
    return !m_currMouseButtons[idx] && m_prevMouseButtons[idx];
}
