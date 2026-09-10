#include "TerrainLOD.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

using namespace DirectX;

namespace TerrainLOD
{
    namespace
    {
        AABB ComputeBounds(const GridMesh::MeshData& mesh, int vertexCountX,
                           int x0, int x1, int z0, int z1)
        {
            AABB bounds;
            bounds.min = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
            bounds.max = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

            // 셀 범위 [x0,x1) 의 정점은 x0 ~ x1 (끝 포함) 이다.
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

        // 정점 (xa,za) (xb,za) (xa,zb) (xb,zb) 네 개로 사각형 하나(삼각형 2개)를 만든다.
        // 스텝이 1 이면 GridMesh::Generate 가 만드는 인덱스와 글자 그대로 같아진다.
        void PushQuad(std::vector<uint32_t>& out, int vertexCountX,
                      int xa, int xb, int za, int zb)
        {
            const uint32_t i0 = static_cast<uint32_t>(za * vertexCountX + xa);
            const uint32_t i1 = static_cast<uint32_t>(za * vertexCountX + xb);
            const uint32_t i2 = static_cast<uint32_t>(zb * vertexCountX + xa);
            const uint32_t i3 = static_cast<uint32_t>(zb * vertexCountX + xb);

            out.push_back(i0);
            out.push_back(i2);
            out.push_back(i1);

            out.push_back(i1);
            out.push_back(i2);
            out.push_back(i3);
        }
    }

    Grid Build(const GridMesh::MeshData& mesh, int chunkCells, int levelCount)
    {
        Grid grid;

        if (mesh.divisionsX <= 0 || mesh.divisionsZ <= 0 || mesh.vertices.empty())
        {
            return grid;
        }

        chunkCells = std::clamp(chunkCells, 1, std::max(mesh.divisionsX, mesh.divisionsZ));
        levelCount = std::clamp(levelCount, 1, kMaxLevels);

        // 스텝이 청크보다 커지면 그 레벨은 의미가 없다(청크 전체가 사각형 하나 밑으로 줄지 않는다).
        while (levelCount > 1 && (1 << (levelCount - 1)) > chunkCells)
        {
            --levelCount;
        }

        const int vertexCountX = mesh.divisionsX + 1;

        grid.chunksX = (mesh.divisionsX + chunkCells - 1) / chunkCells;
        grid.chunksZ = (mesh.divisionsZ + chunkCells - 1) / chunkCells;
        grid.chunkCells = chunkCells;
        grid.levelCount = levelCount;
        grid.baseIndexCount = mesh.indices.size();

        grid.chunks.reserve(static_cast<size_t>(grid.chunksX) * grid.chunksZ);

        // 레벨 0 이 원본과 같은 양이고, 레벨이 하나 오를 때마다 1/4 이므로 합은 약 4/3 배다.
        grid.indices.reserve(mesh.indices.size() * 4 / 3 + 64);

        for (int cz = 0; cz < grid.chunksZ; ++cz)
        {
            for (int cx = 0; cx < grid.chunksX; ++cx)
            {
                Chunk chunk;
                chunk.cellX0 = cx * chunkCells;
                chunk.cellZ0 = cz * chunkCells;
                chunk.cellX1 = std::min(chunk.cellX0 + chunkCells, mesh.divisionsX);
                chunk.cellZ1 = std::min(chunk.cellZ0 + chunkCells, mesh.divisionsZ);

                chunk.bounds = ComputeBounds(mesh, vertexCountX,
                                             chunk.cellX0, chunk.cellX1,
                                             chunk.cellZ0, chunk.cellZ1);

                for (int level = 0; level < levelCount; ++level)
                {
                    const int step = 1 << level;

                    chunk.indexStart[level] = static_cast<uint32_t>(grid.indices.size());

                    for (int z = chunk.cellZ0; z < chunk.cellZ1; z += step)
                    {
                        // 청크 바깥으로 넘어가지 않도록 마지막 사각형은 경계에서 잘라준다.
                        // 덕분에 청크의 바깥 테두리 정점(x0, x1, z0, z1)은 어느 레벨에서도
                        // 항상 쓰인다 -- 이음매는 "테두리 중간 정점이 빠져서" 생기는 것이지
                        // 테두리 자체가 어긋나서 생기는 것이 아니다.
                        const int zb = std::min(z + step, chunk.cellZ1);

                        for (int x = chunk.cellX0; x < chunk.cellX1; x += step)
                        {
                            const int xb = std::min(x + step, chunk.cellX1);
                            PushQuad(grid.indices, vertexCountX, x, xb, z, zb);
                        }
                    }

                    chunk.indexCount[level] =
                        static_cast<uint32_t>(grid.indices.size()) - chunk.indexStart[level];
                }

                // 만들지 않은 레벨은 0 번 레벨을 가리키게 해둔다 (잘못 접근해도 안전하게).
                for (int level = levelCount; level < kMaxLevels; ++level)
                {
                    chunk.indexStart[level] = chunk.indexStart[levelCount - 1];
                    chunk.indexCount[level] = chunk.indexCount[levelCount - 1];
                }

                grid.chunks.push_back(chunk);
            }
        }

        return grid;
    }

    float DistanceToBounds(const AABB& bounds, const XMFLOAT3& p)
    {
        const float dx = std::max({ bounds.min.x - p.x, 0.0f, p.x - bounds.max.x });
        const float dy = std::max({ bounds.min.y - p.y, 0.0f, p.y - bounds.max.y });
        const float dz = std::max({ bounds.min.z - p.z, 0.0f, p.z - bounds.max.z });

        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    int SelectLevel(float distance, float baseDistance, int levelCount)
    {
        levelCount = std::clamp(levelCount, 1, kMaxLevels);

        if (baseDistance <= 0.0f || distance <= 0.0f)
        {
            return 0;
        }

        // log2 를 직접 쓰지 않고 임계값을 두 배씩 밀어보는 이유는, 임계 거리를 HUD 에
        // 그대로 보여주고 싶어서다 (baseDistance, 2x, 4x, ... 가 곧 레벨 경계다).
        int level = 0;
        float threshold = baseDistance;

        while (level + 1 < levelCount && distance >= threshold)
        {
            ++level;
            threshold *= 2.0f;
        }

        return level;
    }

    void ClampNeighborLevels(const Grid& grid, std::vector<int>& levels)
    {
        if (grid.chunks.empty() || levels.size() != grid.chunks.size())
        {
            return;
        }

        // 한 번 낮추면 그 이웃도 다시 낮춰야 할 수 있으므로 변화가 없을 때까지 반복한다.
        // 레벨 수만큼만 돌면 반드시 수렴한다 (한 번 돌 때마다 최소 1 레벨씩 내려간다).
        for (int pass = 0; pass < kMaxLevels; ++pass)
        {
            bool changed = false;

            for (int cz = 0; cz < grid.chunksZ; ++cz)
            {
                for (int cx = 0; cx < grid.chunksX; ++cx)
                {
                    const int self = grid.ChunkIndexAt(cx, cz);

                    const int neighbors[4][2] =
                    {
                        { cx - 1, cz }, { cx + 1, cz }, { cx, cz - 1 }, { cx, cz + 1 },
                    };

                    for (const auto& n : neighbors)
                    {
                        if (n[0] < 0 || n[0] >= grid.chunksX || n[1] < 0 || n[1] >= grid.chunksZ)
                        {
                            continue;
                        }

                        const int other = grid.ChunkIndexAt(n[0], n[1]);
                        if (levels[self] > levels[other] + 1)
                        {
                            levels[self] = levels[other] + 1;
                            changed = true;
                        }
                    }
                }
            }

            if (!changed)
            {
                break;
            }
        }
    }
}
