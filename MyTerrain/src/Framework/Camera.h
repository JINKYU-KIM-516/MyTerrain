#pragma once
#include "../GameObject/Component.h"
#include <DirectXMath.h>

// 3D 카메라 컴포넌트.
//
// 위치/회전은 GameObject 의 Transform 을 그대로 사용하고,
// 여기서는 시야각/근평면/원평면과 뷰·투영 행렬 계산만 담당한다.
//
// 씬에 처음 만들어진 카메라가 자동으로 "메인 카메라"가 되며,
// 지형 렌더러 등은 Camera::GetMain() 으로 행렬을 가져다 쓴다.
class Camera : public Component
{
public:
    void Start() override;
    void Destroy() override;

    // 이 카메라를 메인 카메라로 지정한다
    void SetAsMain();

    static Camera* GetMain() { return s_main; }

    // ---------------- 행렬 ----------------
    DirectX::XMMATRIX GetViewMatrix() const;
    DirectX::XMMATRIX GetProjectionMatrix() const;
    DirectX::XMMATRIX GetViewProjectionMatrix() const;

    // ---------------- 설정 ----------------
    void  SetFieldOfView(float degrees) { m_fovDegrees = degrees; }
    float GetFieldOfView() const { return m_fovDegrees; }

    void  SetClipPlanes(float nearZ, float farZ) { m_nearZ = nearZ; m_farZ = farZ; }
    float GetNearZ() const { return m_nearZ; }
    float GetFarZ() const { return m_farZ; }

    // 백버퍼 크기로부터 계산된 화면 종횡비 (렌더러가 없으면 16:9)
    float GetAspectRatio() const;

    DirectX::XMFLOAT3 GetWorldPosition() const;

private:
    float m_fovDegrees = 60.0f;
    float m_nearZ = 0.1f;
    float m_farZ = 2000.0f;

    static Camera* s_main;
};
