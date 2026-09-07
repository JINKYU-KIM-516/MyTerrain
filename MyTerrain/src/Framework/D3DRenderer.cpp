#include "D3DRenderer.h"
#include <cassert>

bool D3DRenderer::Initialize(HWND hwnd, UINT width, UINT height)
{
    m_hwnd = hwnd;

    if (!CreateDeviceAndSwapChain(hwnd, width, height))
    {
        return false;
    }

    if (!CreateRenderTargetAndDepthStencil(width, height))
    {
        return false;
    }

    return true;
}

bool D3DRenderer::CreateDeviceAndSwapChain(HWND hwnd, UINT width, UINT height)
{
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = width;
    scd.BufferDesc.Height = height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#if defined(_DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_11_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,                    // 기본 어댑터 사용
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        requestedLevels, ARRAYSIZE(requestedLevels),
        D3D11_SDK_VERSION,
        &scd,
        &m_swapChain,
        &m_device,
        &featureLevel,
        &m_context);

    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

bool D3DRenderer::CreateRenderTargetAndDepthStencil(UINT width, UINT height)
{
    m_width = width;
    m_height = height;

    // ---- 백버퍼로부터 렌더타겟뷰 생성 ----
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
    if (FAILED(hr))
    {
        return false;
    }

    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTargetView);
    if (FAILED(hr))
    {
        return false;
    }

    // ---- 뎁스/스텐실 버퍼 및 뷰 생성 (추후 3D 지형 렌더링을 위해 미리 준비) ----
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = m_device->CreateTexture2D(&depthDesc, nullptr, &m_depthStencilBuffer);
    if (FAILED(hr))
    {
        return false;
    }

    hr = m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), nullptr, &m_depthStencilView);
    if (FAILED(hr))
    {
        return false;
    }

    // ---- 뷰포트 설정 ----
    m_viewport.TopLeftX = 0.0f;
    m_viewport.TopLeftY = 0.0f;
    m_viewport.Width = static_cast<float>(width);
    m_viewport.Height = static_cast<float>(height);
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    m_context->RSSetViewports(1, &m_viewport);
    m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());

    return true;
}

void D3DRenderer::OnResize(UINT width, UINT height)
{
    if (!m_swapChain || width == 0 || height == 0)
    {
        return;
    }

    // 기존 뷰들을 반드시 먼저 해제해야 ResizeBuffers가 성공한다
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_renderTargetView.Reset();
    m_depthStencilView.Reset();
    m_depthStencilBuffer.Reset();

    HRESULT hr = m_swapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    assert(SUCCEEDED(hr) && "SwapChain ResizeBuffers 실패");
    (void)hr;

    CreateRenderTargetAndDepthStencil(width, height);
}

void D3DRenderer::BeginFrame(const float clearColor[4])
{
    m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());
    m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
    m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void D3DRenderer::EndFrame()
{
    m_swapChain->Present(1, 0); // VSync 켜짐
}
