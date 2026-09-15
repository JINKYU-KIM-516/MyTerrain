#include "SkyControlComponent.h"
#include "../Terrain/SkyRenderer.h"
#include "../UI/UIText.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace
{
    // Y 키로 순환할 프리셋 (자정 / 일출 / 정오 / 일몰). SkyRenderer.cpp 의 키프레임과
    // 시각이 같은 자리를 가리키도록 맞춰뒀다.
    constexpr float kPresets[] = { 0.0f, 0.27f, 0.5f, 0.73f };
    constexpr int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));
}

void SkyControlComponent::Start()
{
    RefreshInfoText();
}

void SkyControlComponent::Update(float deltaTime)
{
    if (m_sky == nullptr)
    {
        return;
    }

    // 시간 스크럽 / 자동 재생 켬끔 / 프리셋 순환 / 태양 각크기 / 글로우 / 기본값은
    // 전부 화면 버튼(StepTimeBackward 등)으로 옮겨졌다. 여기서는 자동 재생만 흐른다.

    if (m_autoPlay)
    {
        m_sky->SetTimeOfDay(m_sky->GetTimeOfDay() + kAutoPlaySpeed * deltaTime);
    }

    // ---- HUD 갱신 ----
    m_refreshTimer -= deltaTime;
    if (m_refreshTimer <= 0.0f)
    {
        m_refreshTimer = kRefreshInterval;
        RefreshInfoText();
    }
}

// ---- 버튼용 동작 (예전 ',' 키. 버튼도 SetRepeatWhileHeld(true) 로 등록한다) ----
void SkyControlComponent::StepTimeBackward()
{
    if (m_sky == nullptr)
    {
        return;
    }

    m_autoPlay = false;
    m_sky->SetTimeOfDay(m_sky->GetTimeOfDay() - kScrubStep);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 '.' 키) ----
void SkyControlComponent::StepTimeForward()
{
    if (m_sky == nullptr)
    {
        return;
    }

    m_autoPlay = false;
    m_sky->SetTimeOfDay(m_sky->GetTimeOfDay() + kScrubStep);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 T 키) ----
void SkyControlComponent::ToggleAutoPlay()
{
    m_autoPlay = !m_autoPlay;
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 Y 키) ----
void SkyControlComponent::CyclePreset()
{
    if (m_sky == nullptr)
    {
        return;
    }

    m_presetIndex = (m_presetIndex + 1) % kPresetCount;
    m_autoPlay = false;
    m_sky->SetTimeOfDay(kPresets[m_presetIndex]);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 I 키) ----
void SkyControlComponent::DecreaseSunSize()
{
    if (m_sky == nullptr)
    {
        return;
    }

    m_sky->SetSunAngularRadius(m_sky->GetSunAngularRadiusDegrees() * 0.8f);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 K 키) ----
void SkyControlComponent::IncreaseSunSize()
{
    if (m_sky == nullptr)
    {
        return;
    }

    m_sky->SetSunAngularRadius(m_sky->GetSunAngularRadiusDegrees() * 1.25f);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 U 키) ----
// 지수를 키우면 sunGlow = sunDot^지수 가 더 좁고 진해지므로 "약하게(좁게)" (Sky.hlsl 참고).
void SkyControlComponent::IncreaseGlowExponent()
{
    if (m_sky == nullptr)
    {
        return;
    }

    m_sky->SetSunGlowExponent(m_sky->GetSunGlowExponent() * 1.3f);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 J 키) ----
// 지수를 줄이면 더 넓고 은은해지므로 "강하게(넓게)".
void SkyControlComponent::DecreaseGlowExponent()
{
    if (m_sky == nullptr)
    {
        return;
    }

    m_sky->SetSunGlowExponent(m_sky->GetSunGlowExponent() * 0.77f);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 0 키) ----
void SkyControlComponent::ResetToDefault()
{
    if (m_sky == nullptr)
    {
        return;
    }

    m_autoPlay = false;
    m_presetIndex = 2;
    m_sky->SetTimeOfDay(0.5f);
    m_sky->SetSunAngularRadius(1.5f);
    m_sky->SetSunGlowExponent(32.0f);
    RefreshInfoText();
}

void SkyControlComponent::RefreshInfoText()
{
    if (m_infoText == nullptr || m_sky == nullptr)
    {
        return;
    }

    const float t = m_sky->GetTimeOfDay();
    const float hours = t * 24.0f;
    const int hh = static_cast<int>(hours);
    const int mm = static_cast<int>((hours - static_cast<float>(hh)) * 60.0f);

    std::wostringstream oss;
    oss << L"[스카이]\n";
    oss << L"시간대       " << std::setfill(L'0') << std::setw(2) << hh << L":" << std::setw(2) << mm
        << (m_autoPlay ? L" (자동 재생)" : L"") << L"\n";
    oss << L"태양 고도    " << std::fixed << std::setprecision(1) << m_sky->GetSunElevationDegrees() << L"도\n";
    oss << L"태양 각크기  " << std::fixed << std::setprecision(2) << m_sky->GetSunAngularRadiusDegrees() << L"도\n";
    oss << L"글로우 지수  " << std::fixed << std::setprecision(0) << m_sky->GetSunGlowExponent();

    m_infoText->SetText(oss.str());
}
