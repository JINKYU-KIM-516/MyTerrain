#pragma once
#include "../GameObject/Component.h"
#include "GridMesh.h"
#include "ChunkGrid.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <unordered_map>
#include <vector>

// 청크 메시의 표시 방식. TerrainRenderer 의 TerrainDisplayMode 와 값이 같지만,
// 이 모듈이 TerrainRenderer.h(7개 기법의 상태가 전부 들어있는 큰 헤더)를 끌어오지
// 않도록 따로 둔다.
enum class ChunkDisplayMode
{
    SolidWireframe = 0,
    Wireframe,
    Solid,
    Count
};

const wchar_t* ToDisplayName(ChunkDisplayMode mode);

// 10번 무한 지형 청크의 렌더러 겸 스트리밍 관리자.
//
// ---------------------------------------------------------------------------
// 왜 TerrainRenderer 를 청크마다 하나씩 붙이지 않는가
// ---------------------------------------------------------------------------
// 청크 하나 = GameObject 하나 + TerrainRenderer 하나로 두면 코드는 짧아지지만,
// TerrainRenderer 는 인스턴스마다 BasicTerrain.hlsl 을 컴파일하고 래스터라이저/깊이
// 상태와 상수 버퍼를 따로 만든다. 청크가 25개면 그게 25벌이 되고, 청크를 버릴 때마다
// 그 전부를 다시 만들게 된다. 스트리밍 기법에서 가장 피해야 할 비용이다.
//
// 그래서 SkyRenderer / CloudRenderer 와 같은 방식(기법 전용 렌더러가 자기 파이프라인을
// 하나 소유한다)을 따르되, 정점 버퍼만 청크 수만큼 들고 있는다.
//
// ---------------------------------------------------------------------------
// 인덱스 버퍼는 한 벌만 만든다
// ---------------------------------------------------------------------------
// 모든 청크가 같은 분할 수를 쓰므로(균일 해상도) 삼각형 연결 관계가 완전히 동일하다.
// 즉 인덱스 버퍼의 내용도 모든 청크가 똑같다 -- 한 벌만 만들어 전부가 공유한다.
// 128 분할이면 청크당 약 400KB 를 아끼는 셈이고, 청크를 만들 때 할 일도 그만큼 준다.
//
// ---------------------------------------------------------------------------
// 정점 버퍼는 풀링한다
// ---------------------------------------------------------------------------
// 같은 이유로 정점 버퍼의 크기도 모든 청크가 같다. 그래서 청크를 버릴 때 버퍼를
// 파괴하지 않고 풀에 넣어 뒀다가, 새 청크를 만들 때 꺼내 UpdateSubresource 로 덮어쓴다.
// (TerrainRenderer 는 IMMUTABLE 버퍼를 쓰지만, 여기서는 덮어쓸 수 있어야 하므로
//  D3D11_USAGE_DEFAULT 로 만든다)
class InfiniteTerrainRenderer : public Component
{
public:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    void Start() override;
    void Update(float deltaTime) override;
    void Render() override;
    void Destroy() override;

    // ---------------- 지형 정의 ----------------
    // 높이 함수는 "월드 좌표"를 받는다. 청크 경계가 자동으로 이어지는 것은 이 함수가
    // 결정적이기 때문이다 (같은 월드 좌표 -> 언제나 같은 높이).
    void SetHeightFunction(const GridMesh::HeightFunc& heightFunc);

    // 청크 한 변의 분할 수와 셀 크기. 청크 한 변의 월드 크기 = divisions * cellSize.
    // 바꾸면 이미 만들어 둔 청크와 버퍼 풀을 전부 버리고 처음부터 다시 만든다.
    void SetChunkResolution(int divisions, float cellSize);
    int   GetDivisions() const { return m_divisions; }
    float GetCellSize()  const { return m_cellSize; }
    float GetChunkWorldSize() const { return m_divisions * m_cellSize; }

    // 유지 반경(칸). 실제 유지 개수는 (2*radius+1)^2 개다.
    void SetKeepRadius(int radius);
    int  GetKeepRadius() const { return m_keepRadius; }

    // 해제 반경 = 유지 반경 + 1 (히스테리시스).
    // 카메라가 청크 경계 위에서 왔다 갔다 할 때 같은 청크를 만들었다 버렸다 하는 것을 막는다.
    int GetReleaseRadius() const { return m_keepRadius + 1; }

    // 한 프레임에 새로 만들 청크 수의 상한. 이것이 이 기법에서 프레임 히치를 막는
    // 1차 방어선이다 (2차는 "가까운 청크부터" 라는 우선순위).
    void SetMaxBuildsPerFrame(int count);
    int  GetMaxBuildsPerFrame() const { return m_maxBuildsPerFrame; }

    // 스트리밍 일시정지. 카메라만 움직여서 "지금 어디까지 로드돼 있는지" 를 눈으로
    // 확인하기 위한 관찰용 스위치다 (7번의 프리즈와 같은 성격).
    void SetStreamingPaused(bool paused) { m_streamingPaused = paused; }
    bool IsStreamingPaused() const { return m_streamingPaused; }

    // 지금 올라와 있는 청크를 전부 버린다(버퍼는 풀로 반납). 높이 함수가 바뀌었을 때 쓴다.
    void RebuildAll();

    // ---------------- 표시 ----------------
    void SetDisplayMode(ChunkDisplayMode mode) { m_displayMode = mode; }
    ChunkDisplayMode GetDisplayMode() const { return m_displayMode; }
    void CycleDisplayMode();

    void SetSolidColor(float r, float g, float b) { m_solidColor = { r, g, b, 1.0f }; }
    void SetWireColor(float r, float g, float b) { m_wireColor = { r, g, b, 1.0f }; }
    void SetLightDirection(float x, float y, float z) { m_lightDirection = { x, y, z }; }
    void SetCheckerScale(float scale) { m_checkerScale = (scale > 0.0f) ? scale : 1.0f; }

    // 청크 색상 모드: 청크마다 다른 색을 칠한다.
    // 이 기법은 "언제 만들어지고 언제 사라지는지" 가 보이지 않으면 학습 효과가 없다.
    void SetChunkColorMode(bool enabled) { m_chunkColorMode = enabled; }
    bool IsChunkColorMode() const { return m_chunkColorMode; }

    // 새로 만들어진 청크를 잠깐 강조해서 보여준다(약 0.9초에 걸쳐 원래 색으로 돌아간다).
    void SetHighlightNewChunks(bool enabled) { m_highlightNewChunks = enabled; }
    bool IsHighlightNewChunks() const { return m_highlightNewChunks; }

    // 절두체 컬링. 유지 반경 안이라도 화면 뒤쪽 청크는 그리지 않는다.
    // (생성/해제와는 무관하다 -- 컬링은 그리기만 건너뛴다)
    void SetFrustumCullingEnabled(bool enabled) { m_frustumCullingEnabled = enabled; }
    bool IsFrustumCullingEnabled() const { return m_frustumCullingEnabled; }

    // ---------------- 안개 ----------------
    // 로드 반경 끝에서 청크가 사라지는 경계를 배경색으로 덮는다.
    // 거리는 유지 반경과 청크 크기에서 자동으로 계산한다(반경을 바꾸면 같이 따라간다).
    void SetFogEnabled(bool enabled) { m_fogEnabled = enabled; }
    bool IsFogEnabled() const { return m_fogEnabled; }
    void SetFogColor(float r, float g, float b) { m_fogColor = { r, g, b, 1.0f }; }

    float GetFogStart() const { return m_fogStart; }
    float GetFogEnd()   const { return m_fogEnd; }

    // ---------------- 통계 (HUD 표시용) ----------------
    ChunkGrid::Coord GetCenterCoord() const { return m_centerCoord; }

    size_t GetLoadedChunkCount() const { return m_chunks.size(); }
    size_t GetDesiredChunkCount() const
    {
        const size_t side = static_cast<size_t>(2 * m_keepRadius + 1);
        return side * side;
    }
    size_t GetQueuedChunkCount() const { return m_queuedCount; }
    size_t GetPooledBufferCount() const { return m_vertexBufferPool.size(); }

    size_t GetDrawnChunkCount() const { return m_drawnChunkCount; }
    size_t GetDrawnTriangleCount() const { return m_drawnTriangleCount; }
    size_t GetTrianglesPerChunk() const
    {
        return static_cast<size_t>(m_divisions) * static_cast<size_t>(m_divisions) * 2;
    }

    int    GetBuiltLastFrame() const { return m_builtLastFrame; }
    size_t GetTotalBuiltCount() const { return m_totalBuilt; }
    size_t GetTotalReleasedCount() const { return m_totalReleased; }

    double GetLastBuildMilliseconds() const { return m_lastBuildMs; }
    double GetAverageBuildMilliseconds() const { return m_avgBuildMs; }
    double GetLastFrameBuildMilliseconds() const { return m_lastFrameBuildMs; }

    bool IsReady() const { return m_resourcesReady; }

private:
    // InfiniteTerrain.hlsl 의 cbuffer CBInfinite 와 메모리 배치가 같아야 한다 (208바이트).
    struct ChunkConstants
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4X4 worldViewProj;
        DirectX::XMFLOAT4   baseColor;
        DirectX::XMFLOAT3   lightDirection;
        float               useLighting;
        DirectX::XMFLOAT3   cameraPosition;
        float               cellSize;
        DirectX::XMFLOAT4   fogParams;   // x = 시작, y = 끝, z = 예약, w = 켬/끔
        DirectX::XMFLOAT4   fogColor;
    };

    // SkyRenderer / CloudRenderer 와 같은 안전장치. 셰이더의 cbuffer 와 어긋나면
    // 화면이 깨지는 대신 컴파일 시점에 바로 잡힌다.
    static_assert(sizeof(ChunkConstants) == 208, "InfiniteTerrain.hlsl 의 CBInfinite 와 크기가 어긋났습니다");

    // 올라와 있는 청크 하나.
    struct Chunk
    {
        ChunkGrid::Coord coord;
        ComPtr<ID3D11Buffer> vertexBuffer;

        // 월드 공간 AABB (절두체 컬링용). y 범위는 생성할 때 실제 정점에서 잰다.
        DirectX::XMFLOAT3 boundsMin{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 boundsMax{ 0.0f, 0.0f, 0.0f };

        float age = 0.0f;   // 만들어진 뒤 흐른 시간(초). 신규 청크 강조에 쓴다.
    };

    bool CreateDeviceResources();
    bool EnsureIndexBuffer(const GridMesh::MeshData& mesh, ID3D11Device* device);

    // 분할 수/셀 크기가 바뀌었을 때 전부 버리고 시작점으로 되돌린다.
    void ApplyLayoutChange();

    void UpdateFogDistances();

    void UpdateStreaming(float deltaTime, const DirectX::XMFLOAT3& cameraPosition);
    bool BuildChunk(const ChunkGrid::Coord& coord, ID3D11Device* device, ID3D11DeviceContext* context);
    void ReleaseChunk(Chunk& chunk);
    void ReleaseAllChunks();

    void UpdateConstantBuffer(ID3D11DeviceContext* context,
                              const Chunk& chunk,
                              const DirectX::XMMATRIX& viewProj,
                              const DirectX::XMFLOAT3& cameraPosition,
                              const DirectX::XMFLOAT4& baseColor,
                              bool useLighting);

    DirectX::XMFLOAT4 ResolveChunkColor(const Chunk& chunk) const;

private:
    // ---- 지형 정의 ----
    GridMesh::HeightFunc m_heightFunc;

    int   m_divisions = 128;
    float m_cellSize = 1.0f;

    int  m_keepRadius = 2;
    int  m_maxBuildsPerFrame = 2;
    bool m_streamingPaused = false;

    // ---- 올라와 있는 청크 ----
    std::unordered_map<uint64_t, Chunk> m_chunks;

    // 매 프레임 다시 계산하는 임시 버퍼들 (재할당을 피하려고 멤버로 들고 있는다)
    std::vector<ChunkGrid::Coord> m_desired;
    std::vector<uint64_t>         m_releaseScratch;
    std::vector<const Chunk*>     m_drawList;

    // 해제된 정점 버퍼 보관소. 모든 청크의 정점 수가 같아서 그대로 재사용할 수 있다.
    std::vector<ComPtr<ID3D11Buffer>> m_vertexBufferPool;

    ChunkGrid::Coord m_centerCoord{ 0, 0 };
    bool m_hasCenter = false;

    // ---- D3D ----
    ComPtr<ID3D11VertexShader>   m_vertexShader;
    ComPtr<ID3D11PixelShader>    m_pixelShader;
    ComPtr<ID3D11InputLayout>    m_inputLayout;
    ComPtr<ID3D11Buffer>         m_constantBuffer;
    ComPtr<ID3D11Buffer>         m_indexBuffer;      // 모든 청크 공용
    ComPtr<ID3D11RasterizerState> m_solidRasterizer;
    ComPtr<ID3D11RasterizerState> m_wireRasterizer;
    ComPtr<ID3D11DepthStencilState> m_depthState;

    UINT   m_indexCount = 0;
    UINT   m_vertexBufferBytes = 0;

    bool m_resourcesReady = false;
    bool m_resourceCreationFailed = false;
    bool m_layoutDirty = false;

    // ---- 표시 ----
    ChunkDisplayMode m_displayMode = ChunkDisplayMode::Solid;

    DirectX::XMFLOAT4 m_solidColor{ 0.40f, 0.52f, 0.36f, 1.0f };
    DirectX::XMFLOAT4 m_wireColor{ 0.92f, 0.96f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 m_lightDirection{ 0.45f, -1.0f, 0.35f };
    float m_checkerScale = 8.0f;

    bool m_chunkColorMode = true;
    bool m_highlightNewChunks = true;
    bool m_frustumCullingEnabled = true;

    // ---- 안개 ----
    bool m_fogEnabled = true;
    DirectX::XMFLOAT4 m_fogColor{ 0.15f, 0.35f, 0.65f, 1.0f };   // 배경 클리어 색과 같게
    float m_fogStart = 0.0f;
    float m_fogEnd = 0.0f;

    // ---- 통계 ----
    size_t m_queuedCount = 0;
    size_t m_drawnChunkCount = 0;
    size_t m_drawnTriangleCount = 0;

    int    m_builtLastFrame = 0;
    size_t m_totalBuilt = 0;
    size_t m_totalReleased = 0;

    double m_lastBuildMs = 0.0;
    double m_avgBuildMs = 0.0;
    double m_lastFrameBuildMs = 0.0;
};
