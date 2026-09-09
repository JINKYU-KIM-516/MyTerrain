#include "Quadtree.h"
#include <algorithm>
#include <cfloat>
#include <utility>

using namespace DirectX;

namespace Quadtree
{
    namespace
    {
        // [a0, a1) 셀 범위를 절반씩 나눈다. maxCells 이하면 그대로 하나만 돌려준다.
        std::vector<std::pair<int, int>> SplitAxis(int a0, int a1, int maxCells)
        {
            if (a1 - a0 <= maxCells)
            {
                return { { a0, a1 } };
            }

            int mid = a0 + (a1 - a0) / 2;
            mid = std::clamp(mid, a0 + 1, a1 - 1);

            return { { a0, mid }, { mid, a1 } };
        }

        AABB ComputeBounds(const GridMesh::MeshData& mesh, int vertexCountX,
                           int x0, int x1, int z0, int z1)
        {
            AABB bounds;
            bounds.min = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
            bounds.max = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

            for (int z = z0; z <= z1; ++z)
            {
                for (int x = x0; x <= x1; ++x)
                {
                    const XMFLOAT3& p = mesh.vertices[static_cast<size_t>(z) * vertexCountX + x].position;

                    bounds.min.x = std::min(bounds.min.x, p.x);
                    bounds.min.y = std::min(bounds.min.y, p.y);
                    bounds.min.z = std::min(bounds.min.z, p.z);

                    bounds.max.x = std::max(bounds.max.x, p.x);
                    bounds.max.y = std::max(bounds.max.y, p.y);
                    bounds.max.z = std::max(bounds.max.z, p.z);
                }
            }

            return bounds;
        }

        // 셀 (cx, cz) 하나를 삼각형 2개(6 인덱스)로 tree.indices 에 밀어 넣는다.
        // GridMesh::Generate 의 인덱스 생성식과 글자 그대로 같아야 한다 (i0/i1/i2/i3, 삼각형 순서).
        void PushCellIndices(Tree& tree, int vertexCountX, int cx, int cz)
        {
            const uint32_t i0 = static_cast<uint32_t>(cz * vertexCountX + cx);
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + static_cast<uint32_t>(vertexCountX);
            const uint32_t i3 = i2 + 1;

            tree.indices.push_back(i0);
            tree.indices.push_back(i2);
            tree.indices.push_back(i1);

            tree.indices.push_back(i1);
            tree.indices.push_back(i2);
            tree.indices.push_back(i3);
        }

        int BuildNode(Tree& tree, const GridMesh::MeshData& mesh, int vertexCountX,
                     int x0, int x1, int z0, int z1, int maxLeafCells)
        {
            const AABB bounds = ComputeBounds(mesh, vertexCountX, x0, x1, z0, z1);

            if (x1 - x0 <= maxLeafCells && z1 - z0 <= maxLeafCells)
            {
                // ---- 리프: 이 범위 안의 모든 셀을 지금 인덱스 목록 끝에 이어 붙인다 ----
                Leaf leaf;
                leaf.bounds = bounds;
                leaf.indexStart = static_cast<uint32_t>(tree.indices.size());

                for (int z = z0; z < z1; ++z)
                {
                    for (int x = x0; x < x1; ++x)
                    {
                        PushCellIndices(tree, vertexCountX, x, z);
                    }
                }

                leaf.indexCount = static_cast<uint32_t>(tree.indices.size()) - leaf.indexStart;

                Node node;
                node.bounds = bounds;
                node.leafIndex = static_cast<int>(tree.leaves.size());
                tree.leaves.push_back(leaf);

                const int nodeIndex = static_cast<int>(tree.nodes.size());
                tree.nodes.push_back(node);
                return nodeIndex;
            }

            // ---- 내부 노드: 두 축을 각자 필요한 만큼만 갈라서 최대 4개의 자식을 만든다 ----
            const int nodeIndex = static_cast<int>(tree.nodes.size());
            tree.nodes.push_back(Node{});
            tree.nodes[nodeIndex].bounds = bounds;

            const auto xRanges = SplitAxis(x0, x1, maxLeafCells);
            const auto zRanges = SplitAxis(z0, z1, maxLeafCells);

            int childSlot = 0;
            for (const auto& xr : xRanges)
            {
                for (const auto& zr : zRanges)
                {
                    if (childSlot >= 4)
                    {
                        break; // 이론상 xRanges/zRanges 는 각각 최대 2개라 여기 닿을 일이 없다
                    }

                    const int childIndex = BuildNode(tree, mesh, vertexCountX,
                                                      xr.first, xr.second, zr.first, zr.second,
                                                      maxLeafCells);
                    // tree.nodes 가 재귀 중에 재할당됐을 수 있으므로 매번 인덱스로 다시 접근한다
                    // (참조를 루프 밖에 들고 있지 않는다).
                    tree.nodes[nodeIndex].children[childSlot] = childIndex;
                    ++childSlot;
                }
            }

            return nodeIndex;
        }

        void CollectRecursive(const Tree& tree, int nodeIndex, const Frustum& frustum,
                              std::vector<int>& out, Stats* stats)
        {
            if (nodeIndex < 0)
            {
                return;
            }

            const Node& node = tree.nodes[nodeIndex];

            if (stats != nullptr)
            {
                ++stats->nodesVisited;
            }

            if (!frustum.IntersectsAABB(node.bounds.min, node.bounds.max))
            {
                // 이 노드가 절두체 밖이면 자식(그 아래 리프 전부)을 방문조차 하지 않는다.
                // 컬링의 핵심 이득은 바로 이 "통째로 건너뛰기"에서 나온다.
                return;
            }

            if (node.leafIndex >= 0)
            {
                out.push_back(node.leafIndex);
                if (stats != nullptr)
                {
                    ++stats->leavesVisible;
                }
                return;
            }

            for (int child : node.children)
            {
                CollectRecursive(tree, child, frustum, out, stats);
            }
        }
    }

    Tree Build(const GridMesh::MeshData& mesh, int maxLeafCells)
    {
        Tree tree;

        maxLeafCells = std::max(maxLeafCells, 1);

        if (mesh.divisionsX <= 0 || mesh.divisionsZ <= 0 || mesh.vertices.empty())
        {
            return tree;
        }

        const int vertexCountX = mesh.divisionsX + 1;

        tree.indices.reserve(mesh.indices.size());
        tree.rootIndex = BuildNode(tree, mesh, vertexCountX, 0, mesh.divisionsX, 0, mesh.divisionsZ, maxLeafCells);

        return tree;
    }

    void CollectVisible(const Tree& tree, const Frustum& frustum,
                        std::vector<int>& outVisibleLeaves, Stats* outStats)
    {
        outVisibleLeaves.clear();
        if (outStats != nullptr)
        {
            *outStats = Stats{};
        }

        if (tree.IsEmpty())
        {
            return;
        }

        CollectRecursive(tree, tree.rootIndex, frustum, outVisibleLeaves, outStats);
    }
}
