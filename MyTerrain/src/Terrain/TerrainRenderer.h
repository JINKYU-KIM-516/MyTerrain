#pragma once
#include "../GameObject/Component.h"
#include "GridMesh.h"
#include "Quadtree.h"
#include "TerrainLOD.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
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

    // ---- 지난 프레임 기준 통계 (HUD 표시용) ----
    size_t GetLodChunkCount() const { return m_lodGrid.chunks.size(); }
    size_t GetLodDrawnChunkCount() const { return m_lodDrawnChunkCount; }
    size_t GetLodDrawnTriangleCount() const { return m_lodDrawnTriangleCount; }
    int    GetLodChunksAtLevel(int level) const;
    int    GetLodActualLevelCount() const { return m_lodGrid.levelCount; }
    size_t GetLodIndexCount() const { return m_lodGrid.indices.size(); }
    size_t GetLodBaseIndexCount() const { return m_lodGrid.baseIndexCount; }


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
                              bool useLighting);

private:
    // BasicTerrain.hlsl 의 cbuffer CBTerrain 과 메모리 배치가 같아야 한다 (192바이트)
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
    std::vector<int> m_lodDrawList;    // 이번 프레임에 실제로 그릴 청크 인덱스

    size_t m_lodDrawnChunkCount = 0;
    size_t m_lodDrawnTriangleCount = 0;
    int    m_lodLevelHistogram[TerrainLOD::kMaxLevels]{};

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
