#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include "Window.h"
#include "D3DRenderer.h"
#include "TextRenderer.h"
#include "Time.h"
#include "InputManager.h"
#include "../GameObject/Scene.h"

// 엔진의 최상위 클래스.
// Window / D3DRenderer / TextRenderer / Time / InputManager / Scene 을 소유하고
// WinMain에서 Initialize -> Run -> Shutdown 순서로 호출한다.
class Framework
{
public:
    // 한 프레임이 완전히 끝난 뒤(Update/Render/정리 이후) 호출되는 콜백.
    // 씬 전환처럼 "프레임 도중에 하면 위험한 작업"을 이 시점에 처리한다.
    using EndOfFrameCallback = std::function<void()>;

    Framework() = default;
    ~Framework();

    bool Initialize(HINSTANCE hInstance, int width, int height, const std::wstring& title);

    // 메시지 루프 + 게임 루프. 창이 닫히면 반환한다.
    void Run();

    void Shutdown();

    // 프로그램 종료를 요청한다 (게임 루프가 다음 프레임에 빠져나온다)
    void Quit();

    Scene& GetScene() { return m_scene; }
    D3DRenderer& GetRenderer() { return m_renderer; }
    TextRenderer& GetTextRenderer() { return m_textRenderer; }
    Window& GetWindow() { return m_window; }
    Time& GetTime() { return m_time; }

    void SetEndOfFrameCallback(const EndOfFrameCallback& callback) { m_endOfFrameCallback = callback; }

    // 백버퍼를 클리어할 색상 (기본값: 파란색)
    void SetClearColor(float r, float g, float b, float a = 1.0f) { m_clearColor[0] = r; m_clearColor[1] = g; m_clearColor[2] = b; m_clearColor[3] = a; }

    // 컴포넌트 등 어디에서든 프레임워크에 접근하기 위한 전역 인스턴스
    static Framework* GetInstance() { return s_instance; }

private:
    Window m_window;
    D3DRenderer m_renderer;
    TextRenderer m_textRenderer;
    Time m_time;
    Scene m_scene;

    EndOfFrameCallback m_endOfFrameCallback;

    // 기본 클리어 색상: 파란색 (Cornflower Blue 계열)
    float m_clearColor[4] = { 0.15f, 0.35f, 0.65f, 1.0f };

    bool m_initialized = false;
    bool m_quitRequested = false;

    static Framework* s_instance;
};
