#include "KeyCommandComponent.h"
#include "../Framework/InputManager.h"

void KeyCommandComponent::Update(float deltaTime)
{
    (void)deltaTime;

    if (m_virtualKeyCode == 0 || !m_action)
    {
        return;
    }

    if (InputManager::GetInstance().IsKeyPressed(m_virtualKeyCode))
    {
        m_action();
    }
}
