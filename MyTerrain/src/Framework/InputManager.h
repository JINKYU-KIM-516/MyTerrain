#pragma once
#include <windows.h>
#include <array>

// 마우스 버튼 구분용
enum class MouseButton
{
    Left = 0,
    Right,
    Middle,
    Count
};

// 키보드 / 마우스 입력을 관리하는 싱글턴 클래스
// Window의 WndProc에서 전달받은 메시지를 처리하여 상태를 갱신하고,
// 각 GameObject/Component 에서는 이 클래스를 통해 입력 상태를 조회한다.
class InputManager
{
public:
    static InputManager& GetInstance();

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // 매 프레임 시작 시 1회 호출 (이전 프레임 상태 갱신, 휠/이동량 초기화)
    void BeginFrame();

    // Window의 WndProc 에서 호출해준다
    void HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    // ---------------- 키보드 ----------------
    bool IsKeyDown(int virtualKeyCode) const;      // 현재 프레임에 눌려있는가
    bool IsKeyPressed(int virtualKeyCode) const;    // 이번 프레임에 처음 눌렸는가
    bool IsKeyReleased(int virtualKeyCode) const;   // 이번 프레임에 처음 떼어졌는가

    // ---------------- 마우스 ----------------
    bool IsMouseButtonDown(MouseButton button) const;
    bool IsMouseButtonPressed(MouseButton button) const;
    bool IsMouseButtonReleased(MouseButton button) const;

    long GetMouseX() const { return m_mouseX; }
    long GetMouseY() const { return m_mouseY; }
    long GetMouseDeltaX() const { return m_mouseDeltaX; }
    long GetMouseDeltaY() const { return m_mouseDeltaY; }
    int  GetMouseWheelDelta() const { return m_mouseWheelDelta; }

private:
    InputManager() = default;

    static constexpr int kKeyCount = 256;
    static constexpr int kMouseButtonCount = static_cast<int>(MouseButton::Count);

    std::array<bool, kKeyCount> m_currKeys{};
    std::array<bool, kKeyCount> m_prevKeys{};

    std::array<bool, kMouseButtonCount> m_currMouseButtons{};
    std::array<bool, kMouseButtonCount> m_prevMouseButtons{};

    long m_mouseX = 0;
    long m_mouseY = 0;
    long m_mouseDeltaX = 0;
    long m_mouseDeltaY = 0;
    int  m_mouseWheelDelta = 0;
};
