#include "Camera.h"
#include "Framework.h"
#include "../GameObject/GameObject.h"

using namespace DirectX;

Camera* Camera::s_main = nullptr;

void Camera::Start()
{
    // 씬에 메인 카메라가 없으면 자기 자신을 메인으로 등록한다
    if (s_main == nullptr)
    {
        s_main = this;
    }
}

void Camera::Destroy()
{
    if (s_main == this)
    {
        s_main = nullptr;
    }
}

void Camera::SetAsMain()
{
    s_main = this;
}

float Camera::GetAspectRatio() const
{
    Framework* framework = Framework::GetInstance();
    if (framework == nullptr)
    {
        return 16.0f / 9.0f;
    }

    const UINT width = framework->GetRenderer().GetWidth();
    const UINT height = framework->GetRenderer().GetHeight();

    if (width == 0 || height == 0)
    {
        return 16.0f / 9.0f;
    }

    return static_cast<float>(width) / static_cast<float>(height);
}

XMFLOAT3 Camera::GetWorldPosition() const
{
    GameObject* owner = GetGameObject();
    if (owner == nullptr || owner->GetTransform() == nullptr)
    {
        return XMFLOAT3(0.0f, 0.0f, 0.0f);
    }

    // 부모 계층까지 반영된 실제 월드 위치
    const XMMATRIX world = owner->GetTransform()->GetWorldMatrix();

    XMFLOAT3 position;
    XMStoreFloat3(&position, world.r[3]);
    return position;
}

XMMATRIX Camera::GetViewMatrix() const
{
    GameObject* owner = GetGameObject();
    if (owner == nullptr || owner->GetTransform() == nullptr)
    {
        return XMMatrixIdentity();
    }

    Transform* transform = owner->GetTransform();

    const XMFLOAT3 positionF = GetWorldPosition();
    const XMFLOAT3 forwardF = transform->GetForward();
    const XMFLOAT3 upF = transform->GetUp();

    const XMVECTOR position = XMLoadFloat3(&positionF);
    const XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&forwardF));
    const XMVECTOR up = XMVector3Normalize(XMLoadFloat3(&upF));

    // 왼손 좌표계(LH) 기준. "위치에서 forward 방향을 바라보는" 뷰 행렬
    return XMMatrixLookToLH(position, forward, up);
}

XMMATRIX Camera::GetProjectionMatrix() const
{
    return XMMatrixPerspectiveFovLH(
        XMConvertToRadians(m_fovDegrees),
        GetAspectRatio(),
        m_nearZ,
        m_farZ);
}

XMMATRIX Camera::GetViewProjectionMatrix() const
{
    return GetViewMatrix() * GetProjectionMatrix();
}
