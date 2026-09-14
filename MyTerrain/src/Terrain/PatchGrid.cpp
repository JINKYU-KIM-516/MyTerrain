#include "PatchGrid.h"
#include <cfloat>
#include <algorithm>

using namespace DirectX;

namespace PatchGrid
{
    namespace
    {
        AABB ComputeCornerBounds(const XMFLOAT3& p0, const XMFLOAT3& p1,
                                 const XMFLOAT3& p2, const XMFLOAT3& p3)
        {
            AABB bounds;
            bounds.min = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
            bounds.max = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

            for (const XMFLOAT3* p : { &p0, &p1, &p2, &p3 })
            {
                bounds.min.x = std::min(bounds.min.x, p->x);
                bounds.min.y = std::min(bounds.min.y, p->y);
                bounds.min.z = std::min(bounds.min.z, p->z);

                bounds.max.x = std::max(bounds.max.x, p->x);
                bounds.max.y = std::max(bounds.max.y, p->y);
                bounds.max.z = std::max(bounds.max.z, p->z);
            }

            return bounds;
        }
    }

    Grid Build(const GridMesh::MeshData& mesh)
    {
        Grid grid;

        if (mesh.divisionsX <= 0 || mesh.divisionsZ <= 0 || mesh.vertices.empty())
        {
            return grid;
        }

        const int vertexCountX = mesh.divisionsX + 1;

        grid.patchCountX = mesh.divisionsX;
        grid.patchCountZ = mesh.divisionsZ;

        const size_t patchCount = static_cast<size_t>(grid.patchCountX) * grid.patchCountZ;
        grid.patches.reserve(patchCount);
        grid.indices.reserve(patchCount * 4);

        for (int z = 0; z < mesh.divisionsZ; ++z)
        {
            for (int x = 0; x < mesh.divisionsX; ++x)
            {
                // GridMesh::Generate 와 같은 i0,i1,i2,i3 규약 :
                //   i0 = (x,   z)     i1 = (x+1, z)
                //   i2 = (x,   z+1)   i3 = (x+1, z+1)
                const uint32_t i0 = static_cast<uint32_t>(z * vertexCountX + x);
                const uint32_t i1 = i0 + 1;
                const uint32_t i2 = i0 + static_cast<uint32_t>(vertexCountX);
                const uint32_t i3 = i2 + 1;

                grid.indices.push_back(i0);
                grid.indices.push_back(i1);
                grid.indices.push_back(i2);
                grid.indices.push_back(i3);

                Patch patch;
                patch.bounds = ComputeCornerBounds(
                    mesh.vertices[i0].position, mesh.vertices[i1].position,
                    mesh.vertices[i2].position, mesh.vertices[i3].position);

                grid.patches.push_back(patch);
            }
        }

        return grid;
    }
}
