#include "GridControlComponent.h"
#include "../Framework/Framework.h"
#include "../Framework/InputManager.h"
#include "../Terrain/TerrainRenderer.h"
#include "../UI/UIText.h"
#include <algorithm>
#include <string>
#include <cwchar>

// 1234567 -> "1,234,567" 형태로 (숫자가 커지면 읽기 어려우므로)
std::wstring GridControlComponent::FormatThousands(size_t value)
{
    std::wstring digits = std::to_wstring(value);
    std::wstring result;
    result.reserve(digits.size() + digits.size() / 3);

    const size_t leading = digits.size() % 3;

    for (size_t i = 0; i < digits.size(); ++i)
    {
        if (i > 0 && (i - leading) % 3 == 0)
        {
            result += L',';
        }
        result += digits[i];
    }

    return result;
}

// 소수점 아래 자리수를 지정해서 문자열로
std::wstring GridControlComponent::FormatFixed(float value, int decimals)
{
    wchar_t buffer[64] = {};
    std::swprintf(buffer, 64, L"%.*f", decimals, value);
    return buffer;
}

void GridControlComponent::SetDivisionRange(int minDivisions, int maxDivisions)
{
    m_minDivisions = std::max(1, minDivisions);
    m_maxDivisions = std::max(m_minDivisions, maxDivisions);
}

void GridControlComponent::SetCellSizeRange(float minCellSize, float maxCellSize)
{
    m_minCellSize = std::max(0.0001f, minCellSize);
    m_maxCellSize = std::max(m_minCellSize, maxCellSize);
}

void GridControlComponent::Start()
{
    RefreshInfoText();
}

void GridControlComponent::Update(float deltaTime)
{
    if (m_terrain == nullptr)
    {
        return;
    }

    InputManager& input = InputManager::GetInstance();

    bool changed = false;

    int divisions = m_terrain->GetDivisionsX();
    float cellSize = m_terrain->GetCellSize();

    // ---------------- 분할 수 (+ / -) ----------------
    const bool increaseDivisions = input.IsKeyPressed(VK_OEM_PLUS) || input.IsKeyPressed(VK_ADD);
    const bool decreaseDivisions = input.IsKeyPressed(VK_OEM_MINUS) || input.IsKeyPressed(VK_SUBTRACT);

    if (increaseDivisions && divisions < m_maxDivisions)
    {
        divisions = std::min(divisions * 2, m_maxDivisions);
        changed = true;
    }
    else if (decreaseDivisions && divisions > m_minDivisions)
    {
        divisions = std::max(divisions / 2, m_minDivisions);
        changed = true;
    }

    // ---------------- 셀 크기 ([ / ]) ----------------
    if (input.IsKeyPressed(VK_OEM_4) && cellSize > m_minCellSize)          // '['
    {
        cellSize = std::max(cellSize * 0.5f, m_minCellSize);
        changed = true;
    }
    else if (input.IsKeyPressed(VK_OEM_6) && cellSize < m_maxCellSize)     // ']'
    {
        cellSize = std::min(cellSize * 2.0f, m_maxCellSize);
        changed = true;
    }

    if (changed)
    {
        m_terrain->SetGrid(divisions, divisions, cellSize);
    }

    // ---------------- 표시 모드 (Tab) ----------------
    if (input.IsKeyPressed(VK_TAB))
    {
        m_terrain->CycleDisplayMode();
        changed = true;
    }

    // ---------------- 정보 텍스트 갱신 ----------------
    m_refreshTimer += deltaTime;
    if (changed || m_refreshTimer >= kRefreshInterval)
    {
        m_refreshTimer = 0.0f;
        RefreshInfoText();
    }
}

void GridControlComponent::RefreshInfoText()
{
    if (m_infoText == nullptr || m_terrain == nullptr)
    {
        return;
    }

    const int divisionsX = m_terrain->GetDivisionsX();
    const int divisionsZ = m_terrain->GetDivisionsZ();
    const float cellSize = m_terrain->GetCellSize();

    float fps = 0.0f;
    if (Framework* framework = Framework::GetInstance())
    {
        fps = framework->GetTime().GetFPS();
    }

    std::wstring text;
    text += L"분할 수 : " + std::to_wstring(divisionsX) + L" x " + std::to_wstring(divisionsZ);
    text += L"   셀 크기 : " + FormatFixed(cellSize, 3);
    text += L"\n전체 크기 : " + FormatFixed(divisionsX * cellSize, 1) + L" x " + FormatFixed(divisionsZ * cellSize, 1);
    // 메시는 다음 렌더링 때 다시 만들어지므로, 표시용 개수는 파라미터로부터 직접 계산한다
    const size_t vertexCount = static_cast<size_t>(divisionsX + 1) * static_cast<size_t>(divisionsZ + 1);
    const size_t triangleCount = static_cast<size_t>(divisionsX) * static_cast<size_t>(divisionsZ) * 2;

    text += L"\n정점 : " + FormatThousands(vertexCount);
    text += L"   삼각형 : " + FormatThousands(triangleCount);
    text += L"\n표시 모드 : ";
    text += ToDisplayName(m_terrain->GetDisplayMode());
    text += L"\nFPS : " + FormatFixed(fps, 1);

    m_infoText->SetText(text);
}
