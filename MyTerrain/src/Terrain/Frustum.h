#pragma once
#include <DirectXMath.h>

// 뷰-프로젝션 행렬에서 6개의 절두체(frustum) 평면을 뽑아내고, AABB 와의 교차를 검사한다.
// 5번 쿼드트리 컬링 기법에서만 쓴다 (그 전 기법들은 이 클래스를 아예 참조하지 않는다).
//
// 이 프로젝트의 행렬 규약(Camera.cpp, TerrainRenderer::UpdateConstantBuffer 참고)에
// 맞춰 유도했다:
//   - CPU 는 행벡터 규약을 쓴다: v' = v * M (열벡터 규약인 v' = M * v 가 아니다)
//   - Camera::GetViewProjectionMatrix() 는 GetViewMatrix() * GetProjectionMatrix() 순서로
//     곱한 행렬을 돌려주는데, 이것이 바로 v * View * Proj 순서로 적용되는 행벡터용 행렬이다.
//     (GPU 로 올리기 직전에 전치하는 것은 HLSL 의 기본 열 우선 규약에 맞추기 위해서일 뿐,
//      Frustum 은 그 "전치되지 않은" 원래 행렬을 그대로 받는다)
//   - XMMatrixPerspectiveFovLH 를 쓰므로 왼손 좌표계이고, D3D 깊이 범위는 [0, w] 이다
//     (OpenGL 의 [-w, w] 와 달라서 near/far 평면 유도식이 다르다)
//
// 평면은 ax + by + cz + d = 0 형태로 저장하며, 점 p 가 평면 안쪽(절두체 쪽)이면
// a*p.x + b*p.y + c*p.z + d >= 0 이 되도록 정규화한다.
class Frustum
{
public:
    enum Side
    {
        Left = 0,
        Right,
        Bottom,
        Top,
        Near,
        Far,
        Count
    };

    // viewProj 는 Camera::GetViewProjectionMatrix() 가 돌려주는 행렬을 그대로 넘기면 된다
    // (GPU 업로드용으로 전치한 행렬이 아니라 CPU 쪽 원본 행렬이어야 한다).
    void ExtractFromViewProjection(const DirectX::XMMATRIX& viewProj);

    // AABB 가 절두체와 조금이라도 겹치면 true, 완전히 바깥이면 false.
    // 6평면 p-vertex 판정이라 보수적(conservative)이다 -- 실제로는 안 겹치는데 true 가
    // 나올 수는 있어도(교차하지 않는 걸 교차한다고 오판) 그 반대는 없으므로 컬링에 안전하게 쓸 수 있다.
    bool IntersectsAABB(const DirectX::XMFLOAT3& boundsMin, const DirectX::XMFLOAT3& boundsMax) const;

    const DirectX::XMFLOAT4& GetPlane(Side side) const { return m_planes[side]; }

private:
    DirectX::XMFLOAT4 m_planes[Count]{};
};
