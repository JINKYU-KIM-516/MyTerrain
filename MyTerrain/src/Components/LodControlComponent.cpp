#include "LodControlComponent.h"
#include "../Framework/InputManager.h"
#include "../Terrain/TerrainRenderer.h"
#include "../UI/UIText.h"

#include <algorithm>
#include <cwchar>

void LodControlComponent::Start()
{
    ApplyAll();
    RefreshInfoText();
}

void LodControlComponent::ApplyAll()
{
    if (m_terrain == nullptr)
    {
        return;
    }

    m_chunkSizeIndex = std::clamp(m_chunkSizeIndex, 0, kChunkSizeStepCount - 1);
    m_levelCount = std::clamp(m_levelCount, kMinLevelCount, kMaxLevelCount);
    m_baseDistance = std::clamp(m_baseDistance, kMinBaseDistance, kMaxBaseDistance);

    m_terrain->SetLodChunkSize(kChunkSizeSteps[m_chunkSizeIndex]);
    m_terrain->SetLodLevelCount(m_levelCount);
    m_terrain->SetLodBaseDistance(m_baseDistance);
    m_terrain->SetLodEnabled(m_lodEnabled);
    m_terrain->SetLodColorMode(m_colorMode);
    m_terrain->SetLodFrozen(m_frozen);
    m_terrain->SetLodNeighborClampEnabled(m_neighborClamp);
    m_terrain->SetLodFrustumCullingEnabled(m_cullingEnabled);
    m_terrain->SetLodDebugBoxesEnabled(m_debugBoxesEnabled);
}

void LodControlComponent::Update(float deltaTime)
{
    if (m_terrain == nullptr)
    {
        return;
    }

    InputManager& input = InputManager::GetInstance();
    bool changed = false;

    // ---------------- 켬/끔 토글 ----------------
    if (input.IsKeyPressed('L'))
    {
        m_lodEnabled = !m_lodEnabled;
        m_terrain->SetLodEnabled(m_lodEnabled);
        changed = true;
    }

    if (input.IsKeyPressed('K'))
    {
        m_colorMode = !m_colorMode;
        m_terrain->SetLodColorMode(m_colorMode);
        changed = true;
    }

    if (input.IsKeyPressed('F'))
    {
        // 켜는 순간의 카메라 위치를 렌더러가 붙잡는다 (다음 프레임 선택부터 고정된다).
        m_frozen = !m_frozen;
        m_terrain->SetLodFrozen(m_frozen);
        changed = true;
    }

    if (input.IsKeyPressed('J'))
    {
        m_neighborClamp = !m_neighborClamp;
        m_terrain->SetLodNeighborClampEnabled(m_neighborClamp);
        changed = true;
    }

    if (input.IsKeyPressed('C'))
    {
        m_cullingEnabled = !m_cullingEnabled;
        m_terrain->SetLodFrustumCullingEnabled(m_cullingEnabled);
        changed = true;
    }

    if (input.IsKeyPressed('B'))
    {
        m_debugBoxesEnabled = !m_debugBoxesEnabled;
        m_terrain->SetLodDebugBoxesEnabled(m_debugBoxesEnabled);
        changed = true;
    }

    // ---------------- 청크 크기 (, / .) ----------------
    if (input.IsKeyPressed(VK_OEM_COMMA) && m_chunkSizeIndex > 0)
    {
        --m_chunkSizeIndex;
        m_terrain->SetLodChunkSize(kChunkSizeSteps[m_chunkSizeIndex]);
        changed = true;
    }
    if (input.IsKeyPressed(VK_OEM_PERIOD) && m_chunkSizeIndex < kChunkSizeStepCount - 1)
    {
        ++m_chunkSizeIndex;
        m_terrain->SetLodChunkSize(kChunkSizeSteps[m_chunkSizeIndex]);
        changed = true;
    }

    // ---------------- 기준 거리 (; / ') ----------------
    // 거리는 인덱스를 다시 만들지 않으므로 누르고 있는 동안 계속 반응해도 부담이 없다.
    if (input.IsKeyDown(VK_OEM_1))          // ';'
    {
        m_baseDistance = std::max(m_baseDistance / kBaseDistanceFactor, kMinBaseDistance);
        m_terrain->SetLodBaseDistance(m_baseDistance);
        changed = true;
    }
    if (input.IsKeyDown(VK_OEM_7))          // '\''
    {
        m_baseDistance = std::min(m_baseDistance * kBaseDistanceFactor, kMaxBaseDistance);
        m_terrain->SetLodBaseDistance(m_baseDistance);
        changed = true;
    }

    // ---------------- 레벨 수 (U / I) ----------------
    if (input.IsKeyPressed('U') && m_levelCount > kMinLevelCount)
    {
        --m_levelCount;
        m_terrain->SetLodLevelCount(m_levelCount);
        changed = true;
    }
    if (input.IsKeyPressed('I') && m_levelCount < kMaxLevelCount)
    {
        ++m_levelCount;
        m_terrain->SetLodLevelCount(m_levelCount);
        changed = true;
    }

    // ---------------- 기본값 복귀 ----------------
    if (input.IsKeyPressed('0') || input.IsKeyPressed(VK_NUMPAD0))
    {
        m_chunkSizeIndex = kDefaultChunkSizeIndex;
        m_levelCount = kDefaultLevelCount;
        m_baseDistance = kDefaultBaseDistance;
        m_lodEnabled = true;
        m_colorMode = true;
        m_frozen = false;
        m_neighborClamp = false;
        m_cullingEnabled = true;
        m_debugBoxesEnabled = false;
        ApplyAll();
        changed = true;
    }

    m_refreshTimer += deltaTime;
    if (changed || m_refreshTimer >= kRefreshInterval)
    {
        m_refreshTimer = 0.0f;
        RefreshInfoText();
    }
}

std::wstring LodControlComponent::FormatThousands(size_t value)
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

void LodControlComponent::RefreshInfoText()
{
    if (m_infoText == nullptr || m_terrain == nullptr)
    {
        return;
    }

    const int chunkSize = kChunkSizeSteps[m_chunkSizeIndex];
    const int actualLevels = m_terrain->GetLodActualLevelCount();

    const size_t totalChunks = m_terrain->GetLodChunkCount();
    const size_t drawnChunks = m_terrain->GetLodDrawnChunkCount();

    const size_t drawnTriangles = m_terrain->GetLodDrawnTriangleCount();
    const size_t fullTriangles = m_terrain->GetTriangleCount();

    // 레벨별 청크 수 (이번 프레임에 그린 것 기준)
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

    // 레벨 경계 거리 (기준 거리부터 두 배씩). 레벨이 N 개면 경계는 N-1 개다.
    wchar_t boundaryLine[128] = {};
    int boundaryLineLength = 0;
    {
        float threshold = m_baseDistance;
        for (int level = 1; level < actualLevels && level < 5; ++level)
        {
            const int written = std::swprintf(boundaryLine + boundaryLineLength,
                                              static_cast<size_t>(128 - boundaryLineLength),
                                              (level == 1) ? L"%.0f" : L" / %.0f",
                                              threshold);
            if (written <= 0)
            {
                break;
            }
            boundaryLineLength += written;
            threshold *= 2.0f;
        }

        if (boundaryLineLength == 0)
        {
            std::swprintf(boundaryLine, 128, L"없음 (레벨 1개)");
        }
    }

    // 인덱스 버퍼가 원본 대비 몇 배로 늘었는지 (모든 레벨을 미리 구워둔 대가)
    const size_t lodIndices = m_terrain->GetLodIndexCount();
    const size_t baseIndices = m_terrain->GetLodBaseIndexCount();
    const double indexRatio = (baseIndices > 0)
        ? static_cast<double>(lodIndices) / static_cast<double>(baseIndices)
        : 0.0;

    wchar_t buffer[900] = {};

    std::swprintf(buffer, 900,
        L"[거리 LOD]  LOD L   색상 K   프리즈 F   이웃제한 J\n"
        L"           컬링 C   박스 B   청크 , / .   거리 ; / '   레벨수 U / I   기본값 0\n"
        L"청크 : %d x %d 셀   %zu 개 중 %zu 개 그림   레벨 수 : %d\n"
        L"기준 거리 : %.0f   레벨 경계 : %s\n"
        L"레벨별 청크 : %s\n"
        L"삼각형 : %s 개 그림  /  풀 해상도 %s 개  (%.1f%%)\n"
        L"인덱스 버퍼 : 원본의 %.2f 배 (모든 레벨을 미리 구워둔 값)\n"
        L"LOD %s   색상 %s   프리즈 %s   이웃제한 %s   컬링 %s   박스 %s",
        chunkSize, chunkSize,
        totalChunks, drawnChunks,
        actualLevels,
        m_baseDistance, boundaryLine,
        levelLine,
        FormatThousands(drawnTriangles).c_str(),
        FormatThousands(fullTriangles).c_str(),
        (fullTriangles > 0) ? (100.0 * static_cast<double>(drawnTriangles) / static_cast<double>(fullTriangles)) : 0.0,
        indexRatio,
        m_lodEnabled ? L"켬" : L"끔(전부 레벨 0)",
        m_colorMode ? L"켬" : L"끔",
        m_frozen ? L"켬" : L"끔",
        m_neighborClamp ? L"켬" : L"끔",
        m_cullingEnabled ? L"켬" : L"끔",
        m_debugBoxesEnabled ? L"켬" : L"끔");

    m_infoText->SetText(buffer);
}
