#include "Transform.h"
#include <algorithm>

using namespace DirectX;

void Transform::Translate(const XMFLOAT3& delta)
{
    m_position.x += delta.x;
    m_position.y += delta.y;
    m_position.z += delta.z;
}

void Transform::Rotate(const XMFLOAT3& deltaDegrees)
{
    m_rotationEuler.x += deltaDegrees.x;
    m_rotationEuler.y += deltaDegrees.y;
    m_rotationEuler.z += deltaDegrees.z;
}

XMFLOAT3 Transform::GetForward() const
{
    XMVECTOR rotQuat = XMQuaternionRotationRollPitchYaw(
        XMConvertToRadians(m_rotationEuler.x),
        XMConvertToRadians(m_rotationEuler.y),
        XMConvertToRadians(m_rotationEuler.z));

    XMVECTOR forward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotQuat);

    XMFLOAT3 result;
    XMStoreFloat3(&result, forward);
    return result;
}

XMFLOAT3 Transform::GetRight() const
{
    XMVECTOR rotQuat = XMQuaternionRotationRollPitchYaw(
        XMConvertToRadians(m_rotationEuler.x),
        XMConvertToRadians(m_rotationEuler.y),
        XMConvertToRadians(m_rotationEuler.z));

    XMVECTOR right = XMVector3Rotate(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), rotQuat);

    XMFLOAT3 result;
    XMStoreFloat3(&result, right);
    return result;
}

XMFLOAT3 Transform::GetUp() const
{
    XMVECTOR rotQuat = XMQuaternionRotationRollPitchYaw(
        XMConvertToRadians(m_rotationEuler.x),
        XMConvertToRadians(m_rotationEuler.y),
        XMConvertToRadians(m_rotationEuler.z));

    XMVECTOR up = XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotQuat);

    XMFLOAT3 result;
    XMStoreFloat3(&result, up);
    return result;
}

XMMATRIX Transform::GetLocalMatrix() const
{
    XMMATRIX scaleMat = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);

    XMMATRIX rotMat = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(m_rotationEuler.x),
        XMConvertToRadians(m_rotationEuler.y),
        XMConvertToRadians(m_rotationEuler.z));

    XMMATRIX transMat = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);

    return scaleMat * rotMat * transMat;
}

XMMATRIX Transform::GetWorldMatrix() const
{
    XMMATRIX local = GetLocalMatrix();

    if (m_parent)
    {
        return local * m_parent->GetWorldMatrix();
    }

    return local;
}

void Transform::SetParent(Transform* parent)
{
    if (m_parent == parent)
    {
        return;
    }

    if (m_parent)
    {
        m_parent->RemoveChild(this);
    }

    m_parent = parent;

    if (m_parent)
    {
        m_parent->AddChild(this);
    }
}

void Transform::AddChild(Transform* child)
{
    m_children.push_back(child);
}

void Transform::RemoveChild(Transform* child)
{
    m_children.erase(std::remove(m_children.begin(), m_children.end(), child), m_children.end());
}
