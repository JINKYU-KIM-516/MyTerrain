#include "GeoLodControlComponent.h"
#include "../Terrain/TerrainRenderer.h"
#include "../UI/UIText.h"

#include <algorithm>
#include <cwchar>

void GeoLodControlComponent::Start()
{
    ApplyAll();
    RefreshInfoText();
}

void GeoLodControlComponent::ApplyAll()
{
    if (m_terrain == nullptr)
    {
        return;
    }

    m_chunkSizeIndex = std::clamp(m_chunkSizeIndex, 0, kChunkSizeStepCount - 1);
    m_levelCount = std::clamp(m_levelCount, kMinLevelCount, kMaxLevelCount);
    m_baseDistance = std::clamp(m_baseDistance, kMinBaseDistance, kMaxBaseDistance);
    m_morphWidth = std::clamp(m_morphWidth, 0.0f, 0.5f);

    m_terrain->SetLodChunkSize(kChunkSizeSteps[m_chunkSizeIndex]);
    m_terrain->SetLodLevelCount(m_levelCount);
    m_terrain->SetLodBaseDistance(m_baseDistance);
    m_terrain->SetLodEnabled(true);
    m_terrain->SetLodColorMode(m_colorMode);
    m_terrain->SetLodFrozen(m_frozen);
    m_terrain->SetLodFrustumCullingEnabled(m_cullingEnabled);
    m_terrain->SetLodDebugBoxesEnabled(m_debugBoxesEnabled);

    // 이웃 레벨 제한은 스티칭의 전제라 항상 켜둔다.
    m_terrain->SetLodNeighborClampEnabled(true);

    m_terrain->SetLodStitchEnabled(m_stitchEnabled);
    m_terrain->SetLodMorphEnabled(m_morphEnabled);
    m_terrain->SetLodMorphWidth(m_morphWidth);
    m_terrain->SetLodMorphColorMode(m_morphColorMode);
}

void GeoLodControlComponent::Update(float deltaTime)
{
    if (m_terrain == nullptr)
    {
        return;
    }

    // 조작은 전부 화면 버튼으로 옮겨졌다.

    m_refreshTimer += deltaTime;
    if (m_refreshTimer >= kRefreshInterval)
    {
        m_refreshTimer = 0.0f;
        RefreshInfoText();
    }
}

// ---- 버튼용 동작 (예전 H 키) ----
void GeoLodControlComponent::ToggleStitching()
{
    m_stitchEnabled = !m_stitchEnabled;
    m_terrain->SetLodStitchEnabled(m_stitchEnabled);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 G 키) ----
void GeoLodControlComponent::ToggleMorphing()
{
    m_morphEnabled = !m_morphEnabled;
    m_terrain->SetLodMorphEnabled(m_morphEnabled);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 V 키) ----
void GeoLodControlComponent::ToggleMorphColorMode()
{
    m_morphColorMode = !m_morphColorMode;
    m_terrain->SetLodMorphColorMode(m_morphColorMode);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 O 키) ----
void GeoLodControlComponent::DecreaseMorphWidth()
{
    m_morphWidth = std::max(m_morphWidth - kMorphWidthStep, 0.0f);
    m_terrain->SetLodMorphWidth(m_morphWidth);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 P 키) ----
void GeoLodControlComponent::IncreaseMorphWidth()
{
    m_morphWidth = std::min(m_morphWidth + kMorphWidthStep, 0.5f);
    m_terrain->SetLodMorphWidth(m_morphWidth);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 K 키) ----
void GeoLodControlComponent::ToggleColorMode()
{
    m_colorMode = !m_colorMode;
    m_terrain->SetLodColorMode(m_colorMode);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 F 키) ----
void GeoLodControlComponent::ToggleFrozen()
{
    m_frozen = !m_frozen;
    m_terrain->SetLodFrozen(m_frozen);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 C 키) ----
void GeoLodControlComponent::ToggleCulling()
{
    m_cullingEnabled = !m_cullingEnabled;
    m_terrain->SetLodFrustumCullingEnabled(m_cullingEnabled);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 B 키) ----
void GeoLodControlComponent::ToggleDebugBoxes()
{
    m_debugBoxesEnabled = !m_debugBoxesEnabled;
    m_terrain->SetLodDebugBoxesEnabled(m_debugBoxesEnabled);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 ',' 키) ----
void GeoLodControlComponent::DecreaseChunkSize()
{
    if (m_chunkSizeIndex > 0)
    {
        --m_chunkSizeIndex;
        m_terrain->SetLodChunkSize(kChunkSizeSteps[m_chunkSizeIndex]);
        RefreshInfoText();
    }
}

// ---- 버튼용 동작 (예전 '.' 키) ----
void GeoLodControlComponent::IncreaseChunkSize()
{
    if (m_chunkSizeIndex < kChunkSizeStepCount - 1)
    {
        ++m_chunkSizeIndex;
        m_terrain->SetLodChunkSize(kChunkSizeSteps[m_chunkSizeIndex]);
        RefreshInfoText();
    }
}

// ---- 버튼용 동작 (예전 ';' 키. 버튼도 SetRepeatWhileHeld(true) 로 등록한다) ----
void GeoLodControlComponent::DecreaseBaseDistance()
{
    m_baseDistance = std::max(m_baseDistance / kBaseDistanceFactor, kMinBaseDistance);
    m_terrain->SetLodBaseDistance(m_baseDistance);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 '\'' 키) ----
void GeoLodControlComponent::IncreaseBaseDistance()
{
    m_baseDistance = std::min(m_baseDistance * kBaseDistanceFactor, kMaxBaseDistance);
    m_terrain->SetLodBaseDistance(m_baseDistance);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 U 키) ----
void GeoLodControlComponent::DecreaseLevelCount()
{
    if (m_levelCount > kMinLevelCount)
    {
        --m_levelCount;
        m_terrain->SetLodLevelCount(m_levelCount);
        RefreshInfoText();
    }
}

// ---- 버튼용 동작 (예전 I 키) ----
void GeoLodControlComponent::IncreaseLevelCount()
{
    if (m_levelCount < kMaxLevelCount)
    {
        ++m_levelCount;
        m_terrain->SetLodLevelCount(m_levelCount);
        RefreshInfoText();
    }
}

// ---- 버튼용 동작 (예전 0 키) ----
void GeoLodControlComponent::ResetToDefault()
{
    m_chunkSizeIndex = kDefaultChunkSizeIndex;
    m_levelCount = kDefaultLevelCount;
    m_baseDistance = kDefaultBaseDistance;
    m_morphWidth = kDefaultMorphWidth;
    m_stitchEnabled = true;
    m_morphEnabled = true;
    m_morphColorMode = false;
    m_colorMode = false;
    m_frozen = false;
    m_cullingEnabled = true;
    m_debugBoxesEnabled = false;
    ApplyAll();
    RefreshInfoText();
}

std::wstring GeoLodControlComponent::FormatThousands(size_t value)
{
    std::wstring digits = std::to_wstring(value);
    std::wstring out;
    out.reserve(digits.size() + digits.size() / 3);

    const size_t lead = digits.size() % 3;
    for (size_t i = 0; i < digits.size(); ++i)
    {
        if (i > 0 && (i % 3) == lead)
        {
            out.push_back(L',');
        }
        out.push_back(digits[i]);
    }

    return out;
}

void GeoLodControlComponent::RefreshInfoText()
{
    if (m_infoText == nullptr || m_terrain == nullptr)
    {
        return;
    }

    const int chunkSize = kChunkSizeSteps[m_chunkSizeIndex];
    const int actualLevels = m_terrain->GetLodActualLevelCount();

    const size_t totalChunks = m_terrain->GetLodChunkCount();
    const size_t drawnChunks = m_terrain->GetLodDrawnChunkCount();
    const size_t stitchedChunks = m_terrain->GetLodStitchedChunkCount();

    const size_t drawnTriangles = m_terrain->GetLodDrawnTriangleCount();
    const size_t fullTriangles = m_terrain->GetTriangleCount();

    // 매 프레임 새로 만드는 테두리 인덱스 양 -- "이게 얼마나 싼 일인지" 를 보여준다.
    const size_t stitchIndices = m_terrain->GetLodStitchIndexCount();

    // 기준 거리가 청크 크기에 비해 너무 가까우면 지오머핑이 팝핑을 다 흡수하지 못한다.
    const float recommendedBase = m_terrain->GetLodRecommendedBaseDistance();

    wchar_t levelLine[128] = {};
    int levelLineLength = 0;
    for (int level = 0; level < actualLevels && level < 5; ++level)
    {
        const int count = m_terrain->GetLodChunksAtLevel(level);
        const int written = std::swprintf(levelLine + levelLineLength,
                                          static_cast<size_t>(128 - levelLineLength),
                                          (level == 0) ? L"L%d %d" : L" / L%d %d",
                                          level, count);
        if (written <= 0)
        {
            break;
        }
        levelLineLength += written;
    }

    wchar_t buffer[1100] = {};

    std::swprintf(buffer, 1100,
        L"[고급 거리 LOD]\n"
        L"스티칭 %s   지오머핑 %s   morph 폭 %.2f   morph 시각화 %s\n"
        L"청크 : %d x %d 셀   %zu 개 중 %zu 개 그림   그중 테두리를 다시 엮은 청크 %zu 개\n"
        L"매 프레임 새로 만든 테두리 인덱스 : %s 개\n"
        L"기준 거리 : %.0f (권장 최소 %.0f)%s\n"
        L"레벨 수 : %d   레벨별 청크 : %s\n"
        L"삼각형 : %s 개 그림  /  풀 해상도 %s 개  (%.1f%%)\n"
        L"레벨 색상 %s   프리즈 %s   컬링 %s   박스 %s   (이웃 레벨 차이 1 제한은 항상 켬)",
        m_stitchEnabled ? L"켬" : L"끔 (이음매가 보인다)",
        m_morphEnabled ? L"켬" : L"끔 (레벨이 바뀔 때 튄다)",
        m_morphWidth,
        m_morphColorMode ? L"켬" : L"끔",
        chunkSize, chunkSize,
        totalChunks, drawnChunks, stitchedChunks,
        FormatThousands(stitchIndices).c_str(),
        m_baseDistance,
        recommendedBase,
        (m_baseDistance < recommendedBase) ? L"   <- 너무 가까움 : 팝핑이 남는다" : L"",
        actualLevels,
        levelLine,
        FormatThousands(drawnTriangles).c_str(),
        FormatThousands(fullTriangles).c_str(),
        (fullTriangles > 0) ? (100.0 * static_cast<double>(drawnTriangles) / static_cast<double>(fullTriangles)) : 0.0,
        m_colorMode ? L"켬" : L"끔",
        m_frozen ? L"켬" : L"끔",
        m_cullingEnabled ? L"켬" : L"끔",
        m_debugBoxesEnabled ? L"켬" : L"끔");

    m_infoText->SetText(buffer);
}
