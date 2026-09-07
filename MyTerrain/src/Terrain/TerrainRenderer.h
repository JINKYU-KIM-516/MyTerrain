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
    // BasicTerrain.hlsl 의 cbuffer CBTerrain 과 메모리 배치가 같아야 한다 (176바이트)
    struct TerrainConstants
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4X4 worldViewProj;
        DirectX::XMFLOAT4   baseColor;
        DirectX::XMFLOAT3   lightDirection;
        float               useLighting;
        DirectX::XMFLOAT3   cameraPosition;
        float               cellSize;
    };

    // ---- 그리드 파라미터 ----
    int   m_divisionsX = 64;
    int   m_divisionsZ = 64;
    float m_cellSize = 1.0f;
    GridMesh::HeightFunc m_heightFunc;

    size_t m_vertexCount = 0;
    size_t m_triangleCount = 0;
    UINT   m_indexCount = 0;

    bool m_meshDirty = true;

    // ---- 표시 설정 ----
    TerrainDisplayMode m_displayMode = TerrainDisplayMode::SolidWireframe;
    DirectX::XMFLOAT4  m_solidColor{ 0.36f, 0.58f, 0.34f, 1.0f };
    DirectX::XMFLOAT4  m_wireColor{ 0.92f, 0.96f, 1.0f, 1.0f };
    DirectX::XMFLOAT3  m_lightDirection{ 0.5f, -1.0f, 0.35f };

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
