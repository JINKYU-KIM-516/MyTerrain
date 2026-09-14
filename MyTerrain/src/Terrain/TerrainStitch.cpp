#include "TerrainStitch.h"
#include <algorithm>

namespace TerrainStitch
{
    namespace
    {
        // 둘레를 도는 순서. (X0,Z0) -> (X0,Z1) -> (X1,Z1) -> (X1,Z0) -> 처음으로.
        // 변 번호 0,1,2,3 이 각각 -X, +Z, +X, -Z 이고, 매개변수 t = 변 번호 + 변 안의 비율.
        constexpr int kSideEdgeBit[4] = { EdgeMinusX, EdgePlusZ, EdgePlusX, EdgeMinusZ };

        struct Loop
        {
            std::vector<uint32_t> indices;
            std::vector<float>    params;
        };

        inline uint32_t VertexIndex(int vertexCountX, int x, int z)
        {
            return static_cast<uint32_t>(z) * static_cast<uint32_t>(vertexCountX) + static_cast<uint32_t>(x);
        }

        // 사각형 [x0,x1] x [z0,z1] 의 둘레 정점을 순서대로 모은다.
        // skipMask 에 걸린 변에서는 홀수번째 정점을 건너뛴다(= 그 변을 거친 이웃에 맞춘다).
        // 사각형이 선분이나 점으로 찌그러져 있어도 그대로 동작한다.
        //
        // 매개변수(둘레 위치)는 반드시 "바깥 사각형" 기준으로 재야 한다.
        // 각 고리를 자기 크기로 0~1 정규화하면 안쪽 고리가 바깥 고리와 어긋난 간격으로
        // 끼어들면서, 스티칭과 무관한 자리까지 셀 경계를 가로지르는 삼각형이 생긴다.
        // 바깥 좌표로 재면 안쪽 정점이 바깥 정점 사이사이에 정확히 하나씩 끼어들어,
        // 변 가운데에서는 평범한 격자 삼각형 분할과 글자 그대로 같은 결과가 나온다
        // (모서리만 다르다). 그래야 이웃 레벨이 바뀔 때 그 변만 달라진다.
        void BuildLoop(Loop& loop, int vertexCountX,
                       int x0, int x1, int z0, int z1, int step, int skipMask,
                       int refX0, int refX1, int refZ0, int refZ1)
        {
            loop.indices.clear();
            loop.params.clear();

            const int sideStartX[4] = { x0, x0, x1, x1 };
            const int sideStartZ[4] = { z0, z1, z1, z0 };
            const int sideStepX[4] = { 0, +step, 0, -step };
            const int sideStepZ[4] = { +step, 0, -step, 0 };

            const int spanX = (x1 - x0) / std::max(step, 1);
            const int spanZ = (z1 - z0) / std::max(step, 1);
            const int sideCount[4] = { spanZ, spanX, spanZ, spanX };

            const float refWidth = static_cast<float>(std::max(refX1 - refX0, 1));
            const float refDepth = static_cast<float>(std::max(refZ1 - refZ0, 1));

            for (int side = 0; side < 4; ++side)
            {
                const int count = sideCount[side];
                if (count <= 0)
                {
                    continue;   // 찌그러진 변 -- 정점을 내놓지 않는다
                }

                const bool skipOdd = (skipMask & kSideEdgeBit[side]) != 0;

                // 마지막 정점은 다음 변의 첫 정점이므로 여기서는 빼고 넣는다.
                for (int i = 0; i < count; ++i)
                {
                    if (skipOdd && (i % 2) == 1)
                    {
                        continue;
                    }

                    const int x = sideStartX[side] + sideStepX[side] * i;
                    const int z = sideStartZ[side] + sideStepZ[side] * i;

                    // 각 변의 첫 정점은 모서리다. 안쪽 고리의 모서리는 안쪽으로 물러나 있어
                    // 좌표로 재면 변 중간쯤 값이 나오는데, 그러면 지퍼가 그 정점을 건너뛰고
                    // 두 칸짜리 긴 변을 만들어 버린다(그 변 위에 정점이 얹혀 이음매가 된다).
                    // 모서리는 항상 변의 시작(0)으로 못박아 바깥 모서리와 나란히 세운다.
                    float fraction = 0.0f;
                    if (i > 0)
                    {
                        switch (side)
                        {
                        case 0: fraction = static_cast<float>(z - refZ0) / refDepth; break;
                        case 1: fraction = static_cast<float>(x - refX0) / refWidth; break;
                        case 2: fraction = static_cast<float>(refZ1 - z) / refDepth; break;
                        default: fraction = static_cast<float>(refX1 - x) / refWidth; break;
                        }
                    }

                    loop.indices.push_back(VertexIndex(vertexCountX, x, z));
                    loop.params.push_back(static_cast<float>(side) + fraction);
                }
            }

            if (loop.indices.empty())
            {
                // 사각형이 점 하나로 찌그러진 경우 (청크가 2x2 스텝셀일 때의 안쪽).
                loop.indices.push_back(VertexIndex(vertexCountX, x0, z0));
                loop.params.push_back(0.0f);
            }
        }

        inline void EmitTriangle(std::vector<uint32_t>& out, int vertexCountX,
                                 uint32_t a, uint32_t b, uint32_t c)
        {
            if (a == b || b == c || a == c)
            {
                return;
            }

            // 넓이가 0 인(세 점이 한 직선 위인) 삼각형도 버린다.
            // 그리기에는 아무 영향이 없지만, 그 긴 변 위에 다른 정점이 얹혀 있는 모양이라
            // "이음매가 있다" 고 오해하게 만든다(실제로 T-junction 검사에 걸린다).
            // 모서리에서 안쪽 고리가 한 변을 따라 이어질 때 하나씩 생긴다.
            const long long ax = a % vertexCountX, az = a / vertexCountX;
            const long long bx = b % vertexCountX, bz = b / vertexCountX;
            const long long cx = c % vertexCountX, cz = c / vertexCountX;

            if ((bx - ax) * (cz - az) - (bz - az) * (cx - ax) == 0)
            {
                return;
            }

            out.push_back(a);
            out.push_back(b);
            out.push_back(c);
        }

        // 바깥 고리와 안쪽 고리 사이를 지퍼처럼 채운다.
        // 둘 다 같은 방향으로 돌고 매개변수가 단조 증가하므로, 다음 정점이 더 가까운
        // 쪽을 한 칸씩 전진시키면 겹침/빈틈 없이 삼각형 (바깥 수 + 안쪽 수) 개가 나온다.
        void ZipLoops(std::vector<uint32_t>& out, int vertexCountX, const Loop& outer, const Loop& inner)
        {
            const int outerCount = static_cast<int>(outer.indices.size());
            const int innerCount = static_cast<int>(inner.indices.size());

            if (outerCount == 0 || innerCount == 0)
            {
                return;
            }

            constexpr float kEnd = 4.0f;   // 둘레 한 바퀴 = 4 (변 4개)

            int io = 0;
            int ii = 0;

            while (io < outerCount || ii < innerCount)
            {
                const float nextOuter = (io + 1 < outerCount) ? outer.params[io + 1] : kEnd;
                const float nextInner = (ii + 1 < innerCount) ? inner.params[ii + 1] : kEnd;

                bool advanceOuter;
                if (io >= outerCount)      advanceOuter = false;
                else if (ii >= innerCount) advanceOuter = true;
                else                       advanceOuter = (nextOuter <= nextInner);

                if (advanceOuter)
                {
                    EmitTriangle(out, vertexCountX,
                                 outer.indices[io],
                                 outer.indices[(io + 1) % outerCount],
                                 inner.indices[ii % innerCount]);
                    ++io;
                }
                else
                {
                    EmitTriangle(out, vertexCountX,
                                 outer.indices[io % outerCount],
                                 inner.indices[(ii + 1) % innerCount],
                                 inner.indices[ii]);
                    ++ii;
                }
            }
        }

        // 셀 사각형 [xa,xb] x [za,zb] 을 삼각형 2개로. 분할 규약은 GridMesh::Generate 와 같다.
        void PushQuad(std::vector<uint32_t>& out, int vertexCountX, int xa, int xb, int za, int zb)
        {
            const uint32_t i0 = VertexIndex(vertexCountX, xa, za);
            const uint32_t i1 = VertexIndex(vertexCountX, xb, za);
            const uint32_t i2 = VertexIndex(vertexCountX, xa, zb);
            const uint32_t i3 = VertexIndex(vertexCountX, xb, zb);

            out.push_back(i0); out.push_back(i2); out.push_back(i1);
            out.push_back(i1); out.push_back(i2); out.push_back(i3);
        }

        // 스티칭 없이 테두리 한 줄을 평범하게 채운다 (TerrainLOD 가 미리 굽는 것과 같은 모양).
        void PushPlainRing(std::vector<uint32_t>& out, int vertexCountX,
                           int cellX0, int cellX1, int cellZ0, int cellZ1, int step)
        {
            for (int z = cellZ0; z < cellZ1; z += step)
            {
                const int zb = std::min(z + step, cellZ1);

                for (int x = cellX0; x < cellX1; x += step)
                {
                    const int xb = std::min(x + step, cellX1);

                    const bool onRing = (x == cellX0) || (xb >= cellX1) ||
                                        (z == cellZ0) || (zb >= cellZ1);
                    if (!onRing)
                    {
                        continue;
                    }

                    PushQuad(out, vertexCountX, x, xb, z, zb);
                }
            }
        }
    }

    bool CanStitch(int cellX0, int cellX1, int cellZ0, int cellZ1, int step)
    {
        if (step <= 0)
        {
            return false;
        }

        const int width = cellX1 - cellX0;
        const int depth = cellZ1 - cellZ0;

        if (width <= 0 || depth <= 0)                 return false;
        if ((width % step) != 0 || (depth % step) != 0) return false;

        const int nx = width / step;
        const int nz = depth / step;

        // 스티칭은 경계 셀을 둘씩 묶어 홀수번째 정점을 버리는 것이라, 한 변의
        // 스텝셀 수가 짝수이면서 2 이상이어야 한다.
        if (nx < 2 || nz < 2)              return false;
        if ((nx % 2) != 0 || (nz % 2) != 0) return false;

        // 안쪽(코어) 사각형이 "제대로 된 사각형" 이거나 "점 하나" 여야 한다.
        // 한쪽만 눌려서 선분이 되면(예: 2 x 8 스텝셀) 안쪽 고리가 같은 선분을
        // 왕복하게 되어 지퍼가 겹친 삼각형을 만든다. 그런 모양에서는 스티칭을
        // 포기하고 평범한 링을 만든다(그 자리에 이음매는 남는다).
        //
        // 실제로 쓰는 정사각형 청크에서는 nx = nz = 2의 거듭제곱이라
        // 항상 "점"(2x2) 이거나 "제대로 된 사각형"(4 이상) 이므로 이 조건에 걸리지 않는다.
        const bool innerIsPoint = (nx == 2 && nz == 2);
        const bool innerIsRect = (nx >= 4 && nz >= 4);
        if (!innerIsPoint && !innerIsRect) return false;

        return true;
    }

    void BuildRing(std::vector<uint32_t>& out, int vertexCountX,
                   int cellX0, int cellX1, int cellZ0, int cellZ1,
                   int step, int coarserMask)
    {
        coarserMask &= EdgeAll;

        if (coarserMask == 0 || !CanStitch(cellX0, cellX1, cellZ0, cellZ1, step))
        {
            // 이웃이 전부 같은 레벨이거나 스티칭이 성립하지 않는 모양이면 평범하게 채운다.
            // (지퍼가 만드는 테두리는 변 가운데에서는 이것과 글자 그대로 같고, 모서리에서만
            //  다르다. 그래서 마스크가 켜지고 꺼질 때 달라지는 것은 스티칭한 변과 모서리뿐이다)
            PushPlainRing(out, vertexCountX, cellX0, cellX1, cellZ0, cellZ1, step);
            return;
        }

        Loop outer;
        Loop inner;

        BuildLoop(outer, vertexCountX, cellX0, cellX1, cellZ0, cellZ1, step, coarserMask,
                  cellX0, cellX1, cellZ0, cellZ1);
        BuildLoop(inner, vertexCountX,
                  cellX0 + step, cellX1 - step,
                  cellZ0 + step, cellZ1 - step,
                  step, 0,
                  cellX0, cellX1, cellZ0, cellZ1);

        ZipLoops(out, vertexCountX, outer, inner);
    }
}
