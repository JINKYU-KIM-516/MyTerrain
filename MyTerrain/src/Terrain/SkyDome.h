#pragma once
#include <DirectXMath.h>
#include <vector>
#include <cstdint>

// 8. 스카이맵 - 스카이돔의 반구 메시를 CPU 에서 만들어내는 모듈.
//
// GridMesh 와 마찬가지로 D3D 에 의존하지 않는 순수 데이터 생성기다. GridMesh 와
// 결정적으로 다른 점 하나: 여기서는 정점의 "위치"가 곧 그 정점을 카메라에서 바라볼
// 방향(반지름 1의 단위벡터)이다. 실제 반지름은 SkyRenderer 가 월드 행렬의 스케일로
// 곱해 넣으므로(카메라 위치로 매 프레임 옮기는 이동도 마찬가지), 이 모듈이 만드는
// 정점 데이터 자체는 방향 정보만 담고 있으면 충분하다.
//
//   - y >= 0 인 위쪽 반구만 만든다 (카메라는 절대 아래쪽을 보지 못한다 -- 아래는
//     항상 지형이 채운다)
//   - 위도(latitude) 방향은 지평선(적도, y=0)에서 천정(y=1)까지
//   - 경도(longitude) 방향은 한 바퀴(0~2π)를 돈다
//   - 맨 위(천정)는 이음매 없이 부채꼴로 모이는 극 정점 하나로 처리한다
//
// 컬링(앞/뒷면)은 일부러 다루지 않는다 -- SkyRenderer 가 TerrainRenderer 의 솔리드
// 래스터라이저와 같은 이유(평면을 어느 쪽에서 봐도 보이게)로 컬링 자체를 꺼두므로,
// 삼각형을 감는 방향은 결과에 영향을 주지 않는다.
namespace SkyDome
{
    // 스카이 정점 형식. Sky.hlsl 의 VSInput 과 순서/크기가 일치해야 한다.
    // 위치 자체가 카메라에서 그 정점을 바라보는 방향(단위 벡터)이다.
    struct Vertex
    {
        DirectX::XMFLOAT3 direction;
    };

    struct MeshData
    {
        std::vector<Vertex>   vertices;
        std::vector<uint32_t> indices;   // 삼각형 리스트

        size_t GetVertexCount()   const { return vertices.size(); }
        size_t GetTriangleCount() const { return indices.size() / 3; }
    };

    // latitudeSegments  : 지평선~천정 사이 분할 수 (1 이상)
    // longitudeSegments : 한 바퀴 분할 수 (3 이상)
    MeshData Generate(int latitudeSegments, int longitudeSegments);
}
