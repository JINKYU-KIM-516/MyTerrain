#include "GridMesh.h"
#include <algorithm>

using namespace DirectX;

namespace GridMesh
{
    MeshData Generate(int divisionsX, int divisionsZ, float cellSize, const HeightFunc& heightFunc)
    {
        MeshData mesh;

        divisionsX = std::max(divisionsX, 1);
        divisionsZ = std::max(divisionsZ, 1);
        cellSize = std::max(cellSize, 0.0001f);

        mesh.divisionsX = divisionsX;
        mesh.divisionsZ = divisionsZ;
        mesh.cellSize = cellSize;

        const int vertexCountX = divisionsX + 1;
        const int vertexCountZ = divisionsZ + 1;

        const float width = divisionsX * cellSize;
        const float depth = divisionsZ * cellSize;

        // 원점이 격자의 한가운데가 되도록 절반만큼 앞으로 당겨서 시작한다
        const float startX = -width * 0.5f;
        const float startZ = -depth * 0.5f;

        // ---------------- 1) 높이 계산 ----------------
        // 법선을 이웃 높이차로 구해야 하므로 높이를 먼저 전부 채워둔다.
        std::vector<float> heights(static_cast<size_t>(vertexCountX) * vertexCountZ, 0.0f);

        if (heightFunc)
        {
            for (int z = 0; z < vertexCountZ; ++z)
            {
                for (int x = 0; x < vertexCountX; ++x)
                {
                    const float worldX = startX + x * cellSize;
                    const float worldZ = startZ + z * cellSize;
                    heights[static_cast<size_t>(z) * vertexCountX + x] = heightFunc(worldX, worldZ);
                }
            }
        }

        auto HeightAt = [&](int x, int z) -> float
        {
            x = std::clamp(x, 0, vertexCountX - 1);
            z = std::clamp(z, 0, vertexCountZ - 1);
            return heights[static_cast<size_t>(z) * vertexCountX + x];
        };

        // ---------------- 2) 정점 ----------------
        mesh.vertices.reserve(static_cast<size_t>(vertexCountX) * vertexCountZ);

        for (int z = 0; z < vertexCountZ; ++z)
        {
            for (int x = 0; x < vertexCountX; ++x)
            {
                Vertex vertex{};

                vertex.position = XMFLOAT3(
                    startX + x * cellSize,
                    HeightAt(x, z),
                    startZ + z * cellSize);

                // 중앙 차분으로 법선 계산.
                // 접선 벡터 = (2*cellSize, hR - hL, 0), (0, hU - hD, 2*cellSize)
                // 두 벡터의 외적을 정리하면 아래 식이 된다.
                const float heightLeft = HeightAt(x - 1, z);
                const float heightRight = HeightAt(x + 1, z);
                const float heightDown = HeightAt(x, z - 1);
                const float heightUp = HeightAt(x, z + 1);

                XMVECTOR normal = XMVectorSet(
                    heightLeft - heightRight,
                    2.0f * cellSize,
                    heightDown - heightUp,
                    0.0f);
                normal = XMVector3Normalize(normal);
                XMStoreFloat3(&vertex.normal, normal);

                vertex.uv = XMFLOAT2(
                    static_cast<float>(x) / static_cast<float>(divisionsX),
                    static_cast<float>(z) / static_cast<float>(divisionsZ));

                mesh.vertices.push_back(vertex);
            }
        }

        // ---------------- 3) 인덱스 (삼각형 리스트) ----------------
        //   i2 ---- i3
        //    |    / |
        //    |  /   |
        //   i0 ---- i1
        mesh.indices.reserve(static_cast<size_t>(divisionsX) * divisionsZ * 6);

        for (int z = 0; z < divisionsZ; ++z)
        {
            for (int x = 0; x < divisionsX; ++x)
            {
                const uint32_t i0 = static_cast<uint32_t>(z * vertexCountX + x);
                const uint32_t i1 = i0 + 1;
                const uint32_t i2 = i0 + static_cast<uint32_t>(vertexCountX);
                const uint32_t i3 = i2 + 1;

                mesh.indices.push_back(i0);
                mesh.indices.push_back(i2);
                mesh.indices.push_back(i1);

                mesh.indices.push_back(i1);
                mesh.indices.push_back(i2);
                mesh.indices.push_back(i3);
            }
        }

        return mesh;
    }
}
