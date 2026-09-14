#pragma once
#include "GridMesh.h"
#include <DirectXMath.h>
#include <vector>
#include <cstdint>

// 7번 하드웨어 테셀레이션 기법이 쓰는 "패치 컨트롤 넷".
//
// 6-1/6-2 의 TerrainLOD 는 "레벨마다 인덱스를 미리 구워두고 CPU 가 매 프레임 고른다"
// 는 접근이었다. 7번은 그 선택 자체를 GPU 테셀레이션 스테이지(HS -> 테셀레이터 -> DS)에
// 넘긴다. 그래서 여기서 만드는 것은 여러 해상도의 인덱스가 아니라, 딱 하나의 아주
// 성긴 "컨트롤 넷"이다 -- 내부 디테일은 매 프레임 GPU 가 즉석에서 만든다.
//
// ---------------------------------------------------------------------------
// 패치 하나 = 코스 격자 셀 하나
// ---------------------------------------------------------------------------
// TerrainRenderer::SetGrid 로 만드는 GridMesh 를 "아주 성기게"(예: 16 x 16) 잡으면,
// 그 격자의 셀 하나하나가 그대로 패치 하나가 된다 -- 삼각형 2개로 쪼개는 대신
// 정점 4개(코너)를 그대로 컨트롤 포인트 패치로 묶어 올리기만 하면 된다.
// 그래서 이 모듈은 정점을 새로 만들지 않고, GridMesh::Generate 가 이미 만들어 둔
// 정점 버퍼를 그대로 참조하는 인덱스만 만든다 (TerrainLOD 와 같은 전제).
//
// 코너 4개의 순서는 (x,z) (x+1,z) (x,z+1) (x+1,z+1) 이다. GridMesh::Generate 의
// i0,i1,i2,i3 규약과 글자 그대로 같다 -- BasicTerrain.hlsl 의 도메인 셰이더가
// SV_DomainLocation(u,v) 를 이 순서 그대로 쌍선형 보간한다.
//
// ---------------------------------------------------------------------------
// 패치 개수를 크게 잡으면 안 되는 이유
// ---------------------------------------------------------------------------
// 6-1/6-2 의 "청크"와 달리 여기서는 패치 하나 = 셀 하나이므로, divisionsX/Z 를
// 다른 기법처럼 256 이상으로 잡으면 패치가 수만 개가 되어 버린다. 패치가 많아질수록
// HS 가 도는 횟수(패치당 1회)도 늘어나서, GPU 테셀레이션으로 아끼려는 것보다
// CPU->GPU 드로우 콜/HS 오버헤드가 커진다. 이 기법의 전제는 "패치는 적게(수십~수백),
// 패치 안쪽 디테일은 GPU 가 필요한 만큼만" 이다. TessellationControlComponent 가
// 분할 수 범위를 4~64 로 좁혀두는 이유이기도 하다.
namespace PatchGrid
{
    struct AABB
    {
        DirectX::XMFLOAT3 min{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 max{ 0.0f, 0.0f, 0.0f };
    };

    // 패치 하나. indices 안에서 patches[i] 에 대응하는 인덱스 4개는
    // indices[i*4 .. i*4+3] 이다 (그대로 컨트롤 포인트 패치 리스트로 GPU 에 올린다).
    struct Patch
    {
        AABB bounds;   // 코스 코너 4개만으로 잰 경계. displacement(높이맵 재샘플링)로
                       // 정점이 더 밀려날 수 있으므로 컬링할 때는 여유를 둬야 한다
                       // (TerrainRenderer::RebuildPatchIndexBuffer 주석 참고).
    };

    struct Grid
    {
        std::vector<uint32_t> indices;   // patches.size() * 4
        std::vector<Patch>    patches;

        int patchCountX = 0;
        int patchCountZ = 0;

        bool IsEmpty() const { return patches.empty() || indices.empty(); }
    };

    // mesh 는 TerrainRenderer::SetGrid 로 만든 "코스한" GridMesh 다. 셀 (cx, cz) ->
    // 정점 인덱스 변환식은 GridMesh::Generate 와 완전히 같아야 한다 (정점을 새로
    // 만들지 않고 mesh.vertices 를 그대로 참조하므로).
    Grid Build(const GridMesh::MeshData& mesh);
}
