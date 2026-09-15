#include "CloudControlComponent.h"
#include "../Terrain/CloudRenderer.h"
#include "../Terrain/SkyRenderer.h"
#include "../UI/UIText.h"
#include "../Framework/InputManager.h"
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

    InputManager& input = InputManager::GetInstance();

    // ---- 스카이 시간대 동기화 (SkyRenderer 는 건드리지 않고 값만 읽어온다) ----
    if (m_sky != nullptr)
    {
        m_cloud->SetSunDirection(m_sky->GetSunDirection());
        m_cloud->SetSunElevationDegrees(m_sky->GetSunElevationDegrees());
    }

    if (input.IsKeyPressed('L'))
    {
        m_visible = !m_visible;
        m_cloud->SetEnabled(m_visible);
    }

    // 임계값을 올리면(G) fbm 밀도가 넘기 어려워져서 구름이 적어지고,
    // 내리면(H) 더 쉽게 넘어서 구름이 많아진다.
    if (input.IsKeyPressed('G'))
    {
        m_cloud->SetCoverage(m_cloud->GetCoverage() + 0.03f);
    }
    if (input.IsKeyPressed('H'))
    {
        m_cloud->SetCoverage(m_cloud->GetCoverage() - 0.03f);
    }

    if (input.IsKeyPressed('V'))
    {
        m_cloud->SetWarpStrength(m_cloud->GetWarpStrength() * 0.8f);
    }
    if (input.IsKeyPressed('B'))
    {
        m_cloud->SetWarpStrength(m_cloud->GetWarpStrength() * 1.25f);
    }

    if (input.IsKeyPressed('O'))
    {
        m_cloud->SetWindSpeed(m_cloud->GetWindSpeed() * 0.7f);
    }
    if (input.IsKeyPressed('P'))
    {
        m_cloud->SetWindSpeed(m_cloud->GetWindSpeed() * 1.4f);
    }

    if (input.IsKeyPressed('0') || input.IsKeyPressed(VK_NUMPAD0))
    {
        m_visible = true;
        m_cloud->SetEnabled(true);
        m_cloud->SetCoverage(0.55f);
        m_cloud->SetSoftness(0.08f);
        m_cloud->SetNoiseScale(2.2f);
        m_cloud->SetWarpStrength(1.6f);
        m_cloud->SetWindDirectionDegrees(35.0f);
        m_cloud->SetWindSpeed(0.05f);
        m_cloud->SetRimPower(4.0f);
    }

    // ---- HUD 갱신 ----
    m_refreshTimer -= deltaTime;
    if (m_refreshTimer <= 0.0f)
    {
        m_refreshTimer = kRefreshInterval;
        RefreshInfoText();
    }
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
