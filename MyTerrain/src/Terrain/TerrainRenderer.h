#pragma once
#include "../GameObject/Component.h"
#include "GridMesh.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <string>

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

private:
    // 셰이더 / 입력 레이아웃 / 래스터라이저 상태 / 상수 버퍼 생성 (최초 1회)
    bool CreateDeviceResources();

    // 현재 파라미터로 정점/인덱스 버퍼를 다시 만든다
    bool RebuildMesh();

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
