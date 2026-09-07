#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

// DirectX 11 디바이스 / 스왑체인 / 렌더타겟을 관리하는 클래스
// GameObject의 Render() 단계에서 필요한 Device/Context를 제공한다.
class D3DRenderer
{
public:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    D3DRenderer() = default;
    ~D3DRenderer() = default;

    bool Initialize(HWND hwnd, UINT width, UINT height);

    // 창 크기 변경 시 스왑체인/렌더타겟/뎁스버퍼를 다시 생성한다
    void OnResize(UINT width, UINT height);

    // 한 프레임의 시작: 백버퍼 + 뎁스버퍼를 지정한 색으로 클리어
    void BeginFrame(const float clearColor[4]);

    // 한 프레임의 끝: 백버퍼를 화면에 Present
    void EndFrame();

    ID3D11Device*        GetDevice()  const { return m_device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_context.Get(); }

    UINT GetWidth()  const { return m_width; }
    UINT GetHeight() const { return m_height; }

private:
    bool CreateDeviceAndSwapChain(HWND hwnd, UINT width, UINT height);
    bool CreateRenderTargetAndDepthStencil(UINT width, UINT height);

private:
    ComPtr<ID3D11Device>           m_device;
    ComPtr<ID3D11DeviceContext>    m_context;
    ComPtr<IDXGISwapChain>         m_swapChain;

    ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    ComPtr<ID3D11Texture2D>        m_depthStencilBuffer;
    ComPtr<ID3D11DepthStencilView> m_depthStencilView;

    D3D11_VIEWPORT m_viewport = {};

    HWND m_hwnd = nullptr;
    UINT m_width = 0;
    UINT m_height = 0;
};
