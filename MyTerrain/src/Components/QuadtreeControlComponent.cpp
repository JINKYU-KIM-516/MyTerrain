#include "QuadtreeControlComponent.h"
#include "../Terrain/TerrainRenderer.h"
#include "../UI/UIText.h"

#include <algorithm>
#include <cwchar>

void QuadtreeControlComponent::Start()
{
    ApplyLeafSize();
    RefreshInfoText();
}

void QuadtreeControlComponent::ApplyLeafSize()
{
    if (m_terrain == nullptr)
    {
        return;
    }

    m_leafSizeIndex = std::clamp(m_leafSizeIndex, 0, kLeafSizeStepCount - 1);

    m_terrain->SetQuadtreeLeafSize(kLeafSizeSteps[m_leafSizeIndex]);
    m_terrain->SetQuadtreeCullingEnabled(m_cullingEnabled);
    m_terrain->SetQuadtreeDebugBoxesEnabled(m_debugBoxesEnabled);
}

void QuadtreeControlComponent::Update(float deltaTime)
{
    if (m_terrain == nullptr)
    {
        return;
    }

    // 조작(컬링, 디버그 박스, 리프 크기, 기본값)은 전부 화면 버튼으로 옮겨졌다.

    m_refreshTimer += deltaTime;
    if (m_refreshTimer >= kRefreshInterval)
    {
        m_refreshTimer = 0.0f;
        RefreshInfoText();
    }
}

// ---- 버튼용 동작 (예전 C 키) ----
void QuadtreeControlComponent::ToggleCulling()
{
    m_cullingEnabled = !m_cullingEnabled;
    if (m_terrain != nullptr)
    {
        m_terrain->SetQuadtreeCullingEnabled(m_cullingEnabled);
    }
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 B 키) ----
void QuadtreeControlComponent::ToggleDebugBoxes()
{
    m_debugBoxesEnabled = !m_debugBoxesEnabled;
    if (m_terrain != nullptr)
    {
        m_terrain->SetQuadtreeDebugBoxesEnabled(m_debugBoxesEnabled);
    }
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 ',' 키) ----
// 리프를 더 잘게 -- 목록 안에서만 오간다
void QuadtreeControlComponent::DecreaseLeafSize()
{
    if (m_leafSizeIndex > 0)
    {
        --m_leafSizeIndex;
        if (m_terrain != nullptr)
        {
            m_terrain->SetQuadtreeLeafSize(kLeafSizeSteps[m_leafSizeIndex]);
        }
        RefreshInfoText();
    }
}

// ---- 버튼용 동작 (예전 '.' 키) ----
// 리프를 더 크게 -- 목록 안에서만 오간다
void QuadtreeControlComponent::IncreaseLeafSize()
{
    if (m_leafSizeIndex < kLeafSizeStepCount - 1)
    {
        ++m_leafSizeIndex;
        if (m_terrain != nullptr)
        {
            m_terrain->SetQuadtreeLeafSize(kLeafSizeSteps[m_leafSizeIndex]);
        }
        RefreshInfoText();
    }
}

// ---- 버튼용 동작 (예전 0 키) ----
void QuadtreeControlComponent::ResetToDefault()
{
    m_leafSizeIndex = kDefaultLeafSizeIndex;
    m_cullingEnabled = true;
    m_debugBoxesEnabled = true;
    ApplyLeafSize();
    RefreshInfoText();
}

void QuadtreeControlComponent::RefreshInfoText()
{
    if (m_infoText == nullptr || m_terrain == nullptr)
    {
        return;
    }

    wchar_t buffer[512] = {};

    const size_t totalLeaves = m_terrain->GetQuadtreeLeafCount();
    const size_t visibleLeaves = m_terrain->GetQuadtreeVisibleLeafCount();
    const int leafSize = kLeafSizeSteps[m_leafSizeIndex];

    // 리프 하나당 최대 삼각형 수 = leafSize * leafSize * 2 이므로, 대략적인 Draw 호출 절감을
    // "리프 N개 중 M개만 그림" 으로 직접 보여준다 -- 컬링의 이득이 숫자로 바로 와닿게.
    std::swprintf(buffer, 512,
        L"[쿼드트리 컬링]\n"
        L"리프 크기 : %d x %d 셀   리프 개수 : %zu 개 중 %zu 개 그림 (%.0f%%)\n"
        L"컬링 : %s   디버그 박스 : %s   삼각형 총 %zu 개",
        leafSize, leafSize,
        totalLeaves, visibleLeaves,
        totalLeaves > 0 ? (100.0 * static_cast<double>(visibleLeaves) / static_cast<double>(totalLeaves)) : 0.0,
        m_cullingEnabled ? L"켬" : L"끔 (전체를 한 번에 그림)",
        m_debugBoxesEnabled ? L"켬" : L"끔",
        m_terrain->GetTriangleCount());

    m_infoText->SetText(buffer);
}
