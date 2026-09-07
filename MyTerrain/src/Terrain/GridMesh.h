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
    // 지형 정점 형식. BasicTerrain.hlsl 의 VSInput 과 순서/크기가 일치해야 한다.
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 uv;
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
