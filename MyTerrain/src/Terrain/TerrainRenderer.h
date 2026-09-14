#pragma once
#include "../GameObject/Component.h"
#include "GridMesh.h"
#include "Quadtree.h"
#include "TerrainLOD.h"
#include "PatchGrid.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

// 지형 메시의 표시 방식
enum class TerrainDisplayMode
{
    SolidWireframe = 0,  // 솔리드 + 그 위에 와이어프레임 (기본값)
    Wireframe,           // 와이어프레임만
    Solid,               // 솔리드만
    Count
};

const wchar_t* ToDisplayName(TerrainDisplayMode mode);

// 격자(지형) 메시를 실제로 GPU에 올려 그리는 컴포넌트.
//
//  - GridMesh::Generate 로 만든 CPU 메시를 정점/인덱스 버퍼로 올린다
//  - BasicTerrain.hlsl 을 실행 중에 컴파일해서 사용한다
//  - 솔리드 / 와이어프레임 / 솔리드+와이어프레임 세 가지 모드를 지원한다
//
// 그리드 파라미터(SetGrid)가 바뀌면 다음 렌더링 직전에 메시를 다시 만든다.
class TerrainRenderer : public Component
{
public:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    void Start() override;
    void Render() override;
    void Destroy() override;

    // ---------------- 그리드 파라미터 ----------------
    // 값이 바뀌면 메시를 다시 생성한다 (같은 값이면 아무 일도 하지 않음)
    void SetGrid(int divisionsX, int divisionsZ, float cellSize);
    void SetHeightFunction(const GridMesh::HeightFunc& heightFunc);

    // 높이 함수 자체는 그대로지만 그 "안에서 참조하는 값"이 바뀌었을 때 사용한다.
    // (예: 펄린 노이즈 파라미터 조절) 다음 렌더링 직전에 메시를 다시 만든다.
    void RequestRebuild() { m_meshDirty = true; }
    bool IsMeshDirty() const { return m_meshDirty; }

    // 마지막 메시 재생성에 걸린 시간(밀리초). 파라미터별 비용을 눈으로 보기 위한 값이다.
    double GetLastRebuildMilliseconds() const { return m_lastRebuildMs; }

    int   GetDivisionsX() const { return m_divisionsX; }
    int   GetDivisionsZ() const { return m_divisionsZ; }
    float GetCellSize()   const { return m_cellSize; }

    size_t GetVertexCount()   const { return m_vertexCount; }
    size_t GetTriangleCount() const { return m_triangleCount; }

    // ---------------- 표시 설정 ----------------
    void SetDisplayMode(TerrainDisplayMode mode) { m_displayMode = mode; }
    TerrainDisplayMode GetDisplayMode() const { return m_displayMode; }
    void CycleDisplayMode();

    void SetSolidColor(float r, float g, float b) { m_solidColor = { r, g, b, 1.0f }; }
    void SetWireColor(float r, float g, float b) { m_wireColor = { r, g, b, 1.0f }; }

    // 방향광이 나아가는 방향 (정규화하지 않아도 된다)
    void SetLightDirection(float x, float y, float z) { m_lightDirection = { x, y, z }; }

    // 픽셀 셰이더의 체커 패턴 한 칸 = 셀 크기 * 이 배율 (기본 1배).
    // 분할 수가 큰 지형에서 1셀 단위 체커는 너무 잘아서 노이즈처럼 보이므로 키워서 쓴다.
    void SetCheckerScale(float scale) { m_checkerScale = (scale > 0.0f) ? scale : 1.0f; }

    // ---------------- 높이맵 텍스처 (3번 기법에서 사용) ----------------
    // 높이맵을 GPU 텍스처로 올려두면 픽셀 셰이더가 고도별로 색을 칠할 수 있다.
    // 아무것도 넘기지 않으면(기본) 셰이더는 지금까지와 똑같이 동작하므로 1·2번 기법은 영향이 없다.
    void SetHeightMapResources(ID3D11ShaderResourceView* srv, ID3D11SamplerState* sampler);

    // 높이맵 한 장이 덮는 월드 크기와 Z 방향. HeightMap::Params 와 같은 값을 넘겨야
    // 셰이더가 CPU 와 똑같은 텍셀을 읽는다.
    void SetHeightMapMapping(float worldSize, bool flipZ);

    // 고도별 색상 모드. 켜면 체커 대신 높이맵 텍스처를 읽어 고도 램프 색을 칠한다.
    void SetHeightColorMode(bool enabled) { m_heightColorMode = enabled; }
    bool IsHeightColorMode() const { return m_heightColorMode; }

    // ---------------- 텍스처 스플래팅 (4번 기법에서 사용) ----------------
    // 모래/잔디/바위/눈 4장을 배열 텍스처 하나로 받는다. 아무것도 넘기지 않으면
    // (기본) 셰이더는 스플래팅 분기를 타지 않으므로 1~3번 기법은 영향이 없다.
    void SetSplatResources(ID3D11ShaderResourceView* arraySrv, ID3D11SamplerState* sampler);

    // tiling         : 텍스처가 월드 1 단위당 몇 번 반복되는지 (worldPos.xz 에 곱해서 UV 로 쓴다)
    // slopeStart/End : 경사도(0=평지, 1=수직)가 이 구간을 지나며 모래/잔디에서 바위로 전이된다
    void SetSplatParams(float tiling, float slopeStart, float slopeEnd);

    // 켜면 고도 색상/체커 대신 정점 높이·경사도로 섞은 스플래팅 텍스처를 그린다.
    // 실제로는 텍스처가 올라와 있을 때만 켜진다 (UpdateConstantBuffer 참고).
    void SetSplatMode(bool enabled) { m_splatMode = enabled; }
    bool IsSplatMode() const { return m_splatMode; }

    bool IsReady() const { return m_resourcesReady; }

    // ---------------- 쿼드트리 컬링 (5번 기법에서 사용) ----------------
    // maxLeafCells 를 한 번도 설정하지 않으면(기본값 0) 이 기능은 완전히 비활성 상태로 남고
    // 1~4번 기법과 완전히 똑같이 인덱스 버퍼 하나로 통째로 그린다.
    // 리프 한 변의 최대 셀 수. 값이 바뀌면 다음 렌더링 직전에 쿼드트리만 다시 만든다
    // (전체 메시를 다시 굽지는 않는다 -- Perlin 노이즈처럼 높이 계산이 비싼 지형에서도
    //  리프 크기 조절은 가볍게 반응해야 하기 때문).
    void SetQuadtreeLeafSize(int maxLeafCells);
    int  GetQuadtreeLeafSize() const { return m_quadtreeMaxLeafCells; }

    // 켜면 절두체 밖 리프를 건너뛰고 보이는 리프만 나눠 그린다.
    // 끄면(기본) 쿼드트리가 만들어져 있어도 1~4번과 같은 단일 Draw 호출로 되돌아간다
    // (컬링 유무를 눈으로 비교하기 위한 스위치).
    void SetQuadtreeCullingEnabled(bool enabled) { m_quadtreeCullingEnabled = enabled; }
    bool IsQuadtreeCullingEnabled() const { return m_quadtreeCullingEnabled; }

    // 켜면 보이는 리프들의 AABB 를 선으로 겹쳐 그린다 (컬링이 실제로 뭘 하고 있는지 시각화).
    void SetQuadtreeDebugBoxesEnabled(bool enabled) { m_quadtreeDebugBoxesEnabled = enabled; }
    bool IsQuadtreeDebugBoxesEnabled() const { return m_quadtreeDebugBoxesEnabled; }

    // 지난 프레임 기준 통계 (HUD 표시용)
    size_t GetQuadtreeLeafCount() const { return m_quadtree.leaves.size(); }
    size_t GetQuadtreeNodeCount() const { return m_quadtree.nodes.size(); }
    size_t GetQuadtreeVisibleLeafCount() const { return m_quadtreeVisibleLeafCount; }

    // ---------------- 거리 기반 LOD (6-1 기법에서 사용) ----------------
    // 청크 한 변의 셀 수. 0 이면(기본) 이 기능은 완전히 비활성 상태로 남고
    // 1~5번 기법은 이 코드 경로에 아예 들어오지 않는다.
    // 값이 바뀌면 다음 렌더링 직전에 청크 격자만 다시 만든다 (메시는 다시 굽지 않는다).
    void SetLodChunkSize(int cells);
    int  GetLodChunkSize() const { return m_lodChunkCells; }

    // 만들 LOD 레벨 수 (1 ~ TerrainLOD::kMaxLevels). 스텝이 청크 크기를 넘으면 내부에서 줄인다.
    void SetLodLevelCount(int count);
    int  GetLodLevelCount() const { return m_lodLevelCount; }

    // 끄면 모든 청크를 레벨 0(풀 해상도)으로 그린다. 청크 단위로 나눠 그리는 것 자체는
    // 그대로 두므로, 켜고 끌 때 Draw 호출 수는 그대로이고 삼각형 수만 달라진다
    // -- LOD 의 이득만 따로 떼어 비교하기 위해서다.
    void SetLodEnabled(bool enabled) { m_lodEnabled = enabled; }
    bool IsLodEnabled() const { return m_lodEnabled; }

    // 레벨 0 이 유지되는 거리. 이 거리를 넘으면 레벨 1, 그 두 배를 넘으면 레벨 2 ...
    void SetLodBaseDistance(float distance);
    float GetLodBaseDistance() const { return m_lodBaseDistance; }

    // 켜면 청크를 레벨별 색으로 칠한다 (셰이더 수정 없이 gBaseColor 만 바꿔 넣는다).
    void SetLodColorMode(bool enabled) { m_lodColorMode = enabled; }
    bool IsLodColorMode() const { return m_lodColorMode; }

    // 5번과 같은 절두체 컬링. LOD 와 독립적으로 켜고 끌 수 있다.
    void SetLodFrustumCullingEnabled(bool enabled) { m_lodFrustumCullingEnabled = enabled; }
    bool IsLodFrustumCullingEnabled() const { return m_lodFrustumCullingEnabled; }

    // 켜면 그려지는 청크들의 AABB 를 선으로 겹쳐 그린다.
    void SetLodDebugBoxesEnabled(bool enabled) { m_lodDebugBoxesEnabled = enabled; }
    bool IsLodDebugBoxesEnabled() const { return m_lodDebugBoxesEnabled; }

    // 켜면 이웃 청크와의 레벨 차이가 1 을 넘지 않게 낮춘다 (이음매 완화).
    void SetLodNeighborClampEnabled(bool enabled) { m_lodNeighborClampEnabled = enabled; }
    bool IsLodNeighborClampEnabled() const { return m_lodNeighborClampEnabled; }

    // 켜는 순간의 카메라 위치로 레벨을 고정한다. 컬링은 계속 실제 카메라를 따라가므로
    // 레벨 경계까지 날아가서 이음매를 코앞에서 관찰할 수 있다.
    void SetLodFrozen(bool frozen) { m_lodFrozen = frozen; m_lodFreezeRequested = frozen; }
    bool IsLodFrozen() const { return m_lodFrozen; }

    // ---------------- 6-2 : 스티칭 & 지오머핑 ----------------
    // 켜면 이웃보다 세밀한 청크의 테두리를 매 프레임 다시 엮어 T-junction 을 없앤다.
    // (거친 쪽은 손대지 않는다. 켜면 이웃 레벨 차이 제한이 자동으로 강제된다)
    void SetLodStitchEnabled(bool enabled) { m_lodStitchEnabled = enabled; }
    bool IsLodStitchEnabled() const { return m_lodStitchEnabled; }

    // 켜면 레벨 경계에 가까워질수록 정점 높이를 한 단계 거친 레벨 쪽으로 끌어당긴다.
    // 끄면(기본) 정점 셰이더의 lerp 계수가 0 이 되어 6-1 과 완전히 같은 결과가 나온다.
    void SetLodMorphEnabled(bool enabled) { m_lodMorphEnabled = enabled; }
    bool IsLodMorphEnabled() const { return m_lodMorphEnabled; }

    // morph 를 시작하는 지점 (레벨 경계 거리의 몇 % 앞에서부터인지, 0 ~ 0.5).
    // 0.5 를 넘으면 morph 구간이 레벨 범위를 넘어서고, 거친 쪽 청크가 세밀한 이웃과
    // 붙어 있는 동안에도 움직이기 시작해서 경계에 실오라기 같은 틈이 생길 수 있다.
    void SetLodMorphWidth(float width);
    float GetLodMorphWidth() const { return m_lodMorphWidth; }

    // 켜면 morph 계수를 색으로 칠한다 (파랑 = 0, 빨강 = 1).
    void SetLodMorphColorMode(bool enabled) { m_lodMorphColorMode = enabled; }
    bool IsLodMorphColorMode() const { return m_lodMorphColorMode; }

    // 청크 AABB 대각선의 최대값. 지오머핑이 팝핑을 제대로 흡수하려면 기준 거리가
    // 이 값보다 넉넉히 커야 한다 (아래 GetLodRecommendedBaseDistance 참고).
    float GetLodMaxChunkDiagonal() const { return m_lodMaxChunkDiagonal; }

    // 지금 설정에서 권장하는 최소 기준 거리.
    //
    // 레벨 전환은 청크의 "가장 가까운 점" 기준으로 일어나는데, 같은 청크 안에서도
    // 정점마다 거리가 최대 대각선만큼 차이난다. 기준 거리가 그 차이에 비해 작으면
    // 한 청크 안에서 어떤 정점은 이미 morph 를 끝냈고 어떤 정점은 아직 시작도 안 한
    // 상태로 레벨이 바뀌어 버려서, 지오머핑이 팝핑을 다 흡수하지 못한다.
    // (근본 해결은 레벨이 오를수록 청크도 커지는 쿼드트리 구조 -- 10번 무한 지형의 몫이다)
    float GetLodRecommendedBaseDistance() const
    {
        return m_lodMaxChunkDiagonal / std::max(1.0f - 2.0f * m_lodMorphWidth, 0.05f);
    }

    // ---- 지난 프레임 기준 통계 (HUD 표시용) ----
    size_t GetLodStitchedChunkCount() const { return m_lodStitchedChunkCount; }
    size_t GetLodStitchIndexCount() const { return m_stitchIndices.size(); }
    size_t GetLodChunkCount() const { return m_lodGrid.chunks.size(); }
    size_t GetLodDrawnChunkCount() const { return m_lodDrawnChunkCount; }
    size_t GetLodDrawnTriangleCount() const { return m_lodDrawnTriangleCount; }
    int    GetLodChunksAtLevel(int level) const;
    int    GetLodActualLevelCount() const { return m_lodGrid.levelCount; }
    size_t GetLodIndexCount() const { return m_lodGrid.indices.size(); }
    size_t GetLodBaseIndexCount() const { return m_lodGrid.baseIndexCount; }

    // ---------------- 7번 하드웨어 테셀레이션 ----------------
    // 켜면 SetGrid 로 만든 코스한 격자를 "삼각형 메시" 가 아니라 "패치 컨트롤 넷"으로
    // 해석해서 HS/DS 로 그린다 (6-1/6-2 의 m_lodChunkCells > 0 같은 역할의 스위치다).
    // 끄면(기본) 1~6번 기법과 완전히 같은 경로로 그리고, 이 아래 상태는 전부 무시된다.
    void SetTessellationEnabled(bool enabled);
    bool IsTessellationEnabled() const { return m_tessellationEnabled; }

    // 기준 거리 안쪽은 항상 최대 팩터, 그 뒤로는 거리에 반비례해서 최소 팩터까지 줄어든다
    // (HS 의 TessEdgeFactor 와 같은 식이다. 여기서는 '패치 박스'와 HUD 추정치 계산에 쓴다).
    void SetTessFactorRange(float minFactor, float maxFactor);
    float GetTessMinFactor() const { return m_tessMinFactor; }
    float GetTessMaxFactor() const { return m_tessMaxFactor; }

    void SetTessBaseDistance(float distance);
    float GetTessBaseDistance() const { return m_tessBaseDistance; }

    // 끄면 컨트롤 포인트 4개를 쌍선형 보간만 한 매끈한 패치가 된다 (디스플레이스먼트 없음
    // -- 테셀레이션을 늘려도 새 지형 디테일이 안 생기는 것을 눈으로 비교하기 위한 스위치).
    void SetTessDisplacementEnabled(bool enabled) { m_tessDisplacementEnabled = enabled; }
    bool IsTessDisplacementEnabled() const { return m_tessDisplacementEnabled; }

    // 높이맵 0~1 값을 월드 높이로 바꾸는 값. HeightMapControlComponent::GetParams() 의
    // heightScale/heightOffset 과 항상 같은 값을 넘겨야 한다 (Technique07 이 매 프레임 맞춘다).
    void SetTessHeightMapScale(float heightScale, float heightOffset);

    // displacement 뒤 법선을 중앙 차분으로 다시 계산할 때 쓰는 월드 단위 샘플 간격.
    void SetTessNormalEpsilon(float epsilon);
    float GetTessNormalEpsilon() const { return m_tessNormalEpsilon; }

    // partition 모드는 HS 함수에 붙는 컴파일타임 속성이라 값 하나로 못 바꾼다 --
    // 몸통이 같은 HS 를 두 벌 컴파일해두고 이 스위치로 어느 쪽을 바인딩할지 고른다.
    // true = fractional_odd (새 삼각형이 한 점에서 자라나오듯 부드럽게 나타난다)
    // false = integer (팩터가 정수로 딱딱 끊겨서 바뀐다 -- 미세한 팝핑이 남는다)
    void SetTessFractionalPartitioning(bool enabled) { m_tessFractionalPartitioning = enabled; }
    bool IsTessFractionalPartitioning() const { return m_tessFractionalPartitioning; }

    // 켜면 패치 팩터를 색으로 칠한다 (파랑 = 최소, 빨강 = 최대).
    void SetTessFactorColorMode(bool enabled) { m_tessFactorColorMode = enabled; }
    bool IsTessFactorColorMode() const { return m_tessFactorColorMode; }

    // 5번/6-1번과 같은 절두체 컬링. 패치 단위로 이루어진다.
    void SetTessFrustumCullingEnabled(bool enabled) { m_tessFrustumCullingEnabled = enabled; }
    bool IsTessFrustumCullingEnabled() const { return m_tessFrustumCullingEnabled; }

    // 켜면 그려지는 패치들의 AABB 를 선으로 겹쳐 그린다.
    void SetTessDebugBoxesEnabled(bool enabled) { m_tessDebugBoxesEnabled = enabled; }
    bool IsTessDebugBoxesEnabled() const { return m_tessDebugBoxesEnabled; }

    // 켜는 순간의 카메라 위치로 팩터 계산 기준점을 고정한다 (6-1의 F 와 같은 역할 --
    // 패치 경계까지 날아가 이웃 팩터가 정말 일치하는지 코앞에서 관찰할 수 있다).
    void SetTessFrozen(bool frozen) { m_tessFrozen = frozen; m_tessFreezeRequested = frozen; }
    bool IsTessFrozen() const { return m_tessFrozen; }

    // ---- 지난 프레임 기준 통계 (HUD 표시용) ----
    size_t GetTessPatchCount() const { return m_patchGrid.patches.size(); }
    size_t GetTessDrawnPatchCount() const { return m_tessDrawnPatchCount; }
    // TessEdgeFactor 와 같은 식으로 CPU 에서 어림잡은 값이다. integer 파티션 모드에서는
    // 거의 정확하고, fractional 모드에서는 실제 GPU 값과 반 팩터 정도 어긋날 수 있다.
    size_t GetTessEstimatedTriangleCount() const { return m_tessEstimatedTriangleCount; }
    bool IsTessellationPipelineReady() const { return m_tessPipelineReady; }
    bool IsTessellationPipelineFailed() const { return m_tessPipelineFailed; }

private:
    // 셰이더 / 입력 레이아웃 / 래스터라이저 상태 / 상수 버퍼 생성 (최초 1회)
    bool CreateDeviceResources();

    // 현재 파라미터로 정점/인덱스 버퍼를 다시 만든다
    bool RebuildMesh();

    // m_cpuMesh 로부터 쿼드트리(및 재정렬된 인덱스 버퍼)를 다시 만든다.
    // m_quadtreeMaxLeafCells <= 0 이면(기능 미사용) 아무 것도 하지 않고 비운다.
    bool RebuildQuadtreeIndexBuffer();

    // m_cpuMesh 로부터 LOD 청크 격자(및 레벨별 인덱스 버퍼)를 다시 만든다.
    // m_lodChunkCells <= 0 이면(기능 미사용) 아무 것도 하지 않고 비운다.
    bool RebuildLodIndexBuffer();

    // 이번 프레임에 그릴 청크와 각 청크의 레벨을 정한다 (m_lodLevels / m_lodDrawList 갱신).
    void UpdateLodSelection(const DirectX::XMMATRIX& viewProj, const DirectX::XMFLOAT3& cameraPosition);

    // 스티칭이 필요한 청크들의 테두리를 CPU 에서 만들어 동적 인덱스 버퍼에 올린다.
    // (UpdateLodSelection 이 채워둔 m_lodChunkMask 를 보고 결정한다)
    bool UploadLodStitchBuffer(ID3D11DeviceContext* context);

    // m_lodDrawList 를 청크마다 DrawIndexed 로 그린다.
    //   allowLevelColor : 레벨 색상 모드를 허용할지 (와이어프레임 패스는 단색이어야 하므로 false)
    void DrawLodChunks(ID3D11DeviceContext* context,
                       const DirectX::XMMATRIX& world,
                       const DirectX::XMMATRIX& viewProj,
                       const DirectX::XMFLOAT3& cameraPosition,
                       const DirectX::XMFLOAT4& baseColor,
                       bool useLighting,
                       bool allowLevelColor);

    // AABB 목록을 선(LINELIST)으로 그린다. 기존 셰이더/입력 레이아웃을 그대로 재사용한다
    // (GridMesh::Vertex 모양으로 박스 모서리를 채워 넣을 뿐).
    void RenderBoxLines(ID3D11DeviceContext* context,
                        const DirectX::XMMATRIX& world,
                        const DirectX::XMMATRIX& viewProj,
                        const DirectX::XMFLOAT3& cameraPosition,
                        const std::vector<std::pair<DirectX::XMFLOAT3, DirectX::XMFLOAT3>>& boxes,
                        const DirectX::XMFLOAT4& color);

    // 보이는 리프들의 AABB 를 선(LINELIST)으로 그린다. 기존 셰이더/입력 레이아웃을
    // 그대로 재사용한다 (GridMesh::Vertex 모양으로 박스 모서리를 채워 넣을 뿐).
    void RenderDebugBoxes(ID3D11DeviceContext* context,
                          const DirectX::XMMATRIX& world,
                          const DirectX::XMMATRIX& viewProj,
                          const DirectX::XMFLOAT3& cameraPosition,
                          const std::vector<int>& visibleLeaves);

    void UpdateConstantBuffer(ID3D11DeviceContext* context,
                              const DirectX::XMMATRIX& world,
                              const DirectX::XMMATRIX& viewProj,
                              const DirectX::XMFLOAT3& cameraPosition,
                              const DirectX::XMFLOAT4& baseColor,
                              bool useLighting,
                              bool morphEnabled = false);

    // ---------------- 7번 하드웨어 테셀레이션 ----------------
    // VSPatch/HSMain_Integer/HSMain_FracOdd/DSMain 을 컴파일한다. m_tessellationEnabled 가
    // 처음 켜졌을 때 한 번만(지연) 시도한다. 실패해도 CreateDeviceResources 자체는
    // 건드리지 않으므로 1~6번 기법은 전혀 영향받지 않는다 -- 이 기법만 그리기를 건너뛴다.
    bool CreateTessellationPipelineResources();

    // m_cpuMesh 로부터 패치 컨트롤 넷을 다시 만든다 (SetGrid 로 divisions/cellSize 가
    // 바뀔 때마다). m_tessellationEnabled 가 꺼져 있으면 아무 것도 하지 않는다.
    bool RebuildPatchIndexBuffer();

    // 이번 프레임에 그릴 패치 목록을 정한다 (절두체 컬링). displacement 가 켜져 있으면
    // 코너만으로 잰 AABB 가 실제 표면을 다 못 덮을 수 있으므로 y 범위에 heightScale 만큼
    // 여유를 둔다 -- 그래도 극단적으로 뾰족한 봉우리 하나가 코너 넷에 하나도 안 걸리면
    // 컬링에서 잘릴 수 있다는 근사임을 알고 넘어간다(패치를 잘게 쪼개면 덜해진다).
    void UpdateTessPatchSelection(const DirectX::XMMATRIX& viewProj, const DirectX::XMFLOAT3& cameraPosition);

    // m_tessDrawList 를 패치마다 DrawIndexed(패치당 컨트롤 포인트 4개)로 그린다.
    void DrawTessellatedPatches(ID3D11DeviceContext* context);

    // 패치 파이프라인 전체(솔리드+와이어 패스, 디버그 박스 포함)를 그린다.
    // Render() 가 m_tessellationEnabled 일 때 일반 경로 대신 이쪽을 부른다.
    void RenderTessellated(ID3D11DeviceContext* context,
                           const DirectX::XMMATRIX& world,
                           const DirectX::XMMATRIX& viewProj,
                           const DirectX::XMFLOAT3& cameraPosition);

    // CBTessellation 을 채워 올린다. HS/DS/PS 모두에서 슬롯 b1 로 바인딩된다.
    void UpdateTessConstantBuffer(ID3D11DeviceContext* context,
                                  const DirectX::XMMATRIX& viewProj,
                                  const DirectX::XMFLOAT3& origin);

private:
    // BasicTerrain.hlsl 의 cbuffer CBTerrain 과 메모리 배치가 같아야 한다 (224바이트)
    struct TerrainConstants
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4X4 worldViewProj;
        DirectX::XMFLOAT4   baseColor;
        DirectX::XMFLOAT3   lightDirection;
        float               useLighting;
        DirectX::XMFLOAT3   cameraPosition;
        float               cellSize;

        // x = 1 / 높이맵이 덮는 월드 크기
        // y = Z 방향 부호 (flipZ 면 -1)
        // z = 고도 색상 모드 (0 = 끔, 1 = 켬)
        // w = 예약
        DirectX::XMFLOAT4   heightMapParams;

        // 4번 스플래팅 기법에서만 쓴다.
        //   x = 텍스처 타일링 배율
        //   y = 경사 임계값 시작 (0=평지, 1=수직)
        //   z = 경사 임계값 끝
        //   w = 스플래팅 모드 (0 = 끔 -> 1~3번 기법과 완전히 동일하게 동작)
        DirectX::XMFLOAT4   splatParams;

        // 6-2 지오머핑에서만 쓴다.
        //   x = 실제 최고 LOD 레벨 (이 레벨 이상인 정점은 사라지지 않으므로 움직이지 않는다)
        //   y = 기준 거리
        //   z = morph 구간 폭 (0 ~ 0.5)
        //   w = 지오머핑 켬/끔 (0 = 끔 -> 1~6-1번 기법과 완전히 동일하게 동작)
        DirectX::XMFLOAT4   lodParams;

        // xyz = LOD 기준 위치 (프리즈 중이면 얼려둔 카메라 위치)
        // w   = morph 계수 시각화 모드
        DirectX::XMFLOAT4   lodOrigin;
    };

    // BasicTerrain.hlsl 의 cbuffer CBTessellation(b1) 과 메모리 배치가 같아야 한다.
    // 7번 기법 전용이지만, PS 의 시각화 분기가 항상 이 버퍼를 참조하므로(꺼져 있으면
    // gTessOrigin.w == 0 이라 그냥 지나간다) 1~6번 기법을 그릴 때도 항상 바인딩해 둔다.
    struct TessConstants
    {
        DirectX::XMFLOAT4X4 viewProj;

        // x = 기준 거리, y = 최대 팩터, z = 최소 팩터, w = 예약
        DirectX::XMFLOAT4   factorParams;

        // x = heightScale, y = heightOffset, z = displacement 켬/끔, w = 법선 재계산 epsilon
        DirectX::XMFLOAT4   heightParams;

        // xyz = 팩터 계산 기준 위치, w = 팩터 시각화 모드
        DirectX::XMFLOAT4   origin;
    };

    // ---- 그리드 파라미터 ----
    int   m_divisionsX = 64;
    int   m_divisionsZ = 64;
    float m_cellSize = 1.0f;
    GridMesh::HeightFunc m_heightFunc;

    size_t m_vertexCount = 0;
    size_t m_triangleCount = 0;
    UINT   m_indexCount = 0;

    bool   m_meshDirty = true;
    double m_lastRebuildMs = 0.0;

    // RebuildMesh 가 마지막으로 만든 CPU 메시. 리프 크기만 바뀌었을 때(SetQuadtreeLeafSize)
    // 높이를 다시 계산하지 않고 쿼드트리만 다시 만들기 위해 들고 있는다.
    GridMesh::MeshData m_cpuMesh;

    // ---- 표시 설정 ----
    TerrainDisplayMode m_displayMode = TerrainDisplayMode::SolidWireframe;
    DirectX::XMFLOAT4  m_solidColor{ 0.36f, 0.58f, 0.34f, 1.0f };
    DirectX::XMFLOAT4  m_wireColor{ 0.92f, 0.96f, 1.0f, 1.0f };
    DirectX::XMFLOAT3  m_lightDirection{ 0.5f, -1.0f, 0.35f };
    float              m_checkerScale = 1.0f;

    // ---- 높이맵 텍스처 ----
    ComPtr<ID3D11ShaderResourceView> m_heightMapSRV;
    ComPtr<ID3D11SamplerState>       m_heightMapSampler;
    float m_heightMapWorldSize = 256.0f;
    bool  m_heightMapFlipZ = true;
    bool  m_heightColorMode = false;

    // ---- 스플래팅 텍스처 ----
    ComPtr<ID3D11ShaderResourceView> m_splatSRV;
    ComPtr<ID3D11SamplerState>       m_splatSampler;
    float m_splatTiling = 0.08f;
    float m_splatSlopeStart = 0.35f;
    float m_splatSlopeEnd = 0.65f;
    bool  m_splatMode = false;

    // ---- 쿼드트리 컬링 ----
    int    m_quadtreeMaxLeafCells = 0;   // 0 = 기능 꺼짐 (1~4번 기법과 동일하게 동작)
    bool   m_quadtreeDirty = true;
    bool   m_quadtreeCullingEnabled = true;
    bool   m_quadtreeDebugBoxesEnabled = false;
    Quadtree::Tree m_quadtree;
    ComPtr<ID3D11Buffer> m_quadtreeIndexBuffer;
    size_t m_quadtreeVisibleLeafCount = 0;

    // ---- 거리 기반 LOD (6-1) ----
    int    m_lodChunkCells = 0;    // 0 = 기능 꺼짐 (1~5번 기법과 동일하게 동작)
    int    m_lodLevelCount = 4;
    bool   m_lodDirty = true;
    bool   m_lodEnabled = true;
    bool   m_lodColorMode = false;
    bool   m_lodFrustumCullingEnabled = true;
    bool   m_lodDebugBoxesEnabled = false;
    bool   m_lodNeighborClampEnabled = false;
    bool   m_lodFrozen = false;
    bool   m_lodFreezeRequested = false;   // 프리즈를 켠 첫 프레임에 카메라 위치를 붙잡기 위한 플래그
    float  m_lodBaseDistance = 60.0f;
    DirectX::XMFLOAT3 m_lodFrozenCameraPosition{ 0.0f, 0.0f, 0.0f };

    TerrainLOD::Grid m_lodGrid;
    ComPtr<ID3D11Buffer> m_lodIndexBuffer;

    std::vector<int> m_lodLevels;      // 청크마다 이번 프레임에 쓸 레벨
    std::vector<int> m_lodDrawList;    // 이번 프레임에 실제로 그릴 청크 인덱스 (레벨 순으로 정렬)

    // ---- 6-2 스티칭 & 지오머핑 ----
    bool  m_lodStitchEnabled = false;
    bool  m_lodMorphEnabled = false;
    bool  m_lodMorphColorMode = false;
    float m_lodMorphWidth = 0.25f;
    float m_lodMaxChunkDiagonal = 0.0f;   // 청크 격자를 다시 만들 때 함께 계산한다

    // 청크마다 "이웃이 한 단계 거친 방향" 비트 (TerrainStitch::EdgeBit). 0 이면 통짜로 그린다.
    std::vector<int> m_lodChunkMask;

    // 마스크가 0 이 아닌 청크의 테두리를 매 프레임 여기에 만들어 동적 버퍼로 올린다.
    // 레벨 영역은 넓고 경계는 얇아서 실제로 만드는 양은 전체의 10~20% 수준이다.
    std::vector<uint32_t> m_stitchIndices;
    std::vector<UINT>     m_lodStitchStart;   // 청크별 시작 (m_stitchIndices 안에서의 위치)
    std::vector<UINT>     m_lodStitchCount;   // 청크별 인덱스 수 (0 이면 스티칭 안 함)
    ComPtr<ID3D11Buffer>  m_lodStitchIndexBuffer;
    UINT   m_lodStitchCapacity = 0;
    size_t m_lodStitchedChunkCount = 0;

    size_t m_lodDrawnChunkCount = 0;
    size_t m_lodDrawnTriangleCount = 0;
    int    m_lodLevelHistogram[TerrainLOD::kMaxLevels]{};

    // ---- 7번 하드웨어 테셀레이션 ----
    bool m_tessellationEnabled = false;   // 0 그대로면(기본) 1~6번 기법과 완전히 동일하게 동작
    bool m_tessPipelineReady = false;
    bool m_tessPipelineFailed = false;

    bool m_tessPatchGridDirty = true;
    PatchGrid::Grid m_patchGrid;
    ComPtr<ID3D11Buffer> m_patchIndexBuffer;

    float m_tessBaseDistance = 60.0f;
    float m_tessMinFactor = 1.0f;
    float m_tessMaxFactor = 32.0f;

    bool  m_tessDisplacementEnabled = true;
    float m_tessHeightScale = 60.0f;
    float m_tessHeightOffset = 0.0f;
    float m_tessNormalEpsilon = 1.0f;

    bool m_tessFractionalPartitioning = true;
    bool m_tessFactorColorMode = false;
    bool m_tessFrustumCullingEnabled = true;
    bool m_tessDebugBoxesEnabled = false;

    bool m_tessFrozen = false;
    bool m_tessFreezeRequested = false;
    DirectX::XMFLOAT3 m_tessFrozenCameraPosition{ 0.0f, 0.0f, 0.0f };

    std::vector<int> m_tessDrawList;   // 이번 프레임에 그릴 패치 인덱스 (컬링을 통과한 것들)
    size_t m_tessDrawnPatchCount = 0;
    size_t m_tessEstimatedTriangleCount = 0;

    ComPtr<ID3D11VertexShader> m_tessVertexShader;   // VSPatch
    ComPtr<ID3D11HullShader>   m_hullShaderInteger;  // HSMain_Integer
    ComPtr<ID3D11HullShader>   m_hullShaderFracOdd;  // HSMain_FracOdd
    ComPtr<ID3D11DomainShader> m_domainShader;       // DSMain
    ComPtr<ID3D11Buffer>       m_tessConstantBuffer; // CBTessellation(b1)

    // ---- 쿼드트리 디버그 박스 (매 프레임 보이는 리프 집합이 바뀌므로 동적 버퍼를 쓴다) ----
    ComPtr<ID3D11Buffer> m_debugBoxVertexBuffer;
    ComPtr<ID3D11Buffer> m_debugBoxIndexBuffer;
    UINT  m_debugBoxVertexCapacity = 0;
    UINT  m_debugBoxIndexCapacity = 0;
    DirectX::XMFLOAT4 m_debugBoxColor{ 1.0f, 0.85f, 0.15f, 1.0f };

    // ---- D3D 리소스 ----
    ComPtr<ID3D11VertexShader>   m_vertexShader;
    ComPtr<ID3D11PixelShader>    m_pixelShader;
    ComPtr<ID3D11InputLayout>    m_inputLayout;
    ComPtr<ID3D11Buffer>         m_vertexBuffer;
    ComPtr<ID3D11Buffer>         m_indexBuffer;
    ComPtr<ID3D11Buffer>         m_constantBuffer;
    ComPtr<ID3D11RasterizerState> m_solidRasterizer;
    ComPtr<ID3D11RasterizerState> m_wireRasterizer;
    ComPtr<ID3D11DepthStencilState> m_depthState;

    bool m_resourcesReady = false;
    bool m_resourceCreationFailed = false;
};
