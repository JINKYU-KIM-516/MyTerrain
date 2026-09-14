#include "SkyDome.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace SkyDome
{
    MeshData Generate(int latitudeSegments, int longitudeSegments)
    {
        MeshData mesh;

        latitudeSegments = std::max(latitudeSegments, 1);
        longitudeSegments = std::max(longitudeSegments, 3);

        // ---------------- 정점 ----------------
        // 위도 링을 지평선(lat=0)부터 천정 바로 아래(lat=latitudeSegments-1)까지 만들고,
        // 천정 자체는 부채꼴로 모이는 극 정점 하나로 따로 둔다.
        // 링 하나당 (longitudeSegments + 1)개 -- 마지막 경도가 첫 경도와 같은 자리를
        // 한 번 더 찍어(이음매 복제) UV 없이도 텍스처를 붙일 여지를 남겨둔다.
        const int ringCount = latitudeSegments;
        const int vertsPerRing = longitudeSegments + 1;

        mesh.vertices.reserve(static_cast<size_t>(ringCount) * vertsPerRing + 1);

        for (int lat = 0; lat < ringCount; ++lat)
        {
            // lat=0 -> 지평선(phi=0), lat=ringCount-1 -> 천정 바로 아래
            const float phi = XM_PIDIV2 * (static_cast<float>(lat) / static_cast<float>(latitudeSegments));
            const float y = std::sin(phi);
            const float ringRadius = std::cos(phi);

            for (int lon = 0; lon <= longitudeSegments; ++lon)
            {
                const float theta = XM_2PI * (static_cast<float>(lon) / static_cast<float>(longitudeSegments));

                Vertex vertex{};
                vertex.direction = XMFLOAT3(
                    ringRadius * std::cos(theta),
                    y,
                    ringRadius * std::sin(theta));

                mesh.vertices.push_back(vertex);
            }
        }

        // ---- 천정 극 정점 ----
        const uint32_t apexIndex = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(Vertex{ XMFLOAT3(0.0f, 1.0f, 0.0f) });

        // ---------------- 인덱스 (삼각형 리스트) ----------------
        // 링과 링 사이는 GridMesh 와 같은 사각형 2분할이고, 맨 위 링만 극 정점과
        // 부채꼴로 이어 붙인다.
        mesh.indices.reserve(static_cast<size_t>(ringCount) * longitudeSegments * 6);

        for (int lat = 0; lat < ringCount - 1; ++lat)
        {
            for (int lon = 0; lon < longitudeSegments; ++lon)
            {
                const uint32_t i0 = static_cast<uint32_t>(lat * vertsPerRing + lon);
                const uint32_t i1 = i0 + 1;
                const uint32_t i2 = static_cast<uint32_t>((lat + 1) * vertsPerRing + lon);
                const uint32_t i3 = i2 + 1;

                mesh.indices.push_back(i0);
                mesh.indices.push_back(i1);
                mesh.indices.push_back(i2);

                mesh.indices.push_back(i1);
                mesh.indices.push_back(i3);
                mesh.indices.push_back(i2);
            }
        }

        const int topRing = ringCount - 1;
        for (int lon = 0; lon < longitudeSegments; ++lon)
        {
            const uint32_t i0 = static_cast<uint32_t>(topRing * vertsPerRing + lon);
            const uint32_t i1 = i0 + 1;

            mesh.indices.push_back(i0);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(apexIndex);
        }

        return mesh;
    }
}
