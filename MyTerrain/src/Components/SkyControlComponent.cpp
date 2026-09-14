#include "SkyControlComponent.h"
#include "../Terrain/SkyRenderer.h"
#include "../UI/UIText.h"
#include "../Framework/InputManager.h"
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

    InputManager& input = InputManager::GetInstance();

    if (input.IsKeyPressed('T'))
    {
        m_autoPlay = !m_autoPlay;
    }

    if (input.IsKeyPressed('Y'))
    {
        m_presetIndex = (m_presetIndex + 1) % kPresetCount;
        m_autoPlay = false;
        m_sky->SetTimeOfDay(kPresets[m_presetIndex]);
    }

    if (input.IsKeyPressed('I'))
    {
        m_sky->SetSunAngularRadius(m_sky->GetSunAngularRadiusDegrees() * 0.8f);
    }
    if (input.IsKeyPressed('K'))
    {
        m_sky->SetSunAngularRadius(m_sky->GetSunAngularRadiusDegrees() * 1.25f);
    }

    // 지수를 키우면 sunGlow = sunDot^지수 가 더 좁고 진해지므로 "약하게(좁게)",
    // 줄이면 더 넓고 은은해지므로 "강하게(넓게)" 로 안내 문구를 붙였다 (Sky.hlsl 참고).
    if (input.IsKeyPressed('U'))
    {
        m_sky->SetSunGlowExponent(m_sky->GetSunGlowExponent() * 1.3f);
    }
    if (input.IsKeyPressed('J'))
    {
        m_sky->SetSunGlowExponent(m_sky->GetSunGlowExponent() * 0.77f);
    }

    if (input.IsKeyPressed('0') || input.IsKeyPressed(VK_NUMPAD0))
    {
        m_autoPlay = false;
        m_presetIndex = 2;
        m_sky->SetTimeOfDay(0.5f);
        m_sky->SetSunAngularRadius(1.5f);
        m_sky->SetSunGlowExponent(32.0f);
    }

    // ---- , / . 로 시간 스크럽 (누르고 있으면 연속 -- GridControlComponent 와 같은 패턴) ----
    const bool scrubBack = input.IsKeyDown(VK_OEM_COMMA);      // ','
    const bool scrubForward = input.IsKeyDown(VK_OEM_PERIOD);  // '.'
    const int direction = scrubForward ? 1 : (scrubBack ? -1 : 0);

    if (direction != m_repeatDirection)
    {
        m_repeatDirection = direction;
        m_repeatTimer = 0.0f;
        m_repeatStarted = false;
    }

    if (direction != 0)
    {
        bool shouldStep = false;

        if (!m_repeatStarted)
        {
            shouldStep = true;
            m_repeatStarted = true;
            m_repeatTimer = kRepeatDelay;
        }
        else
        {
            m_repeatTimer -= deltaTime;
            if (m_repeatTimer <= 0.0f)
            {
                shouldStep = true;
                m_repeatTimer = kRepeatInterval;
            }
        }

        if (shouldStep)
        {
            m_autoPlay = false;
            m_sky->SetTimeOfDay(m_sky->GetTimeOfDay() + kScrubStep * static_cast<float>(direction));
        }
    }

    // ---- 자동 재생 ----
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
