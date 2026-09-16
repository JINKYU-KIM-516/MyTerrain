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

    // 10번 무한 지형 청크용 생성. Generate 와 두 가지가 다르다.
    //
    // 1) 높이 함수에 "월드 좌표"를 넘긴다
    //    정점 위치 자체는 Generate 와 똑같이 원점 중심으로 만들어진다(청크의 배치는
    //    GameObject 의 Transform 이 아니라 InfiniteTerrainRenderer 가 상수 버퍼에
    //    넣어주는 월드 행렬이 담당한다). 대신 높이만은 worldOffset 을 더한 좌표에서
    //    평가하므로, 이웃 청크의 맞닿은 정점은 같은 월드 좌표를 넣게 되어 값이 정확히
    //    같아진다 -- 펄린이 결정적이기 때문에 이것만으로 위치 이음매가 사라진다.
    //
    // 2) 테두리 바깥 한 칸을 실제로 더 샘플링해서 법선을 구한다 (apron)
    //    Generate 는 격자 끝에서 이웃 높이를 clamp 해서 쓴다. 한 장짜리 지형에서는
    //    가장자리가 화면 밖이라 문제가 없지만, 청크로 쪼개면 그 "가장자리"가 청크마다
    //    생기므로 경계선을 따라 조명이 한 줄 어긋난다(위치는 붙었는데 접힌 자국처럼
    //    보이는 현상). 여유 칸을 진짜 높이로 채우면 경계 정점도 바깥 이웃을 보고
    //    중앙 차분을 계산하므로 법선까지 매끄럽게 이어진다.
    //    비용은 정점 수가 (n+1)^2 -> (n+3)^2 로 늘어나는 만큼의 높이 함수 호출뿐이고,
    //    GPU 로 올라가는 정점은 그대로 (n+1)^2 이다 (여유 칸은 법선용으로만 쓰고 버린다).
    MeshData GenerateChunk(int divisionsX, int divisionsZ, float cellSize,
                           const HeightFunc& heightFunc,
                           float worldOffsetX, float worldOffsetZ);
}
