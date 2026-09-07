#include "PerlinControlComponent.h"
#include "../Framework/Framework.h"
#include "../Framework/InputManager.h"
#include "../Terrain/TerrainRenderer.h"
#include "../UI/UIText.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

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

void PerlinControlComponent::SetParams(const Noise::Params& params)
{
    m_params = params;
    m_defaultParams = params;
}

GridMesh::HeightFunc PerlinControlComponent::MakeHeightFunction()
{
    // GridMesh::Generate 가 정점마다 이 함수를 부른다.
    // 컴포넌트와 TerrainRenderer 는 같은 씬에서 함께 죽으므로 this 캡처로 충분하다.
    return [this](float x, float z) { return SampleHeight(x, z); };
}

void PerlinControlComponent::Start()
{
    m_perlin.Reseed(m_params.seed);
    m_appliedSeed = m_params.seed;

    if (m_terrain != nullptr)
    {
        m_terrain->SetHeightFunction(MakeHeightFunction());
    }

    // 부모의 Start 가 HUD 를 한 번 갱신해준다 (가상 함수라 아래 RefreshInfoText 가 불린다)
    GridControlComponent::Start();
}

float PerlinControlComponent::SampleHeight(float worldX, float worldZ)
{
    const float height = m_perlin.Evaluate(worldX, worldZ, m_params);

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

void PerlinControlComponent::AdjustSelected(int direction)
{
    const float sign = static_cast<float>(direction);

    switch (m_selected)
    {
    case ParamId::Scale:
        // 스케일은 범위가 넓으므로 곱셈으로 움직인다
        m_params.scale = std::clamp(m_params.scale * ((direction > 0) ? 1.15f : (1.0f / 1.15f)), 2.0f, 8192.0f);
        break;

    case ParamId::Octaves:
        m_params.octaves = std::clamp(m_params.octaves + direction, 1, 10);
        break;

    case ParamId::Lacunarity:
        m_params.lacunarity = std::clamp(SnapTo(m_params.lacunarity + sign * 0.05f, 0.05f), 1.10f, 4.00f);
        break;

    case ParamId::Persistence:
        m_params.persistence = std::clamp(SnapTo(m_params.persistence + sign * 0.02f, 0.02f), 0.04f, 0.96f);
        break;

    case ParamId::Amplitude:
        m_params.amplitude = std::clamp(m_params.amplitude * ((direction > 0) ? 1.12f : (1.0f / 1.12f)), 0.5f, 2000.0f);
        break;

    case ParamId::Seed:
        m_params.seed += static_cast<unsigned int>(direction);   // 음수는 자연스럽게 감소로 감싼다
        break;

    case ParamId::Type:
    {
        const int count = static_cast<int>(Noise::Type::Count);
        const int next = (static_cast<int>(m_params.type) + count + direction) % count;
        m_params.type = static_cast<Noise::Type>(next);
        break;
    }

    default:
        break;
    }
}

void PerlinControlComponent::ApplyParamChange()
{
    if (m_params.seed != m_appliedSeed)
    {
        m_perlin.Reseed(m_params.seed);
        m_appliedSeed = m_params.seed;
    }

    if (m_terrain != nullptr)
    {
        m_terrain->RequestRebuild();
    }
}

void PerlinControlComponent::Update(float deltaTime)
{
    // 분할 수(+/-), 셀 크기([ / ]), 표시 모드(Tab) 는 부모가 처리한다
    GridControlComponent::Update(deltaTime);

    if (m_terrain == nullptr)
    {
        return;
    }

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
        const int typeCount = static_cast<int>(Noise::Type::Count);
        m_params.type = static_cast<Noise::Type>((static_cast<int>(m_params.type) + 1) % typeCount);
        m_selected = ParamId::Type;
        paramChanged = true;
    }

    if (input.IsKeyPressed('M'))
    {
        // 실행할 때마다 다른 시드가 나오도록 시각으로 초기화한 난수기를 쓴다
        static std::mt19937 rng(static_cast<unsigned int>(
            std::chrono::steady_clock::now().time_since_epoch().count()));

        m_params.seed = rng();
        m_selected = ParamId::Seed;
        paramChanged = true;
    }

    if (input.IsKeyPressed('0') || input.IsKeyPressed(VK_NUMPAD0))
    {
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

void PerlinControlComponent::RefreshInfoText()
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

    std::wstring text = L"[펄린 노이즈]  항목 ↑/↓   값 ←/→\n";

    text += Row(ParamId::Scale, L"노이즈 스케일", FormatFixed(m_params.scale, 1));
    text += Row(ParamId::Octaves, L"옥타브", std::to_wstring(m_params.octaves));
    text += Row(ParamId::Lacunarity, L"라쿠나리티", FormatFixed(m_params.lacunarity, 2));
    text += Row(ParamId::Persistence, L"퍼시스턴스", FormatFixed(m_params.persistence, 2));
    text += Row(ParamId::Amplitude, L"진폭", FormatFixed(m_params.amplitude, 1));
    text += Row(ParamId::Seed, L"시드", std::to_wstring(m_params.seed));
    text += Row(ParamId::Type, L"합성 방식", Noise::ToName(m_params.type));

    // ---- 가장 잘은 옥타브의 굴곡 크기 ----
    // 이 값이 셀 크기의 2배보다 작아지면 정점 사이에서 잘려나가 앨리어싱만 남는다.
    const float finest = m_params.scale / std::pow(std::max(m_params.lacunarity, 1.0001f),
                                                  static_cast<float>(m_params.octaves - 1));
    const float cellSize = m_terrain->GetCellSize();

    text += L"\n가장 잘은 굴곡 : " + FormatFixed(finest, 2) + L"   (셀 크기 " + FormatFixed(cellSize, 3) + L")";
    if (finest < cellSize * 2.0f)
    {
        text += L"  <- 셀보다 잘아서 낭비";
    }

    // ---- 메시 정보 ----
    const int divisionsX = m_terrain->GetDivisionsX();
    const int divisionsZ = m_terrain->GetDivisionsZ();

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
    text += L"   FPS : " + FormatFixed(fps, 1);

    m_infoText->SetText(text);
}
