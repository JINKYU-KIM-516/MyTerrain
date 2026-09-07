#pragma once
#include <windows.h>
#include <string>
#include "Window.h"
#include "D3DRenderer.h"
#include "Time.h"
#include "InputManager.h"
#include "../GameObject/Scene.h"

// 엔진의 최상위 클래스.
// Window / D3DRenderer / Time / InputManager / Scene 을 소유하고
// WinMain에서 Initialize -> Run -> Shutdown 순서로 호출한다.
class Framework
{
public:
    Framework() = default;
    ~Framework();

    bool Initialize(HINSTANCE hInstance, int width, int height, const std::wstring& title);

    // 메시지 루프 + 게임 루프. 창이 닫히면 반환한다.
    void Run();

    void Shutdown();

    Scene& GetScene() { return m_scene; }
    D3DRenderer& GetRenderer() { return m_renderer; }
    Window& GetWindow() { return m_window; }
    Time& GetTime() { return m_time; }

    // 백버퍼를 클리어할 색상 (기본값: 파란색)
    void SetClearColor(float r, float g, float b, float a = 1.0f) { m_clearColor[0] = r; m_clearColor[1] = g; m_clearColor[2] = b; m_clearColor[3] = a; }

private:
    Window m_window;
    D3DRenderer m_renderer;
    Time m_time;
    Scene m_scene;

    // 기본 클리어 색상: 파란색 (Cornflower Blue 계열)
    float m_clearColor[4] = { 0.15f, 0.35f, 0.65f, 1.0f };

    bool m_initialized = false;
};
