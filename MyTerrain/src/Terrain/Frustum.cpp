#include "Frustum.h"
#include <cmath>

using namespace DirectX;

namespace
{
    XMVECTOR NormalizePlane(XMVECTOR plane)
    {
        const float lengthSq = XMVectorGetX(XMVector3LengthSq(plane));
        // 이론상 0이 나올 일은 없지만(투영 행렬이 항상 정상적이므로), 방어적으로 그대로 돌려준다.
        if (lengthSq < 1e-12f)
        {
            return plane;
        }
        return XMVectorScale(plane, 1.0f / std::sqrt(lengthSq));
    }
}

void Frustum::ExtractFromViewProjection(const XMMATRIX& viewProj)
{
    // 이 프로젝트는 v' = v * M (행벡터) 규약을 쓴다. 표준 Gribb-Hartmann 유도는
    // 열벡터(v' = M * v) 규약에서 "M 의 행"을 더하고 빼는 방식으로 적혀 있는데,
    // 행벡터 규약에서는 그 자리에 "M 의 열"이 와야 한다. 한 번 전치해서
    // M 의 각 열을 mt.r[i] 로 바로 꺼낼 수 있게 만든다.
    const XMMATRIX mt = XMMatrixTranspose(viewProj);
    const XMVECTOR col0 = mt.r[0];
    const XMVECTOR col1 = mt.r[1];
    const XMVECTOR col2 = mt.r[2];
    const XMVECTOR col3 = mt.r[3];

    XMVECTOR planes[Count];
    planes[Left] = XMVectorAdd(col3, col0);
    planes[Right] = XMVectorSubtract(col3, col0);
    planes[Bottom] = XMVectorAdd(col3, col1);
    planes[Top] = XMVectorSubtract(col3, col1);

    // D3D 깊이 범위는 [0, w] 이므로(OpenGL 의 [-w, w] 와 다르다) near 는 col2 자체이고,
    // far 는 col3 - col2 이다 (OpenGL 이었다면 near = col3+col2, far = col3-col2).
    planes[Near] = col2;
    planes[Far] = XMVectorSubtract(col3, col2);

    for (int i = 0; i < Count; ++i)
    {
        XMStoreFloat4(&m_planes[i], NormalizePlane(planes[i]));
    }
}

bool Frustum::IntersectsAABB(const XMFLOAT3& boundsMin, const XMFLOAT3& boundsMax) const
{
    for (int i = 0; i < Count; ++i)
    {
        const XMFLOAT4& plane = m_planes[i];

        // p-vertex : 평면 법선 방향으로 가장 멀리 있는 AABB 꼭짓점.
        // 이 점조차 평면 안쪽이 아니면(거리 < 0) 박스 전체가 이 평면 바깥에 있다.
        const float px = (plane.x >= 0.0f) ? boundsMax.x : boundsMin.x;
        const float py = (plane.y >= 0.0f) ? boundsMax.y : boundsMin.y;
        const float pz = (plane.z >= 0.0f) ? boundsMax.z : boundsMin.z;

        const float distance = plane.x * px + plane.y * py + plane.z * pz + plane.w;
        if (distance < 0.0f)
        {
            return false;
        }
    }
    return true;
}
