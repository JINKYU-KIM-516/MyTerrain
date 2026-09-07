#pragma once
#include <windows.h>
#include <string>
#include <functional>

// Win32 윈도우 생성 및 메시지 처리를 담당하는 클래스
class Window
{
public:
    using ResizeCallback = std::function<void(UINT width, UINT height)>;

    Window() = default;
    ~Window();

    bool Create(HINSTANCE hInstance, int width, int height, const std::wstring& title);

    // 메시지 큐를 비운다. 종료 메시지(WM_QUIT)를 받으면 false 반환
    bool ProcessMessages();

    HWND GetHandle() const { return m_hwnd; }
    int  GetWidth() const { return m_width; }
    int  GetHeight() const { return m_height; }

    // 창 크기가 바뀔 때 호출될 콜백 (D3DRenderer의 스왑체인 리사이즈 등에 사용)
    void SetResizeCallback(const ResizeCallback& callback) { m_resizeCallback = callback; }

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    int m_width = 0;
    int m_height = 0;
    bool m_isRunning = false;

    ResizeCallback m_resizeCallback;

    static constexpr wchar_t kWindowClassName[] = L"MyTerrainWindowClass";
};
