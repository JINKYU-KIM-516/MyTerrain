#pragma once
#include "Component.h"
#include <DirectXMath.h>
#include <vector>

// 모든 GameObject가 기본적으로 갖는 컴포넌트.
// 위치/회전(오일러, degree)/스케일과 부모-자식 계층 구조를 관리한다.
class Transform : public Component
{
public:
    Transform() = default;

    // ---------------- Position ----------------
    const DirectX::XMFLOAT3& GetPosition() const { return m_position; }
    void SetPosition(const DirectX::XMFLOAT3& position) { m_position = position; }
    void SetPosition(float x, float y, float z) { m_position = { x, y, z }; }
    void Translate(const DirectX::XMFLOAT3& delta);

    // ---------------- Rotation (Euler, degree) ----------------
    const DirectX::XMFLOAT3& GetRotation() const { return m_rotationEuler; }
    void SetRotation(const DirectX::XMFLOAT3& eulerDegrees) { m_rotationEuler = eulerDegrees; }
    void SetRotation(float pitch, float yaw, float roll) { m_rotationEuler = { pitch, yaw, roll }; }
    void Rotate(const DirectX::XMFLOAT3& deltaDegrees);

    // ---------------- Scale ----------------
    const DirectX::XMFLOAT3& GetScale() const { return m_scale; }
    void SetScale(const DirectX::XMFLOAT3& scale) { m_scale = scale; }
    void SetScale(float x, float y, float z) { m_scale = { x, y, z }; }
    void SetScale(float uniform) { m_scale = { uniform, uniform, uniform }; }

    // ---------------- 방향 벡터 ----------------
    DirectX::XMFLOAT3 GetForward() const;
    DirectX::XMFLOAT3 GetRight() const;
    DirectX::XMFLOAT3 GetUp() const;

    // ---------------- 행렬 ----------------
    DirectX::XMMATRIX GetLocalMatrix() const;
    DirectX::XMMATRIX GetWorldMatrix() const; // 부모 계층까지 반영한 월드 행렬

    // ---------------- 계층 구조 (부모/자식) ----------------
    void SetParent(Transform* parent);
    Transform* GetParent() const { return m_parent; }
    const std::vector<Transform*>& GetChildren() const { return m_children; }

private:
    void AddChild(Transform* child);
    void RemoveChild(Transform* child);

private:
    DirectX::XMFLOAT3 m_position{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_rotationEuler{ 0.0f, 0.0f, 0.0f }; // pitch(x), yaw(y), roll(z) - degree
    DirectX::XMFLOAT3 m_scale{ 1.0f, 1.0f, 1.0f };

    Transform* m_parent = nullptr;
    std::vector<Transform*> m_children;
};
