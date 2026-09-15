#include "CloudControlComponent.h"
#include "../Terrain/CloudRenderer.h"
#include "../Terrain/SkyRenderer.h"
#include "../UI/UIText.h"
#include <sstream>
#include <iomanip>

void CloudControlComponent::Start()
{
    RefreshInfoText();
}

void CloudControlComponent::Update(float deltaTime)
{
    if (m_cloud == nullptr)
    {
        return;
    }

    // ---- 스카이 시간대 동기화 (SkyRenderer 는 건드리지 않고 값만 읽어온다) ----
    if (m_sky != nullptr)
    {
        m_cloud->SetSunDirection(m_sky->GetSunDirection());
        m_cloud->SetSunElevationDegrees(m_sky->GetSunElevationDegrees());
    }

    // 조작(표시 켬/끔, 커버리지, 워프 세기, 바람 속도, 기본값)은 전부 화면 버튼으로
    // 옮겨졌다.

    // ---- HUD 갱신 ----
    m_refreshTimer -= deltaTime;
    if (m_refreshTimer <= 0.0f)
    {
        m_refreshTimer = kRefreshInterval;
        RefreshInfoText();
    }
}

// ---- 버튼용 동작 (예전 L 키) ----
void CloudControlComponent::ToggleVisible()
{
    if (m_cloud == nullptr)
    {
        return;
    }

    m_visible = !m_visible;
    m_cloud->SetEnabled(m_visible);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 G 키) ----
// 임계값을 올리면 fbm 밀도가 넘기 어려워져서 구름이 적어진다.
void CloudControlComponent::IncreaseCoverage()
{
    if (m_cloud == nullptr)
    {
        return;
    }

    m_cloud->SetCoverage(m_cloud->GetCoverage() + 0.03f);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 H 키) ----
// 임계값을 내리면 더 쉽게 넘어서 구름이 많아진다.
void CloudControlComponent::DecreaseCoverage()
{
    if (m_cloud == nullptr)
    {
        return;
    }

    m_cloud->SetCoverage(m_cloud->GetCoverage() - 0.03f);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 V 키) ----
void CloudControlComponent::DecreaseWarpStrength()
{
    if (m_cloud == nullptr)
    {
        return;
    }

    m_cloud->SetWarpStrength(m_cloud->GetWarpStrength() * 0.8f);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 B 키) ----
void CloudControlComponent::IncreaseWarpStrength()
{
    if (m_cloud == nullptr)
    {
        return;
    }

    m_cloud->SetWarpStrength(m_cloud->GetWarpStrength() * 1.25f);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 O 키) ----
void CloudControlComponent::DecreaseWindSpeed()
{
    if (m_cloud == nullptr)
    {
        return;
    }

    m_cloud->SetWindSpeed(m_cloud->GetWindSpeed() * 0.7f);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 P 키) ----
void CloudControlComponent::IncreaseWindSpeed()
{
    if (m_cloud == nullptr)
    {
        return;
    }

    m_cloud->SetWindSpeed(m_cloud->GetWindSpeed() * 1.4f);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 0 키) ----
void CloudControlComponent::ResetToDefault()
{
    if (m_cloud == nullptr)
    {
        return;
    }

    m_visible = true;
    m_cloud->SetEnabled(true);
    m_cloud->SetCoverage(0.55f);
    m_cloud->SetSoftness(0.08f);
    m_cloud->SetNoiseScale(2.2f);
    m_cloud->SetWarpStrength(1.6f);
    m_cloud->SetWindDirectionDegrees(35.0f);
    m_cloud->SetWindSpeed(0.05f);
    m_cloud->SetRimPower(4.0f);
    RefreshInfoText();
}

void CloudControlComponent::RefreshInfoText()
{
    if (m_infoText == nullptr || m_cloud == nullptr)
    {
        return;
    }

    std::wostringstream oss;
    oss << L"[구름]\n";
    oss << L"표시             " << (m_visible ? L"켜짐" : L"꺼짐") << L"\n";
    oss << L"커버리지 임계값  " << std::fixed << std::setprecision(2) << m_cloud->GetCoverage()
        << L" (낮을수록 구름 많음)\n";
    oss << L"워프 세기        " << std::fixed << std::setprecision(2) << m_cloud->GetWarpStrength() << L"\n";
    oss << L"바람 속도        " << std::fixed << std::setprecision(3) << m_cloud->GetWindSpeed();

    m_infoText->SetText(oss.str());
}
