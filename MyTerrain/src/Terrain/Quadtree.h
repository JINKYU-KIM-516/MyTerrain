#pragma once
#include "GridMesh.h"
#include "Frustum.h"
#include <DirectXMath.h>
#include <vector>
#include <cstdint>

// 지형을 사분면(쿼드트리)으로 나눠, 절두체 밖의 덩어리를 통째로 건너뛰기 위한 모듈.
// 5번 쿼드트리 컬링 기법에서만 쓴다.
//
// 핵심 설계 결정: 정점 버퍼는 지금까지(1~4번)처럼 통째로 하나만 그대로 쓴다.
// 쿼드트리가 하는 일은 "인덱스 버퍼를 리프(leaf)별로 다시 정렬"하는 것뿐이다.
//   - 리프 경계에서 정점이 새로 생기거나 복제되지 않는다 -> 이음매(crack) 문제가
//     원천적으로 생기지 않는다 (진짜 청크마다 별도 메시를 만드는 방식과 다른 점)
//   - 각 리프는 하나의 연속된 인덱스 구간 [indexStart, indexStart+indexCount) 을 갖는다.
//     보이는 리프들만 모아서 그 구간만큼씩 DrawIndexed 를 나눠 부르면 된다.
//
// 분할은 셀(x, z) 좌표 사각형을 가로/세로로 절반씩 재귀적으로 나누는 방식이다.
// 정사각형 격자(예: 256 x 256)에서는 두 축이 항상 같이 갈라져서 정직한 사분할이 되고,
// 직사각형 격자에서는 이미 충분히 작아진 축은 그대로 두고 남은 축만 갈라진다
// (그래도 리프가 지나치게 길쭉해지지 않도록 절반씩 나눈다).
namespace Quadtree
{
    struct AABB
    {
        DirectX::XMFLOAT3 min{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 max{ 0.0f, 0.0f, 0.0f };
    };

    // 리프 하나 = 인덱스 버퍼(재정렬된) 안의 연속된 구간 하나.
    struct Leaf
    {
        AABB bounds;
        uint32_t indexStart = 0;
        uint32_t indexCount = 0;
    };

    // 내부 노드/리프 공용. leafIndex >= 0 이면 리프(자식 없음), 아니면 내부 노드
    // (children 중 -1 이 아닌 것만 유효한 자식).
    struct Node
    {
        AABB bounds;
        int  children[4] = { -1, -1, -1, -1 };
        int  leafIndex = -1;
    };

    struct Tree
    {
        std::vector<Node>     nodes;
        std::vector<Leaf>     leaves;
        std::vector<uint32_t> indices;   // 리프별로 재정렬된 인덱스. GPU 인덱스 버퍼에 그대로 올린다.
        int rootIndex = -1;

        bool IsEmpty() const { return rootIndex < 0 || nodes.empty(); }
    };

    // 절두체 컬링을 돌린 결과 통계 (HUD 표시용).
    struct Stats
    {
        int nodesVisited = 0;
        int leavesVisible = 0;
    };

    // mesh 로부터 쿼드트리를 만든다.
    //   maxLeafCells : 리프 한 변의 최대 셀 수 (이 이하가 되면 더 이상 나누지 않는다).
    //                  작을수록 리프가 잘게 쪼개져 컬링이 세밀해지지만 Draw 호출 수가 늘어난다.
    // 셀 (cx, cz) -> 정점 인덱스 변환 공식이 GridMesh::Generate 와 완전히 같아야 한다
    // (정점은 새로 만들지 않고 mesh.vertices 를 그대로 참조하기 때문).
    Tree Build(const GridMesh::MeshData& mesh, int maxLeafCells);

    // 절두체와 겹치는 리프들의 인덱스(Tree::leaves 안에서의 인덱스)를 outVisibleLeaves 에 모은다.
    // 절두체 밖에 있는 노드는 자식을 아예 방문하지 않고 통째로 건너뛴다 -- 이것이 이 기법의
    // 핵심 이득이다 (리프 하나하나를 다 검사하는 것보다 훨씬 적게 검사한다).
    void CollectVisible(const Tree& tree, const Frustum& frustum,
                        std::vector<int>& outVisibleLeaves, Stats* outStats = nullptr);
}
