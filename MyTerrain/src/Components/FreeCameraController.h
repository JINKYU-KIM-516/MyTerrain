#pragma once
#include "../GameObject/Component.h"
#include <DirectXMath.h>

// 자유 비행(FPS 형) 카메라 조작 컴포넌트.
//
//   W / S           앞뒤 이동
//   A / D           좌우 이동
//   E / Q           위 / 아래 이동 (월드 Y축 기준)
//   마우스 우클릭 드래그   시점 회전
//   마우스 휠        이동 속도 조절
//   Shift           가속 (누르고 있는 동안)
//   R               처음 위치/각도로 복귀
//
// Transform 의 오일러 회전(pitch = x, yaw = y)을 직접 조작한다.
class FreeCameraController : public Component
{
public:
    void Start() override;
    void Update(float deltaTime) override;

    void  SetMoveSpeed(float speed) { m_moveSpeed = speed; }
    float GetMoveSpeed() const { return m_moveSpeed; }

    void SetLookSensitivity(float sensitivity) { m_lookSensitivity = sensitivity; }
    void SetBoostMultiplier(float multiplier) { m_boostMultiplier = multiplier; }

private:
    float m_moveSpeed = 20.0f;         // 초당 이동 거리
    float m_lookSensitivity = 0.15f;   // 마우스 1픽셀당 회전 각도(도)
    float m_boostMultiplier = 4.0f;    // Shift 를 누르고 있을 때 배속

    float m_minSpeed = 1.0f;
    float m_maxSpeed = 500.0f;
    float m_maxPitch = 89.0f;          // 위아래로 넘어가지 않도록 제한

    // R 키로 되돌아갈 초기 상태
    DirectX::XMFLOAT3 m_initialPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_initialRotation{ 0.0f, 0.0f, 0.0f };
    float m_initialMoveSpeed = 20.0f;
    bool  m_initialStateStored = false;
};
