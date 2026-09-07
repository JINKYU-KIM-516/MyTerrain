#include "Time.h"
#include <windows.h>

Time::Time()
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    m_frequency = freq.QuadPart;

    Reset();
}

void Time::Reset()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    m_startTime = now.QuadPart;
    m_prevTime = now.QuadPart;

    m_deltaTime = 0.0f;
    m_totalTime = 0.0f;

    m_fpsTimeAccumulator = 0.0f;
    m_fpsFrameCount = 0;
    m_fps = 0.0f;
}

void Time::Tick()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    int64_t delta = now.QuadPart - m_prevTime;
    m_prevTime = now.QuadPart;

    m_deltaTime = static_cast<float>(delta) / static_cast<float>(m_frequency);

    // 알트탭, 브레이크포인트 등으로 인한 비정상적인 큰 델타타임 방지
    if (m_deltaTime > 0.1f)
    {
        m_deltaTime = 0.1f;
    }

    m_totalTime = static_cast<float>(now.QuadPart - m_startTime) / static_cast<float>(m_frequency);

    // FPS 계산 (1초마다 갱신)
    m_fpsFrameCount++;
    m_fpsTimeAccumulator += m_deltaTime;
    if (m_fpsTimeAccumulator >= 1.0f)
    {
        m_fps = static_cast<float>(m_fpsFrameCount) / m_fpsTimeAccumulator;
        m_fpsFrameCount = 0;
        m_fpsTimeAccumulator = 0.0f;
    }
}
