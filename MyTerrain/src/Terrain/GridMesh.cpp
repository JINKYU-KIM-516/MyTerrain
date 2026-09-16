#include "GridMesh.h"
#include <algorithm>

using namespace DirectX;

namespace GridMesh
{
    namespace
    {
        // 정점 (x, z) 가 살아남는 마지막 LOD 레벨.
        // 레벨 L 에서는 x, z 가 모두 2^L 의 배수인 정점만 쓰이므로,
        // "x 와 z 를 동시에 나누는 2의 최대 거듭제곱" 의 지수가 곧 정점 레벨이다.
        int VertexLevel(int x, int z, int maxLevel)
        {
            int level = 0;
            while (level < maxLevel)
            {
                const int nextStep = 1 << (level + 1);
                if ((x % nextStep) != 0 || (z % nextStep) != 0)
                {
                    break;
                }
                ++level;
            }
            return level;
        }

        // 정점 (x, z) 가 "스텝 step 짜리 거친 격자" 위에서 갖게 될 높이.
        //
        // 거친 격자에서 이 정점을 품는 사각형은 [x0, x1] x [z0, z1] 이고,
        // 그 사각형은 GridMesh 의 삼각형 분할 규약(i0,i2,i1 / i1,i2,i3)에 따라
        // (x0,z1)-(x1,z0) 대각선으로 잘려 있다. 그래서 단순 쌍선형 보간이 아니라
        // 대각선의 어느 쪽에 있는지를 보고 해당 삼각형 위에서 보간해야
        // 거친 메시와 정확히 같은 높이가 나온다 (TerrainLOD 가 굽는 거친 인덱스와
        // 같은 대각선이어야 한다는 뜻이다).
        //
        // 격자 맨 끝(x == divisionsX 등)에서는 그 축으로 사각형이 없다. 그때는
        // 그 축의 비율을 0 으로 두어 남은 축으로만 보간한다 -- 한쪽 축이 눌렸다고
        // 통째로 포기하면 지형 가장자리에서 부모 높이가 어긋난다.
        template <typename HeightFn>
        float ParentHeight(const HeightFn& HeightAt, int divisionsX, int divisionsZ,
                           int x, int z, int step)
        {
            const int x0 = (x / step) * step;
            const int z0 = (z / step) * step;
            const int x1 = std::min(x0 + step, divisionsX);
            const int z1 = std::min(z0 + step, divisionsZ);

            const float u = (x1 > x0) ? static_cast<float>(x - x0) / static_cast<float>(x1 - x0) : 0.0f;
            const float v = (z1 > z0) ? static_cast<float>(z - z0) / static_cast<float>(z1 - z0) : 0.0f;

            const float h00 = HeightAt(x0, z0);
            const float h10 = HeightAt(x1, z0);
            const float h01 = HeightAt(x0, z1);
            const float h11 = HeightAt(x1, z1);

            if (u + v <= 1.0f)
            {
                // 삼각형 (x0,z0) (x0,z1) (x1,z0)
                return h00 * (1.0f - u - v) + h01 * v + h10 * u;
            }

            // 삼각형 (x1,z0) (x0,z1) (x1,z1)
            return h10 * (1.0f - v) + h01 * (1.0f - u) + h11 * (u + v - 1.0f);
        }

        // Generate 와 GenerateChunk 의 공통 본체.
        //
        //   worldOffsetX/Z : 높이 함수에 넘길 좌표에 더할 값. 정점 위치에는 더하지 않는다
        //                    (정점은 언제나 원점 중심이고, 배치는 월드 행렬이 담당한다).
        //   apronNormals   : true 면 격자 바깥 한 칸을 실제로 샘플링해서 법선을 구한다.
        //                    false 면 예전과 똑같이 가장자리 값을 clamp 해서 쓴다.
        //
        // 10번 이전(1~9번) 기법은 전부 offset 0 / apron 없음으로 들어오므로 결과가
        // 한 비트도 달라지지 않는다.
        MeshData GenerateInternal(int divisionsX, int divisionsZ, float cellSize,
                                  const HeightFunc& heightFunc,
                                  float worldOffsetX, float worldOffsetZ,
                                  bool apronNormals)
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
            //
            // 사방으로 한 칸씩 여유를 둔 배열에 담는다(패딩). 인덱스 -1 과 vertexCount 를
            // 그대로 쓸 수 있게 하기 위한 것이고, 그 여유 칸을 무엇으로 채우느냐가
            // apronNormals 의 전부다.
            const int paddedX = vertexCountX + 2;
            const int paddedZ = vertexCountZ + 2;

            std::vector<float> heights(static_cast<size_t>(paddedX) * paddedZ, 0.0f);

            auto Slot = [&](int x, int z) -> float&
            {
                const int px = std::clamp(x + 1, 0, paddedX - 1);
                const int pz = std::clamp(z + 1, 0, paddedZ - 1);
                return heights[static_cast<size_t>(pz) * paddedX + px];
            };

            if (heightFunc)
            {
                const int lo = apronNormals ? -1 : 0;
                const int hiX = apronNormals ? vertexCountX : vertexCountX - 1;
                const int hiZ = apronNormals ? vertexCountZ : vertexCountZ - 1;

                for (int z = lo; z <= hiZ; ++z)
                {
                    for (int x = lo; x <= hiX; ++x)
                    {
                        Slot(x, z) = heightFunc(startX + x * cellSize + worldOffsetX,
                                                startZ + z * cellSize + worldOffsetZ);
                    }
                }

                if (!apronNormals)
                {
                    // 여유 칸을 가장자리 값으로 복사한다. 예전 구현이 HeightAt 안에서
                    // clamp 하던 것과 결과가 완전히 같다 (1~9번 기법 결과 불변).
                    for (int z = -1; z <= vertexCountZ; ++z)
                    {
                        for (int x = -1; x <= vertexCountX; ++x)
                        {
                            const bool inside = (x >= 0 && x < vertexCountX && z >= 0 && z < vertexCountZ);
                            if (inside)
                            {
                                continue;
                            }

                            Slot(x, z) = Slot(std::clamp(x, 0, vertexCountX - 1),
                                              std::clamp(z, 0, vertexCountZ - 1));
                        }
                    }
                }
            }

            auto HeightAt = [&](int x, int z) -> float
            {
                return Slot(x, z);
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

                    // ---- 6-2 지오머핑용 : 정점 레벨과, 그 정점이 사라질 때의 목표 높이 ----
                    const int vertexLevel = VertexLevel(x, z, kMaxVertexLevel);
                    const int parentStep = 1 << (vertexLevel + 1);

                    vertex.morphData = XMFLOAT2(
                        ParentHeight(HeightAt, divisionsX, divisionsZ, x, z, parentStep),
                        static_cast<float>(vertexLevel));

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

    MeshData Generate(int divisionsX, int divisionsZ, float cellSize, const HeightFunc& heightFunc)
    {
        return GenerateInternal(divisionsX, divisionsZ, cellSize, heightFunc, 0.0f, 0.0f, false);
    }

    MeshData GenerateChunk(int divisionsX, int divisionsZ, float cellSize,
                           const HeightFunc& heightFunc,
                           float worldOffsetX, float worldOffsetZ)
    {
        return GenerateInternal(divisionsX, divisionsZ, cellSize, heightFunc,
                                worldOffsetX, worldOffsetZ, true);
    }
}
