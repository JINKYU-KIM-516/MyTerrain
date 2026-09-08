#include "SplatControlComponent.h"
#include "../Framework/InputManager.h"
#include "../Terrain/TerrainRenderer.h"
#include "../UI/UIText.h"

#include <algorithm>
#include <cwchar>

namespace
{
    // GridControlComponent::FormatFixed 와 같은 모양. SplatControlComponent 는
    // GridControlComponent 를 상속하지 않으므로(HeightMapControlComponent 가 이미
    // 그 자리를 쓰고 있어서) 여기 따로 둔다.
    std::wstring FormatFixed(float value, int decimals)
    {
        wchar_t buffer[64] = {};
        std::swprintf(buffer, 64, L"%.*f", decimals, value);
        return buffer;
    }
}

void SplatControlComponent::Start()
{
    std::wstring error;
    if (!m_textures.LoadFixedSet(&error))
    {
        // 텍스처가 없어도 프로그램이 죽어서는 안 된다 -- 스플래팅만 강제로 끄고
        // 3번과 같은 고도 색상/체커로 대신 보여준다. HUD 에 이유를 남긴다.
        m_loadError = error.empty() ? L"스플래팅 텍스처를 읽지 못했습니다." : error;
    }

    ApplySplatParams();
    RefreshInfoText();
}

void SplatControlComponent::Destroy()
{
    if (m_terrain != nullptr)
    {
        m_terrain->SetSplatResources(nullptr, nullptr);
        m_terrain->SetSplatMode(false);
    }

    m_textures.Reset();
}

void SplatControlComponent::ApplySplatParams()
{
    if (m_terrain == nullptr)
    {
        return;
    }

    // 디바이스가 Start 시점에 아직 준비되지 않았을 수 있으니 여기서 한 번 더 시도한다
    // (HeightMapControlComponent::UploadTexture 와 같은 이유).
    if (!m_textures.IsReady())
    {
        std::wstring error;
        if (m_textures.LoadFixedSet(&error))
        {
            m_loadError.clear();
        }
    }

    if (m_textures.IsReady())
    {
        m_terrain->SetSplatResources(m_textures.GetSRV(), m_textures.GetSampler());
    }

    m_terrain->SetSplatParams(m_tiling, m_slopeStart, m_slopeEnd);
    m_terrain->SetSplatMode(m_splatMode && m_textures.IsReady());
}

void SplatControlComponent::Update(float deltaTime)
{
    if (m_terrain == nullptr)
    {
        return;
    }

    // 디바이스가 Start 시점에 아직 준비되지 않았을 수 있어, 텍스처가 올라올 때까지
    // 매 프레임 다시 시도한다 (HeightMapControlComponent::UploadTexture 와 같은 이유).
    if (!m_textures.IsReady())
    {
        ApplySplatParams();
    }

    InputManager& input = InputManager::GetInstance();
    bool changed = false;

    if (input.IsKeyPressed('V'))
    {
        m_splatMode = !m_splatMode;
        changed = true;
    }

    if (input.IsKeyPressed('I'))
    {
        m_tiling = std::clamp(m_tiling * 1.15f, 0.002f, 2.0f);
        changed = true;
    }
    if (input.IsKeyPressed('K'))
    {
        m_tiling = std::clamp(m_tiling / 1.15f, 0.002f, 2.0f);
        changed = true;
    }

    // 경사 구간을 위(U)/아래(J)로 넓힌다 -- 시작점은 내려가고 끝점은 올라간다.
    // 최소 0.05 간격은 항상 유지한다 (구간이 0으로 접히면 smoothstep 이 계단이 된다).
    if (input.IsKeyPressed('U'))
    {
        m_slopeStart = std::clamp(m_slopeStart - 0.05f, 0.0f, m_slopeEnd - 0.05f);
        m_slopeEnd = std::clamp(m_slopeEnd + 0.05f, m_slopeStart + 0.05f, 1.0f);
        changed = true;
    }
    if (input.IsKeyPressed('J'))
    {
        const float mid = (m_slopeStart + m_slopeEnd) * 0.5f;
        m_slopeStart = std::clamp(mid - 0.025f, 0.0f, mid);
        m_slopeEnd = std::clamp(mid + 0.025f, mid, 1.0f);
        changed = true;
    }

    if (input.IsKeyPressed('0') || input.IsKeyPressed(VK_NUMPAD0))
    {
        m_tiling = kDefaultTiling;
        m_slopeStart = kDefaultSlopeStart;
        m_slopeEnd = kDefaultSlopeEnd;
        m_splatMode = true;
        changed = true;
    }

    if (changed)
    {
        ApplySplatParams();
    }

    m_refreshTimer += deltaTime;
    if (changed || m_refreshTimer >= kRefreshInterval)
    {
        m_refreshTimer = 0.0f;
        RefreshInfoText();
    }
}

void SplatControlComponent::RefreshInfoText()
{
    if (m_infoText == nullptr)
    {
        return;
    }

    std::wstring text = L"[스플래팅]  켬/끔 V   타일링 I/K   경사 구간 넓히기/좁히기 U/J   기본값 0\n";

    if (!m_loadError.empty())
    {
        text += m_loadError;
        text += L"\n(MyTerrain/textures/ 에 splat_sand.png 등 4장이 있는지 확인)";
    }
    else
    {
        const bool activelyOn = m_splatMode && m_textures.IsReady();
        text += L"모드 : " + std::wstring(activelyOn ? L"켬" : L"끔");
        text += L"   타일링 : " + FormatFixed(m_tiling, 3);
        text += L" (텍스처 1칸 ≈ " + FormatFixed(1.0f / std::max(m_tiling, 0.0001f), 1) + L" 월드단위)";
        text += L"\n경사 임계값 (0=평지, 1=수직) : " + FormatFixed(m_slopeStart, 2) + L" ~ " + FormatFixed(m_slopeEnd, 2);
        text += L"\n레이어 : 모래 -> 잔디 -> 바위 -> 눈 (높이) , 급경사는 바위로 전이";
    }

    m_infoText->SetText(text);
}
