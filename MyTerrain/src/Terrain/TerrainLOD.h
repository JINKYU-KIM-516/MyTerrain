#pragma once
#include "GridMesh.h"
#include <DirectXMath.h>
#include <vector>
#include <cstdint>

// 지형을 정사각형 "청크"로 나누고, 청크마다 여러 해상도(LOD 레벨)의 인덱스를
// 미리 만들어두는 모듈. 6-1 거리 기반 LOD 기법에서만 쓴다.
//
// ---------------------------------------------------------------------------
// 핵심 설계 결정 : 정점 버퍼는 5번과 마찬가지로 통째로 하나만 그대로 쓴다.
// ---------------------------------------------------------------------------
// LOD 를 낮춘다는 것은 "정점을 몇 칸씩 건너뛰고 인덱스를 만든다"는 뜻이다.
// 레벨 L 의 스텝은 2^L 이고, 레벨이 1 올라갈 때마다 삼각형 수가 1/4 로 준다.
//
//   레벨 0 : 스텝 1  -- 16x16 청크 기준 512 삼각형 (100%)
//   레벨 1 : 스텝 2  -- 128 삼각형 (25%)
//   레벨 2 : 스텝 4  -- 32 삼각형  (6.25%)
//   레벨 3 : 스텝 8  -- 8 삼각형   (1.6%)
//
// 정점을 새로 만들거나 높이를 다시 계산하지 않으므로, 레벨을 바꿔도 메시를 다시
// 구울 필요가 없다. 법선도 풀 해상도로 계산된 값을 그대로 쓴다(멀리 있으니 충분하다).
//
// 모든 레벨의 인덱스를 한 번에 만들어 하나의 정적 인덱스 버퍼에 이어 붙인다.
// 추가 메모리는 1 + 1/4 + 1/16 + ... 이라 기본 인덱스 버퍼의 약 1.33 배에 그치고,
// 그 대신 매 프레임 CPU 가 할 일은 "청크마다 어느 구간을 그릴지 고르는 것"뿐이다.
// (매 프레임 동적 인덱스 버퍼를 다시 채우는 방식보다 훨씬 싸다)
//
// ---------------------------------------------------------------------------
// 이 기법이 안고 가는 문제 : 이음매(crack)
// ---------------------------------------------------------------------------
// 레벨이 다른 두 청크가 맞닿으면, 세밀한 쪽에는 경계 변의 중간에 정점이 있고
// 거친 쪽에는 없다. 그 중간 정점의 실제 높이가 양 끝의 평균과 다르면(펄린 지형에서는
// 항상 다르다) 그만큼 틈이 벌어져 배경이 비친다 -- 전형적인 T-junction 문제다.
//
// 6-1 에서는 이 문제를 "숨기지 않고 보여주는" 쪽을 택했다. 완화 수단으로
// ClampNeighborLevels(이웃 레벨 차이를 1 이하로) 만 제공하고, 제대로 된 해결
// (스티칭 / 지오머핑)은 6-2 기법의 주제로 남긴다.
namespace TerrainLOD
{
    // 레벨 0 ~ 4 (스텝 1, 2, 4, 8, 16). 청크 한 변이 16 셀이면 레벨 4 가 청크 전체를
    // 사각형 하나로 그리는 극단값이 된다.
    constexpr int kMaxLevels = 5;

    struct AABB
    {
        DirectX::XMFLOAT3 min{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 max{ 0.0f, 0.0f, 0.0f };
    };

    // 청크 하나 = 셀 범위 [cellX0, cellX1) x [cellZ0, cellZ1) 하나.
    // 레벨마다 인덱스 버퍼 안의 연속 구간을 하나씩 갖는다.
    struct Chunk
    {
        AABB bounds;

        int cellX0 = 0;
        int cellX1 = 0;
        int cellZ0 = 0;
        int cellZ1 = 0;

        uint32_t indexStart[kMaxLevels]{};
        uint32_t indexCount[kMaxLevels]{};
    };

    struct Grid
    {
        std::vector<Chunk>    chunks;    // chunksZ 행 x chunksX 열, 행 우선(z 바깥 루프)
        std::vector<uint32_t> indices;   // 모든 청크 x 모든 레벨의 인덱스. GPU 에 그대로 올린다.

        int chunksX = 0;
        int chunksZ = 0;
        int chunkCells = 0;   // 청크 한 변의 셀 수
        int levelCount = 0;   // 실제로 만들어진 레벨 수 (1 ~ kMaxLevels)

        // 레벨 0 만 썼을 때의 인덱스 수 (= 원본 메시의 인덱스 수). HUD 비교용.
        size_t baseIndexCount = 0;

        bool IsEmpty() const { return chunks.empty() || indices.empty(); }

        int ChunkIndexAt(int cx, int cz) const
        {
            return cz * chunksX + cx;
        }
    };

    // 메시로부터 청크 격자와 레벨별 인덱스를 만든다.
    //   chunkCells : 청크 한 변의 셀 수 (2 이상. 2의 거듭제곱이 아니어도 동작하지만
    //                스텝이 딱 나눠떨어지지 않으면 마지막 사각형이 작아진다)
    //   levelCount : 만들 레벨 수. 스텝(2^(levelCount-1))이 chunkCells 를 넘지 않도록
    //                내부에서 자동으로 줄인다.
    //
    // 셀 (cx, cz) -> 정점 인덱스 변환식과 삼각형 순서가 GridMesh::Generate 와
    // 완전히 같아야 한다 (정점을 새로 만들지 않고 mesh.vertices 를 그대로 참조하므로).
    Grid Build(const GridMesh::MeshData& mesh, int chunkCells, int levelCount);

    // 점 p 에서 AABB 까지의 최단 거리. 중심까지의 거리가 아니라 "가장 가까운 점"까지를
    // 재는 이유는, 카메라가 큰 청크 위에 올라섰을 때 발밑이 갑자기 저해상도로 튀는 것을
    // 막기 위해서다 (중심 기준이면 큰 청크일수록 중심이 멀어진다).
    float DistanceToBounds(const AABB& bounds, const DirectX::XMFLOAT3& p);

    // 거리 -> 레벨. baseDistance 안쪽이면 레벨 0, 그 뒤로는 거리가 2배가 될 때마다
    // 레벨이 하나씩 올라간다 (baseDistance, 2*baseDistance, 4*baseDistance, ...).
    int SelectLevel(float distance, float baseDistance, int levelCount);

    // 이웃(상하좌우) 청크와의 레벨 차이가 1 을 넘지 않도록 낮춘다.
    // 틈의 크기를 줄여줄 뿐 없애지는 못한다 -- 제대로 된 해결은 6-2 의 스티칭이다.
    // levels 의 길이는 grid.chunks 와 같아야 한다.
    void ClampNeighborLevels(const Grid& grid, std::vector<int>& levels);
}
