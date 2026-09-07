#pragma once
#include "../GameObject/Component.h"
#include <functional>

// 특정 키를 누르는 순간 등록된 동작을 실행하는 간단한 컴포넌트.
// (예: 메뉴 화면에서 ESC -> 프로그램 종료 / 기법 화면에서 ESC -> 메뉴로 이동)
class KeyCommandComponent : public Component
{
public:
    using Action = std::function<void()>;

    KeyCommandComponent() = default;
    KeyCommandComponent(int virtualKeyCode, const Action& action)
        : m_virtualKeyCode(virtualKeyCode), m_action(action) {}

    void SetKey(int virtualKeyCode) { m_virtualKeyCode = virtualKeyCode; }
    void SetAction(const Action& action) { m_action = action; }

    void Update(float deltaTime) override;

private:
    int m_virtualKeyCode = 0;
    Action m_action;
};
