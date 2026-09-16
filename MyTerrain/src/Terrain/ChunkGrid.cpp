#include "ChunkGrid.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace DirectX;

namespace ChunkGrid
{
    Coord WorldToChunk(float worldX, float worldZ, float chunkWorldSize)
    {
        const float size = (chunkWorldSize > 0.0001f) ? chunkWorldSize : 0.0001f;

        // 청크의 중심이 cx * S 이므로, 경계는 (cx +- 0.5) * S 다.
        // floor(world / S + 0.5) 가 그 경계로 자르는 식이 된다.
        Coord coord;
        coord.x = static_cast<int>(std::floor(worldX / size + 0.5f));
        coord.z = static_cast<int>(std::floor(worldZ / size + 0.5f));
        return coord;
    }

    XMFLOAT3 ChunkCenter(const Coord& coord, float chunkWorldSize)
    {
        return XMFLOAT3(static_cast<float>(coord.x) * chunkWorldSize,
                        0.0f,
                        static_cast<float>(coord.z) * chunkWorldSize);
    }

    int ChebyshevDistance(const Coord& a, const Coord& b)
    {
        return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z));
    }

    void CollectDesired(const Coord& center, int radius,
                        const XMFLOAT3& cameraPosition, float chunkWorldSize,
                        std::vector<Coord>& outCoords)
    {
        radius = std::max(radius, 0);

        outCoords.clear();
        outCoords.reserve(static_cast<size_t>(2 * radius + 1) * static_cast<size_t>(2 * radius + 1));

        for (int dz = -radius; dz <= radius; ++dz)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                outCoords.push_back(Coord{ center.x + dx, center.z + dz });
            }
        }

        // 카메라(XZ 평면 기준)에서 청크 중심까지의 거리 제곱으로 정렬한다.
        // 높이는 보지 않는다 -- 카메라가 높이 떠 있어도 "발밑"의 기준은 XZ 거리다.
        auto DistanceSq = [&](const Coord& coord)
        {
            const XMFLOAT3 center3 = ChunkCenter(coord, chunkWorldSize);
            const float dx = center3.x - cameraPosition.x;
            const float dz = center3.z - cameraPosition.z;
            return dx * dx + dz * dz;
        };

        std::sort(outCoords.begin(), outCoords.end(),
                  [&](const Coord& a, const Coord& b)
                  {
                      const float da = DistanceSq(a);
                      const float db = DistanceSq(b);
                      if (da != db)
                      {
                          return da < db;
                      }

                      // 거리가 같을 때의 순서를 고정해 둔다 -- 그래야 카메라가 멈춰 있는데
                      // 생성 순서가 프레임마다 달라지는 일이 없다.
                      if (a.z != b.z)
                      {
                          return a.z < b.z;
                      }
                      return a.x < b.x;
                  });
    }
}
