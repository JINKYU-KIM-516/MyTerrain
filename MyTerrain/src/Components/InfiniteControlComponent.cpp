#include "InfiniteControlComponent.h"
#include "../Terrain/InfiniteTerrainRenderer.h"
#include "../Framework/Camera.h"
#include "../Framework/InputManager.h"
#include "../GameObject/GameObject.h"
#include "../GameObject/Transform.h"
#include "../UI/UIText.h"

#include <algorithm>
#include <chrono>
#include <cwchar>

void InfiniteControlComponent::SetParams(const Noise::Params& params)
{
    m_params = params;
    m_defaultParams = params;
    m_perlin.Reseed(m_params.seed);
}

GridMesh::HeightFunc InfiniteControlComponent::MakeHeightFunction()
{
    // 여기 들어오는 (x, z) 는 이미 월드 좌표다 (GridMesh::GenerateChunk 가 청크의
    // 월드 오프셋을 더해서 부른다). 그래서 이 함수는 청크를 전혀 모르고,
    // 그 덕분에 이웃 청크의 맞닿은 정점이 저절로 같은 높이를 얻는다.
    return [this](float worldX, float worldZ) -> float
    {
        return m_perlin.Evaluate(worldX, worldZ, m_params);
    };
}

void InfiniteControlComponent::Start()
{
    if (m_terrain == nullptr)
    {
        return;
    }

    ApplyResolution();

    m_terrain->SetKeepRadius(kDefaultRadius);
    m_terrain->SetMaxBuildsPerFrame(kDefaultBudget);

    // 높이 함수를 넣는 순간부터 청크가 만들어지기 시작한다
    m_terrain->SetHeightFunction(MakeHeightFunction());

    RefreshInfoText();
}

void InfiniteControlComponent::ApplyResolution()
{
    if (m_terrain == nullptr)
    {
        return;
    }

    m_divisions = std::clamp(m_divisions, kMinDivisions, kMaxDivisions);

    // 청크 한 변의 월드 크기는 고정. 분할 수가 늘면 셀이 그만큼 잘아진다.
    m_terrain->SetChunkResolution(m_divisions, kChunkWorldSize / static_cast<float>(m_divisions));
}

void InfiniteControlComponent::Update(float deltaTime)
{
    if (m_terrain == nullptr)
    {
        return;
    }

    InputManager& input = InputManager::GetInstance();

    if (input.IsKeyPressed(VK_TAB))
    {
        CycleDisplayMode();
    }

    // HUD 는 매 프레임 다시 만들 필요가 없다. 숫자가 빠르게 깜빡이면 오히려 읽기 어렵다.
    m_refreshTimer += deltaTime;
    if (m_refreshTimer >= kRefreshInterval)
    {
        m_refreshTimer = 0.0f;
        RefreshInfoText();
    }
}

//=====================================================================
// 버튼 동작
//=====================================================================

void InfiniteControlComponent::IncreaseRadius()
{
    if (m_terrain != nullptr)
    {
        m_terrain->SetKeepRadius(m_terrain->GetKeepRadius() + 1);
        RefreshInfoText();
    }
}

void InfiniteControlComponent::DecreaseRadius()
{
    if (m_terrain != nullptr)
    {
        m_terrain->SetKeepRadius(m_terrain->GetKeepRadius() - 1);
        RefreshInfoText();
    }
}

void InfiniteControlComponent::IncreaseDivisions()
{
    if (m_divisions >= kMaxDivisions)
    {
        return;
    }

    m_divisions *= 2;
    ApplyResolution();
    RefreshInfoText();
}

void InfiniteControlComponent::DecreaseDivisions()
{
    if (m_divisions <= kMinDivisions)
    {
        return;
    }

    m_divisions /= 2;
    ApplyResolution();
    RefreshInfoText();
}

void InfiniteControlComponent::IncreaseBudget()
{
    if (m_terrain != nullptr)
    {
        m_terrain->SetMaxBuildsPerFrame(m_terrain->GetMaxBuildsPerFrame() + 1);
        RefreshInfoText();
    }
}

void InfiniteControlComponent::DecreaseBudget()
{
    if (m_terrain != nullptr)
    {
        m_terrain->SetMaxBuildsPerFrame(m_terrain->GetMaxBuildsPerFrame() - 1);
        RefreshInfoText();
    }
}

void InfiniteControlComponent::ToggleStreaming()
{
    if (m_terrain != nullptr)
    {
        m_terrain->SetStreamingPaused(!m_terrain->IsStreamingPaused());
        RefreshInfoText();
    }
}

void InfiniteControlComponent::ToggleFrustumCulling()
{
    if (m_terrain != nullptr)
    {
        m_terrain->SetFrustumCullingEnabled(!m_terrain->IsFrustumCullingEnabled());
        RefreshInfoText();
    }
}

void InfiniteControlComponent::ToggleChunkColor()
{
    if (m_terrain != nullptr)
    {
        m_terrain->SetChunkColorMode(!m_terrain->IsChunkColorMode());
        RefreshInfoText();
    }
}

void InfiniteControlComponent::ToggleHighlight()
{
    if (m_terrain != nullptr)
    {
        m_terrain->SetHighlightNewChunks(!m_terrain->IsHighlightNewChunks());
        RefreshInfoText();
    }
}

void InfiniteControlComponent::ToggleFog()
{
    if (m_terrain != nullptr)
    {
        m_terrain->SetFogEnabled(!m_terrain->IsFogEnabled());
        RefreshInfoText();
    }
}

void InfiniteControlComponent::CycleDisplayMode()
{
    if (m_terrain != nullptr)
    {
        m_terrain->CycleDisplayMode();
        RefreshInfoText();
    }
}

void InfiniteControlComponent::JumpFar()
{
    // 카메라를 유지 반경 밖으로 한 번에 옮긴다. 그 순간 올라와 있던 청크가 전부
    // 해제 반경 밖으로 나가고, 새 위치의 청크를 예산만큼씩 다시 만들게 된다 --
    // "프레임당 생성 제한" 이 무엇을 막고 있는지가 가장 잘 보이는 조작이다.
    Camera* camera = Camera::GetMain();
    if (camera == nullptr || camera->GetGameObject() == nullptr)
    {
        return;
    }

    Transform* transform = camera->GetGameObject()->GetTransform();
    if (transform == nullptr)
    {
        return;
    }

    const DirectX::XMFLOAT3 position = transform->GetPosition();
    const float jump = kChunkWorldSize * 8.0f;

    transform->SetPosition(position.x + jump, position.y, position.z + jump);
    RefreshInfoText();
}

void InfiniteControlComponent::RandomizeSeed()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

    m_params.seed = static_cast<unsigned int>(ticks) * 2654435761u + 1u;
    m_perlin.Reseed(m_params.seed);

    // 지형의 정의가 통째로 바뀌었으므로 구워둔 청크는 전부 무효다.
    if (m_terrain != nullptr)
    {
        m_terrain->RebuildAll();
    }

    RefreshInfoText();
}

void InfiniteControlComponent::ResetToDefault()
{
    m_params = m_defaultParams;
    m_perlin.Reseed(m_params.seed);

    m_divisions = kDefaultDivisions;
    ApplyResolution();

    if (m_terrain != nullptr)
    {
        m_terrain->SetKeepRadius(kDefaultRadius);
        m_terrain->SetMaxBuildsPerFrame(kDefaultBudget);
        m_terrain->SetStreamingPaused(false);
        m_terrain->SetFrustumCullingEnabled(true);
        m_terrain->SetChunkColorMode(true);
        m_terrain->SetHighlightNewChunks(true);
        m_terrain->SetFogEnabled(true);
        m_terrain->SetDisplayMode(ChunkDisplayMode::Solid);
        m_terrain->RebuildAll();
    }

    RefreshInfoText();
}

//=====================================================================
// HUD
//=====================================================================

std::wstring InfiniteControlComponent::FormatThousands(size_t value)
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

void InfiniteControlComponent::RefreshInfoText()
{
    if (m_infoText == nullptr || m_terrain == nullptr)
    {
        return;
    }

    const ChunkGrid::Coord center = m_terrain->GetCenterCoord();

    DirectX::XMFLOAT3 cameraPosition{ 0.0f, 0.0f, 0.0f };
    if (Camera* camera = Camera::GetMain())
    {
        cameraPosition = camera->GetWorldPosition();
    }

    const int radius = m_terrain->GetKeepRadius();
    const int side = 2 * radius + 1;

    wchar_t buffer[1200] = {};

    std::swprintf(buffer, 1200,
        L"[무한 지형 청크]\n"
        L"중심 청크 (%d, %d)   카메라 (%.0f, %.0f)\n"
        L"청크 : 한 변 %.0f 월드단위 · %d 분할 (셀 %.2f)   청크당 %s 삼각형\n"
        L"유지 반경 %d -> %d x %d = %s 청크   해제 반경 %d (히스테리시스)\n"
        L"로드됨 %s / %s   대기 %s   버퍼 풀 %s\n"
        L"생성 : 이번 프레임 %d / 예산 %d   누적 생성 %s · 해제 %s\n"
        L"생성 시간 : 마지막 %.2f ms · 평균 %.2f ms · 이번 프레임 %.2f ms\n"
        L"그리기 : %s 청크 · %s 삼각형\n"
        L"스트리밍 %s   컬링 %s   안개 %s (%.0f ~ %.0f)   표시 %s\n"
        L"청크 색상 %s   신규 강조 %s   시드 %u",
        center.x, center.z,
        cameraPosition.x, cameraPosition.z,
        m_terrain->GetChunkWorldSize(), m_terrain->GetDivisions(), m_terrain->GetCellSize(),
        FormatThousands(m_terrain->GetTrianglesPerChunk()).c_str(),
        radius, side, side,
        FormatThousands(m_terrain->GetDesiredChunkCount()).c_str(),
        m_terrain->GetReleaseRadius(),
        FormatThousands(m_terrain->GetLoadedChunkCount()).c_str(),
        FormatThousands(m_terrain->GetDesiredChunkCount()).c_str(),
        FormatThousands(m_terrain->GetQueuedChunkCount()).c_str(),
        FormatThousands(m_terrain->GetPooledBufferCount()).c_str(),
        m_terrain->GetBuiltLastFrame(), m_terrain->GetMaxBuildsPerFrame(),
        FormatThousands(m_terrain->GetTotalBuiltCount()).c_str(),
        FormatThousands(m_terrain->GetTotalReleasedCount()).c_str(),
        m_terrain->GetLastBuildMilliseconds(),
        m_terrain->GetAverageBuildMilliseconds(),
        m_terrain->GetLastFrameBuildMilliseconds(),
        FormatThousands(m_terrain->GetDrawnChunkCount()).c_str(),
        FormatThousands(m_terrain->GetDrawnTriangleCount()).c_str(),
        m_terrain->IsStreamingPaused() ? L"일시정지" : L"켬",
        m_terrain->IsFrustumCullingEnabled() ? L"켬" : L"끔",
        m_terrain->IsFogEnabled() ? L"켬" : L"끔",
        m_terrain->GetFogStart(), m_terrain->GetFogEnd(),
        ToDisplayName(m_terrain->GetDisplayMode()),
        m_terrain->IsChunkColorMode() ? L"켬" : L"끔",
        m_terrain->IsHighlightNewChunks() ? L"켬" : L"끔",
        m_params.seed);

    m_infoText->SetText(buffer);
}
