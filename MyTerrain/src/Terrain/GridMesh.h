#pragma once
#include <DirectXMath.h>
#include <functional>
#include <vector>
#include <cstdint>

// 지형의 바탕이 되는 "격자(그리드) 메시"를 CPU에서 만들어내는 모듈.
//
// 1번 기법(기본 평면 그리드)에서는 높이가 전부 0인 평면을 만들지만,
// Generate() 에 높이 함수를 넘기면 그대로 펄린 노이즈 지형 / 높이맵 지형에도
// 재사용할 수 있도록 만들어두었다.
//
//   - XZ 평면 위에 원점을 중심으로 펼쳐진다
//   - 정점 수 = (divisionsX + 1) * (divisionsZ + 1)
//   - 삼각형 수 = divisionsX * divisionsZ * 2
//   - 법선은 이웃 정점의 높이차(중앙 차분)로 계산하므로 높이 함수를 넣어도 자연스럽다
namespace GridMesh
{
    // 정점 레벨의 상한. TerrainLOD::kMaxLevels - 1 과 같아야 한다.
    constexpr int kMaxVertexLevel = 4;

    // 지형 정점 형식. BasicTerrain.hlsl 의 VSInput 과 순서/크기가 일치해야 한다.
    //
    // morphData 는 6-2 지오머핑에서만 쓴다. 6-1 까지의 기법은 셰이더가 이 값을
    // 읽더라도 morph 계수가 0 이라 결과가 완전히 같다(lerp(y, parent, 0) == y).
    //
    //   morphData.x = 부모 높이  : 이 정점이 "사라질 때" 놓이게 될 거친 표면 위의 높이
    //   morphData.y = 정점 레벨  : 이 정점이 살아남는 마지막 LOD 레벨
    //
    // 정점 (x, z) 는 레벨 L 에서 x, z 가 모두 2^L 의 배수일 때만 쓰인다. 그러니
    // 이 정점이 사라지는 전환은 "정점 레벨 -> 정점 레벨 + 1" 딱 한 번뿐이고,
    // 목표 높이도 하나면 충분하다 (레벨마다 따로 들고 있을 필요가 없다).
    //
    // 이것이 6-2 지오머핑의 핵심이다. morph 계수를 "청크의 레벨" 이 아니라
    // "정점 자신의 레벨과 거리" 로 계산하면, 이 정점을 공유하는 두 청크가 서로 다른
    // 레벨이더라도 똑같은 높이를 계산한다 -- 이음매를 없애려다 새 이음매를 만드는
    // 일이 원천적으로 생기지 않는다.
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 uv;
        DirectX::XMFLOAT2 morphData;
    };

    struct MeshData
    {
        std::vector<Vertex>   vertices;
        std::vector<uint32_t> indices;   // 삼각형 리스트

        int   divisionsX = 0;
        int   divisionsZ = 0;
        float cellSize = 0.0f;

        float GetWidth()  const { return divisionsX * cellSize; }
        float GetDepth()  const { return divisionsZ * cellSize; }

        size_t GetVertexCount()   const { return vertices.size(); }
        size_t GetTriangleCount() const { return indices.size() / 3; }
    };

    // (x, z) 월드 좌표를 받아 높이(y)를 돌려주는 함수.
    // 비워두면(nullptr) 높이가 전부 0인 평면이 만들어진다.
    using HeightFunc = std::function<float(float x, float z)>;

    // 격자 메시를 생성한다.
    //   divisionsX / divisionsZ : 가로 / 세로 방향 분할(셀) 개수 (1 이상)
    //   cellSize                : 셀 한 칸의 크기
    MeshData Generate(int divisionsX, int divisionsZ, float cellSize,
                      const HeightFunc& heightFunc = nullptr);
}
