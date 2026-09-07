#pragma once
#include <cstdint>

// 게임 루프의 델타타임 / 총 경과시간을 관리하는 클래스
// QueryPerformanceCounter 기반 고정밀 타이머
class Time
{
public:
    Time();

    void Reset();     // 타이머 초기화 (게임 시작 시 1회 호출)
    void Tick();       // 매 프레임 호출하여 델타타임 갱신

    float GetDeltaTime() const { return m_deltaTime; }
    float GetTotalTime() const { return m_totalTime; }
    float GetFPS() const { return m_fps; }

private:
    int64_t m_frequency = 0;
    int64_t m_startTime = 0;
    int64_t m_prevTime = 0;

    float m_deltaTime = 0.0f;
    float m_totalTime = 0.0f;

    // FPS 계산용
    float m_fpsTimeAccumulator = 0.0f;
    int   m_fpsFrameCount = 0;
    float m_fps = 0.0f;
};
