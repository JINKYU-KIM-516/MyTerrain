#include "Framework.h"

Framework::~Framework()
{
    Shutdown();
}

bool Framework::Initialize(HINSTANCE hInstance, int width, int height, const std::wstring& title)
{
    if (!m_window.Create(hInstance, width, height, title))
    {
        return false;
    }

    if (!m_renderer.Initialize(m_window.GetHandle(), width, height))
    {
        return false;
    }

    // 창 크기가 바뀌면 스왑체인/렌더타겟도 다시 생성되도록 콜백 연결
    m_window.SetResizeCallback([this](UINT w, UINT h)
        {
            m_renderer.OnResize(w, h);
        });

    m_time.Reset();

    m_initialized = true;
    return true;
}

void Framework::Run()
{
    if (!m_initialized)
    {
        return;
    }

    // 게임 루프 시작 전, 이미 생성되어 있는 모든 GameObject의 Start()를 호출
    m_scene.Start();

    bool running = true;
    while (running)
    {
        running = m_window.ProcessMessages();
        if (!running)
        {
            break;
        }

        InputManager::GetInstance().BeginFrame();

        m_time.Tick();
        const float deltaTime = m_time.GetDeltaTime();

        // ---- Update ----
        m_scene.Update(deltaTime);

        // ---- Render ----
        m_renderer.BeginFrame(m_clearColor);
        m_scene.Render();
        m_renderer.EndFrame();

        // ---- 파괴 예약된 GameObject 정리 ----
        m_scene.LateUpdate();
    }
}

void Framework::Shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    m_scene.Clear();
    m_initialized = false;
}
