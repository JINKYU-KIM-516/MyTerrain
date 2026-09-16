#pragma once
#include <DirectXMath.h>
#include <vector>
#include <cstdint>

// 월드를 정사각형 청크로 자르고, "지금 메모리에 있어야 할 청크 목록"을 계산하는 모듈.
// 10번 무한 지형 청크 기법에서만 쓴다.
//
// ---------------------------------------------------------------------------
// 이 모듈에는 D3D 가 전혀 없다
// ---------------------------------------------------------------------------
// Quadtree / TerrainLOD / PatchGrid 와 같은 성격의 순수 계산 모듈이다. 버퍼를 만들고
// 그리는 일은 InfiniteTerrainRenderer 가 하고, 여기서는 정수 좌표 계산만 한다.
//
// ---------------------------------------------------------------------------
// 6-1/6-2 의 "청크" 와는 다른 것이다
// ---------------------------------------------------------------------------
// TerrainLOD::Chunk 는 "하나의 큰 정점 버퍼 안의 인덱스 구간"이라 개수가 처음부터
// 고정돼 있다(그리기 최적화). 여기서 말하는 청크는 자기 정점 버퍼를 가진 독립 메시이고,
// 카메라가 움직이면 개수가 늘었다 줄었다 한다(존재 관리). 이름만 같고 목적이 다르다.
//
// ---------------------------------------------------------------------------
// 좌표 규약
// ---------------------------------------------------------------------------
// 청크 (cx, cz) 의 "중심"이 월드 좌표 (cx * S, cz * S) 다 (S = chunkWorldSize).
// 즉 청크는 [cx*S - S/2, cx*S + S/2) 를 덮는다. 중심을 기준으로 잡은 이유는
// GridMesh::Generate 가 만드는 격자가 원점 중심이기 때문이다 -- 청크 하나를 그대로
// (cx*S, 0, cz*S) 로 평행이동만 하면 딱 맞아떨어진다.
namespace ChunkGrid
{
    struct Coord
    {
        int x = 0;
        int z = 0;

        bool operator==(const Coord& other) const { return x == other.x && z == other.z; }
        bool operator!=(const Coord& other) const { return !(*this == other); }
    };

    // (cx, cz) -> 64비트 키. unordered_map 의 키로 쓴다.
    // 음수 좌표를 그대로 비트로 밀어 넣어야 하므로 uint32_t 를 한 번 거친다.
    inline uint64_t MakeKey(const Coord& coord)
    {
        const uint64_t ux = static_cast<uint64_t>(static_cast<uint32_t>(coord.x));
        const uint64_t uz = static_cast<uint64_t>(static_cast<uint32_t>(coord.z));
        return (ux << 32) | uz;
    }

    inline Coord FromKey(uint64_t key)
    {
        Coord coord;
        coord.x = static_cast<int>(static_cast<uint32_t>(key >> 32));
        coord.z = static_cast<int>(static_cast<uint32_t>(key & 0xFFFFFFFFull));
        return coord;
    }

    // 월드 좌표가 속한 청크. 경계는 위의 좌표 규약대로 "중심 기준"이다.
    Coord WorldToChunk(float worldX, float worldZ, float chunkWorldSize);

    // 청크의 중심 월드 좌표 (y 는 0). 그대로 청크의 평행이동 값이 된다.
    DirectX::XMFLOAT3 ChunkCenter(const Coord& coord, float chunkWorldSize);

    // 두 청크 좌표의 체비쇼프 거리(= max(|dx|, |dz|)).
    // 유지 범위를 원이 아니라 정사각형으로 잡기 때문에 이 거리를 쓴다.
    //   - 정사각형이면 카메라가 대각선으로 움직여도 유지 개수가 변하지 않아
    //     프레임당 작업량이 예측 가능하다
    //   - 원형은 모서리 청크가 빠져 개수는 줄지만, 경계에서 생성/해제가 더 자주 튄다
    int ChebyshevDistance(const Coord& a, const Coord& b);

    // center 를 중심으로 radius 칸 안쪽((2*radius+1)^2 개)의 청크 좌표를 모은다.
    // 카메라에서 가까운 청크가 앞에 오도록 정렬해서 돌려준다 -- 생성 예산이 모자랄 때
    // 발밑부터 채우기 위해서다(멀리 있는 청크가 늦게 나타나는 것은 잘 보이지 않는다).
    void CollectDesired(const Coord& center, int radius,
                        const DirectX::XMFLOAT3& cameraPosition, float chunkWorldSize,
                        std::vector<Coord>& outCoords);
}
