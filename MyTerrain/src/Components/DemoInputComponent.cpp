#include "DemoInputComponent.h"
#include "../GameObject/GameObject.h"
#include "../Framework/InputManager.h"
#include <windows.h>
#include <cstdio>

void DemoInputComponent::Start()
{
    char buffer[128];
    sprintf_s(buffer, "[DemoInputComponent] %s Start() 호출됨\n", GetGameObject()->GetName().c_str());
    OutputDebugStringA(buffer);
}

void DemoInputComponent::Update(float deltaTime)
{
    InputManager& input = InputManager::GetInstance();
    Transform* transform = GetGameObject()->GetTransform();

    // ---- 키보드: WASD / 방향키로 이동 ----
    DirectX::XMFLOAT3 move{ 0.0f, 0.0f, 0.0f };

    if (input.IsKeyDown('W') || input.IsKeyDown(VK_UP))    move.z += 1.0f;
    if (input.IsKeyDown('S') || input.IsKeyDown(VK_DOWN))  move.z -= 1.0f;
    if (input.IsKeyDown('D') || input.IsKeyDown(VK_RIGHT)) move.x += 1.0f;
    if (input.IsKeyDown('A') || input.IsKeyDown(VK_LEFT))  move.x -= 1.0f;
    if (input.IsKeyDown('E'))                              move.y += 1.0f;
    if (input.IsKeyDown('Q'))                              move.y -= 1.0f;

    if (move.x != 0.0f || move.y != 0.0f || move.z != 0.0f)
    {
        transform->Translate({
            move.x * m_moveSpeed * deltaTime,
            move.y * m_moveSpeed * deltaTime,
            move.z * m_moveSpeed * deltaTime });
    }

    // ---- 마우스: 왼쪽 버튼을 누른 채 이동하면 회전 ----
    if (input.IsMouseButtonDown(MouseButton::Left))
    {
        const float dx = static_cast<float>(input.GetMouseDeltaX());
        const float dy = static_cast<float>(input.GetMouseDeltaY());

        transform->Rotate({ dy * m_rotateSpeed, dx * m_rotateSpeed, 0.0f });
    }

    // 스페이스바를 누르는 순간(눌리는 첫 프레임)에만 디버그 로그 출력
    if (input.IsKeyPressed(VK_SPACE))
    {
        OutputDebugStringA("[DemoInputComponent] Space 눌림!\n");
    }
}

void DemoInputComponent::Destroy()
{
    OutputDebugStringA("[DemoInputComponent] Destroy() 호출됨\n");
}
