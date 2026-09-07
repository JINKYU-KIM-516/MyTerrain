#include "FreeCameraController.h"
#include "../Framework/InputManager.h"
#include "../GameObject/GameObject.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

void FreeCameraController::Start()
{
    GameObject* owner = GetGameObject();
    if (owner == nullptr || owner->GetTransform() == nullptr)
    {
        return;
    }

    // 씬을 구성할 때 지정해둔 위치/각도를 "초기 상태"로 기억해둔다 (R 키 복귀용)
    m_initialPosition = owner->GetTransform()->GetPosition();
    m_initialRotation = owner->GetTransform()->GetRotation();
    m_initialMoveSpeed = m_moveSpeed;
    m_initialStateStored = true;
}

void FreeCameraController::Update(float deltaTime)
{
    GameObject* owner = GetGameObject();
    if (owner == nullptr || owner->GetTransform() == nullptr)
    {
        return;
    }

    Transform* transform = owner->GetTransform();
    InputManager& input = InputManager::GetInstance();

    // ---------------- R: 초기 상태로 복귀 ----------------
    if (m_initialStateStored && input.IsKeyPressed('R'))
    {
        transform->SetPosition(m_initialPosition);
        transform->SetRotation(m_initialRotation);
        m_moveSpeed = m_initialMoveSpeed;
        return;
    }

    // ---------------- 시점 회전 (마우스 우클릭 드래그) ----------------
    if (input.IsMouseButtonDown(MouseButton::Right))
    {
        const float deltaYaw = static_cast<float>(input.GetMouseDeltaX()) * m_lookSensitivity;
        const float deltaPitch = static_cast<float>(input.GetMouseDeltaY()) * m_lookSensitivity;

        XMFLOAT3 rotation = transform->GetRotation();
        rotation.y += deltaYaw;                 // 좌우 = yaw
        rotation.x += deltaPitch;               // 상하 = pitch (마우스를 내리면 아래를 본다)
        rotation.x = std::clamp(rotation.x, -m_maxPitch, m_maxPitch);

        // yaw 가 무한정 커지지 않도록 -180 ~ 180 범위로 정리
        if (rotation.y > 180.0f)       rotation.y -= 360.0f;
        else if (rotation.y < -180.0f) rotation.y += 360.0f;

        transform->SetRotation(rotation);
    }

    // ---------------- 휠: 이동 속도 조절 ----------------
    const int wheel = input.GetMouseWheelDelta();
    if (wheel != 0)
    {
        m_moveSpeed *= std::pow(1.15f, static_cast<float>(wheel));
        m_moveSpeed = std::clamp(m_moveSpeed, m_minSpeed, m_maxSpeed);
    }

    // ---------------- 이동 ----------------
    const XMFLOAT3 forwardF = transform->GetForward();
    const XMFLOAT3 rightF = transform->GetRight();

    XMVECTOR move = XMVectorZero();

    if (input.IsKeyDown('W')) move = XMVectorAdd(move, XMLoadFloat3(&forwardF));
    if (input.IsKeyDown('S')) move = XMVectorSubtract(move, XMLoadFloat3(&forwardF));
    if (input.IsKeyDown('D')) move = XMVectorAdd(move, XMLoadFloat3(&rightF));
    if (input.IsKeyDown('A')) move = XMVectorSubtract(move, XMLoadFloat3(&rightF));

    // 위/아래는 화면 기울기와 상관없이 항상 월드 Y축으로 움직이는 편이 조작하기 쉽다
    if (input.IsKeyDown('E')) move = XMVectorAdd(move, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    if (input.IsKeyDown('Q')) move = XMVectorSubtract(move, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    if (XMVectorGetX(XMVector3LengthSq(move)) > 0.0001f)
    {
        // 대각선 이동이 더 빨라지지 않도록 정규화
        move = XMVector3Normalize(move);

        float speed = m_moveSpeed;
        if (input.IsKeyDown(VK_SHIFT))
        {
            speed *= m_boostMultiplier;
        }

        const XMFLOAT3 positionF = transform->GetPosition();
        XMVECTOR position = XMLoadFloat3(&positionF);
        position = XMVectorAdd(position, XMVectorScale(move, speed * deltaTime));

        XMFLOAT3 result;
        XMStoreFloat3(&result, position);
        transform->SetPosition(result);
    }
}
