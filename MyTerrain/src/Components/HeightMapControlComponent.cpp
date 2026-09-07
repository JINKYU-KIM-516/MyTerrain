#include "HeightMapControlComponent.h"
#include "../Framework/Framework.h"
#include "../Framework/InputManager.h"
#include "../Terrain/TerrainRenderer.h"
#include "../UI/UIText.h"

#include <algorithm>
#include <cmath>

namespace
{
    // 더하기/빼기를 반복하면 부동소수 오차가 쌓이므로 눈금에 맞춰 정리한다
    float SnapTo(float value, float step)
    {
        return std::round(value / step) * step;
    }

    // 화면에서 "지금 고른 항목" 표시
    constexpr wchar_t kSelectedMark[] = L"▶ ";
    constexpr wchar_t kNormalMark[] = L"   ";
}

void HeightMapControlComponent::SetParams(const HeightMap::Params& params)
{
    m_params = params;
    m_defaultParams = params;
}

GridMesh::HeightFunc HeightMapControlComponent::MakeHeightFunction()
{
    // GridMesh::Generate 가 정점마다 이 함수를 부른다.
    // 컴포넌트와 TerrainRenderer 는 같은 씬에서 함께 죽으므로 this 캡처로 충분하다.
    return [this](float x, float z) { return SampleHeight(x, z); };
}

void HeightMapControlComponent::Start()
{
    RescanFiles();

    if (!m_files.empty())
    {
        LoadFileAt(0);
    }
    else
    {
        m_loadError = L"heightmaps 폴더를 찾지 못했거나 비어 있습니다.";
    }

    if (m_terrain != nullptr)
    {
        m_terrain->SetHeightFunction(MakeHeightFunction());
        m_terrain->SetHeightMapMapping(m_params.worldSize, m_params.flipZ);
    }

    // 부모의 Start 가 HUD 를 한 번 갱신해준다 (가상 함수라 아래 RefreshInfoText 가 불린다)
    GridControlComponent::Start();
}

void HeightMapControlComponent::Destroy()
{
    if (m_terrain != nullptr)
    {
        m_terrain->SetHeightMapResources(nullptr, nullptr);
    }

    m_texture.Reset();
}

float HeightMapControlComponent::SampleHeight(float worldX, float worldZ)
{
    const float height = HeightMap::Evaluate(m_image, worldX, worldZ, m_params);

    if (!m_sampleValid)
    {
        m_sampleMinHeight = height;
        m_sampleMaxHeight = height;
        m_sampleValid = true;
    }
    else
    {
        m_sampleMinHeight = std::min(m_sampleMinHeight, height);
        m_sampleMaxHeight = std::max(m_sampleMaxHeight, height);
    }

    return height;
}

// ---------------- 파일 ----------------

void HeightMapControlComponent::RescanFiles()
{
    const std::wstring previous = (m_fileIndex >= 0 && m_fileIndex < static_cast<int>(m_files.size()))
        ? m_files[m_fileIndex]
        : std::wstring();

    m_files = HeightMap::ListFiles();

    // 다시 훑기 전에 보고 있던 파일이 그대로 있으면 그 자리를 유지한다
    m_fileIndex = -1;
    if (!previous.empty())
    {
        for (size_t i = 0; i < m_files.size(); ++i)
        {
            if (m_files[i] == previous)
            {
                m_fileIndex = static_cast<int>(i);
                break;
            }
        }
    }
}

void HeightMapControlComponent::LoadFileAt(int index)
{
    if (m_files.empty())
    {
        m_image.Clear();
        m_fileIndex = -1;
        m_loadError = L"heightmaps 폴더에 읽을 수 있는 파일이 없습니다.";
        return;
    }

    const int count = static_cast<int>(m_files.size());
    m_fileIndex = ((index % count) + count) % count;

    std::wstring error;
    if (m_image.LoadFromFile(m_files[m_fileIndex], &error))
    {
        m_loadError.clear();
        m_texturePending = true;
    }
    else
    {
        m_loadError = error.empty() ? L"높이맵을 읽지 못했습니다." : error;
        m_texture.Reset();
        m_texturePending = false;

        if (m_terrain != nullptr)
        {
            m_terrain->SetHeightMapResources(nullptr, nullptr);
        }
    }

    if (m_terrain != nullptr)
    {
        m_terrain->RequestRebuild();
    }

    m_sampleValid = false;
    m_lastValid = false;
}

void HeightMapControlComponent::CycleFile(int direction)
{
    if (m_files.empty())
    {
        return;
    }

    LoadFileAt((m_fileIndex < 0) ? 0 : (m_fileIndex + direction));
}

void HeightMapControlComponent::UploadTexture()
{
    if (!m_texturePending || !m_image.IsValid())
    {
        return;
    }

    if (!m_texture.Upload(m_image))
    {
        return;   // 디바이스가 아직 준비되지 않았다. 다음 프레임에 다시 시도한다.
    }

    m_texture.SetWrapMode(m_params.wrap);
    m_texturePending = false;

    if (m_terrain != nullptr)
    {
        m_terrain->SetHeightMapResources(m_texture.GetSRV(), m_texture.GetSampler());
    }
}

// ---------------- 파라미터 ----------------

void HeightMapControlComponent::AdjustSelected(int direction)
{
    const float sign = static_cast<float>(direction);

    switch (m_selected)
    {
    case ParamId::File:
        CycleFile(direction);
        break;

    case ParamId::HeightScale:
        // 범위가 넓으므로 곱셈으로 움직인다 (2번의 진폭 조절과 같은 방식)
        m_params.heightScale = std::clamp(
            m_params.heightScale * ((direction > 0) ? 1.12f : (1.0f / 1.12f)), 0.5f, 2000.0f);
        break;

    case ParamId::HeightOffset:
        m_params.heightOffset = std::clamp(SnapTo(m_params.heightOffset + sign * 2.0f, 2.0f), -1000.0f, 1000.0f);
        break;

    case ParamId::WorldSize:
        m_params.worldSize = std::clamp(
            m_params.worldSize * ((direction > 0) ? 1.15f : (1.0f / 1.15f)), 8.0f, 8192.0f);
        break;

    case ParamId::Wrap:
    {
        const int count = static_cast<int>(HeightMap::WrapMode::Count);
        const int next = (static_cast<int>(m_params.wrap) + count + direction) % count;
        m_params.wrap = static_cast<HeightMap::WrapMode>(next);
        break;
    }

    case ParamId::FlipZ:
        m_params.flipZ = !m_params.flipZ;
        break;

    default:
        break;
    }
}

void HeightMapControlComponent::ApplyParamChange()
{
    m_texture.SetWrapMode(m_params.wrap);

    if (m_terrain != nullptr)
    {
        m_terrain->SetHeightMapMapping(m_params.worldSize, m_params.flipZ);
        m_terrain->RequestRebuild();
    }
}

void HeightMapControlComponent::Update(float deltaTime)
{
    // 분할 수(+/-), 셀 크기([ / ]), 표시 모드(Tab) 는 부모가 처리한다
    GridControlComponent::Update(deltaTime);

    if (m_terrain == nullptr)
    {
        return;
    }

    // 디바이스가 늦게 준비되는 경우가 있어 텍스처 업로드는 여기서 마무리한다
    UploadTexture();

    // 직전 프레임의 Render 에서 생성이 끝났다면 그 결과를 표시용으로 확정한다
    if (m_sampleValid)
    {
        m_lastMinHeight = m_sampleMinHeight;
        m_lastMaxHeight = m_sampleMaxHeight;
        m_lastValid = true;
    }

    InputManager& input = InputManager::GetInstance();

    bool refreshHud = false;
    bool paramChanged = false;

    // ---------------- 항목 선택 (위 / 아래) ----------------
    const int paramCount = static_cast<int>(ParamId::Count);

    if (input.IsKeyPressed(VK_UP))
    {
        m_selected = static_cast<ParamId>((static_cast<int>(m_selected) + paramCount - 1) % paramCount);
        refreshHud = true;
    }
    if (input.IsKeyPressed(VK_DOWN))
    {
        m_selected = static_cast<ParamId>((static_cast<int>(m_selected) + 1) % paramCount);
        refreshHud = true;
    }

    // ---------------- 값 조절 (왼쪽 / 오른쪽) ----------------
    const int direction = ReadAdjustDirection(deltaTime);
    if (direction != 0)
    {
        AdjustSelected(direction);
        paramChanged = true;
    }

    // ---------------- 단축키 ----------------
    if (input.IsKeyPressed('N'))
    {
        CycleFile(1);
        m_selected = ParamId::File;
        paramChanged = true;
    }

    if (input.IsKeyPressed('C'))
    {
        m_terrain->SetHeightColorMode(!m_terrain->IsHeightColorMode());
        refreshHud = true;
    }

    // F5 : 폴더를 다시 훑고 현재 파일을 다시 읽는다.
    // 이미지를 고쳐 저장한 뒤 프로그램을 다시 켜지 않고 확인하기 위한 것이다.
    if (input.IsKeyPressed(VK_F5))
    {
        RescanFiles();
        LoadFileAt((m_fileIndex < 0) ? 0 : m_fileIndex);
        paramChanged = true;
    }

    if (input.IsKeyPressed('0') || input.IsKeyPressed(VK_NUMPAD0))
    {
        // 파일 선택은 그대로 두고 펼침 파라미터만 되돌린다
        m_params = m_defaultParams;
        paramChanged = true;
    }

    if (paramChanged)
    {
        ApplyParamChange();
        refreshHud = true;
    }

    // 부모가 분할 수나 셀 크기를 바꿨을 수도 있으므로, 재생성이 예약된 상태면
    // 여기서 통계를 비워둔다. (Update 는 Render 보다 먼저 호출된다)
    if (m_terrain->IsMeshDirty())
    {
        m_sampleValid = false;
    }

    if (refreshHud)
    {
        RefreshInfoText();
    }
}

void HeightMapControlComponent::RefreshInfoText()
{
    if (m_infoText == nullptr || m_terrain == nullptr)
    {
        return;
    }

    auto Row = [this](ParamId id, const wchar_t* label, const std::wstring& value)
    {
        std::wstring line = (m_selected == id) ? kSelectedMark : kNormalMark;
        line += label;
        line += L" : ";
        line += value;
        line += L"\n";
        return line;
    };

    std::wstring text = L"[높이맵]  항목 ↑/↓   값 ←/→\n";

    // ---- 파일 ----
    std::wstring fileLabel;
    if (m_files.empty())
    {
        fileLabel = L"(없음)";
    }
    else
    {
        fileLabel = HeightMap::GetFileNameOnly(m_files[std::max(m_fileIndex, 0)]);
        fileLabel += L"  (" + std::to_wstring(m_fileIndex + 1) + L" / " + std::to_wstring(m_files.size()) + L")";
    }

    text += Row(ParamId::File, L"파일", fileLabel);
    text += Row(ParamId::HeightScale, L"높이 배율", FormatFixed(m_params.heightScale, 1));
    text += Row(ParamId::HeightOffset, L"높이 오프셋", FormatFixed(m_params.heightOffset, 1));
    text += Row(ParamId::WorldSize, L"덮는 범위", FormatFixed(m_params.worldSize, 1));
    text += Row(ParamId::Wrap, L"경계 처리", HeightMap::ToName(m_params.wrap));
    text += Row(ParamId::FlipZ, L"Z 뒤집기", m_params.flipZ ? L"켬" : L"끔");

    // ---- 이미지 정보 ----
    if (!m_loadError.empty())
    {
        text += L"\n" + m_loadError;
    }
    else if (m_image.IsValid())
    {
        text += L"\n이미지 : " + std::to_wstring(m_image.GetWidth()) + L" x " + std::to_wstring(m_image.GetHeight());
        text += L"   " + m_image.GetSourceKind();
        text += L"   계조 " + FormatThousands(static_cast<size_t>(m_image.GetDistinctLevels())) + L"단계";

        // 8비트는 아무리 커도 256단계를 넘지 못한다. 높이 배율이 크면 그대로 계단이 된다.
        if (m_image.GetSourceBits() <= 8)
        {
            const float stepHeight = m_params.heightScale / 255.0f;
            text += L"  <- 8비트, 계단 한 칸 " + FormatFixed(stepHeight, 3);
        }

        text += L"\n값 범위 : " + FormatFixed(m_image.GetMinValue(), 3) + L" ~ " + FormatFixed(m_image.GetMaxValue(), 3);

        // ---- 텍셀 간격 대 셀 크기 ----
        // 높이맵 지형의 결과를 좌우하는 것이 결국 이 둘의 비율이다.
        //   텍셀이 셀보다 잘면  -> 이미지 디테일이 정점 사이에서 잘려나간다
        //   셀이 텍셀보다 잘면  -> 정점만 늘고 사이는 전부 보간이라 디테일이 늘지 않는다
        const float texelSize = m_params.worldSize / static_cast<float>(std::max(m_image.GetWidth(), 1));
        const float cellSize = m_terrain->GetCellSize();
        const float ratio = texelSize / std::max(cellSize, 0.0001f);

        text += L"   텍셀 간격 : " + FormatFixed(texelSize, 3) + L" (셀 " + FormatFixed(cellSize, 3) + L")";

        if (ratio < 1.0f)
        {
            text += L"  <- 이미지가 더 잘아서 디테일이 잘림";
        }
        else if (ratio > 2.0f)
        {
            text += L"  <- 정점만 늘고 사이는 보간뿐";
        }
    }
    else
    {
        text += L"\n이미지 : 없음";
    }

    // ---- 메시 정보 ----
    const int divisionsX = m_terrain->GetDivisionsX();
    const int divisionsZ = m_terrain->GetDivisionsZ();
    const float cellSize = m_terrain->GetCellSize();

    const size_t vertexCount = static_cast<size_t>(divisionsX + 1) * static_cast<size_t>(divisionsZ + 1);
    const size_t triangleCount = static_cast<size_t>(divisionsX) * static_cast<size_t>(divisionsZ) * 2;

    text += L"\n분할 수 : " + std::to_wstring(divisionsX) + L" x " + std::to_wstring(divisionsZ);
    text += L"   전체 크기 : " + FormatFixed(divisionsX * cellSize, 1) + L" x " + FormatFixed(divisionsZ * cellSize, 1);
    text += L"\n정점 : " + FormatThousands(vertexCount);
    text += L"   삼각형 : " + FormatThousands(triangleCount);

    text += L"\n높이 범위 : ";
    if (m_lastValid)
    {
        text += FormatFixed(m_lastMinHeight, 1) + L" ~ " + FormatFixed(m_lastMaxHeight, 1);
    }
    else
    {
        text += L"계산 중";
    }

    text += L"   생성 시간 : " + FormatFixed(static_cast<float>(m_terrain->GetLastRebuildMilliseconds()), 1) + L" ms";

    float fps = 0.0f;
    if (Framework* framework = Framework::GetInstance())
    {
        fps = framework->GetTime().GetFPS();
    }

    text += L"\n표시 모드 : ";
    text += ToDisplayName(m_terrain->GetDisplayMode());
    text += L"   고도 색상(C) : ";
    text += m_terrain->IsHeightColorMode() ? L"켬" : L"끔";
    text += L"   FPS : " + FormatFixed(fps, 1);

    m_infoText->SetText(text);
}
