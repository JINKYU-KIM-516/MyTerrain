#include "TessellationControlComponent.h"
#include "HeightMapControlComponent.h"
#include "../Terrain/TerrainRenderer.h"
#include "../UI/UIText.h"

#include <algorithm>
#include <cwchar>

void TessellationControlComponent::Start()
{
    if (m_terrain != nullptr)
    {
        m_terrain->SetTessellationEnabled(true);
    }

    ApplyAll();
    SyncHeightMapScale();
    RefreshInfoText();
}

void TessellationControlComponent::ApplyAll()
{
    if (m_terrain == nullptr)
    {
        return;
    }

    m_baseDistance = std::clamp(m_baseDistance, kMinBaseDistance, kMaxBaseDistance);
    m_minFactor = std::clamp(m_minFactor, 1.0f, 64.0f);
    m_maxFactor = std::clamp(std::max(m_maxFactor, m_minFactor), 1.0f, 64.0f);

    m_terrain->SetTessBaseDistance(m_baseDistance);
    m_terrain->SetTessFactorRange(m_minFactor, m_maxFactor);
    m_terrain->SetTessDisplacementEnabled(m_displacementEnabled);
    m_terrain->SetTessFractionalPartitioning(m_fractionalPartitioning);
    m_terrain->SetTessFactorColorMode(m_factorColorMode);
    m_terrain->SetTessFrustumCullingEnabled(m_frustumCullingEnabled);
    m_terrain->SetTessDebugBoxesEnabled(m_debugBoxesEnabled);
    m_terrain->SetTessFrozen(m_frozen);
    m_terrain->SetTessNormalEpsilon(kDefaultNormalEpsilon);
}

void TessellationControlComponent::SyncHeightMapScale()
{
    // heightScale/heightOffset 은 HeightMapControlComponent 가 조절하는 값이다.
    // 도메인 셰이더가 displacement 할 때 0~1 값을 월드 높이로 바꾸려면 이 값이
    // 필요하므로, 매 프레임 그대로 전달한다(비용은 float 두 개를 복사하는 정도다).
    if (m_terrain == nullptr || m_heightMapControl == nullptr)
    {
        return;
    }

    const HeightMap::Params& params = m_heightMapControl->GetParams();
    m_terrain->SetTessHeightMapScale(params.heightScale, params.heightOffset);
}

void TessellationControlComponent::Update(float deltaTime)
{
    if (m_terrain == nullptr)
    {
        return;
    }

    SyncHeightMapScale();

    // 조작은 전부 화면 버튼으로 옮겨졌다.

    m_refreshTimer += deltaTime;
    if (m_refreshTimer >= kRefreshInterval)
    {
        m_refreshTimer = 0.0f;
        RefreshInfoText();
    }
}

// ---- 버튼용 동작 (예전 H 키) ----
void TessellationControlComponent::ToggleDisplacement()
{
    m_displacementEnabled = !m_displacementEnabled;
    m_terrain->SetTessDisplacementEnabled(m_displacementEnabled);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 J 키) ----
void TessellationControlComponent::TogglePartitionMode()
{
    m_fractionalPartitioning = !m_fractionalPartitioning;
    m_terrain->SetTessFractionalPartitioning(m_fractionalPartitioning);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 V 키) ----
void TessellationControlComponent::ToggleFactorColorMode()
{
    m_factorColorMode = !m_factorColorMode;
    m_terrain->SetTessFactorColorMode(m_factorColorMode);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 F 키) ----
void TessellationControlComponent::ToggleFrozen()
{
    m_frozen = !m_frozen;
    m_terrain->SetTessFrozen(m_frozen);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 X 키) ----
void TessellationControlComponent::ToggleCulling()
{
    m_frustumCullingEnabled = !m_frustumCullingEnabled;
    m_terrain->SetTessFrustumCullingEnabled(m_frustumCullingEnabled);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 B 키) ----
void TessellationControlComponent::ToggleDebugBoxes()
{
    m_debugBoxesEnabled = !m_debugBoxesEnabled;
    m_terrain->SetTessDebugBoxesEnabled(m_debugBoxesEnabled);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 O 키) ----
void TessellationControlComponent::DecreaseMaxFactor()
{
    m_maxFactor = std::max(m_maxFactor - kFactorStep, m_minFactor);
    m_terrain->SetTessFactorRange(m_minFactor, m_maxFactor);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 P 키) ----
void TessellationControlComponent::IncreaseMaxFactor()
{
    m_maxFactor = std::min(m_maxFactor + kFactorStep, 64.0f);
    m_terrain->SetTessFactorRange(m_minFactor, m_maxFactor);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 ';' 키. 버튼도 SetRepeatWhileHeld(true) 로 등록한다) ----
void TessellationControlComponent::DecreaseBaseDistance()
{
    m_baseDistance = std::max(m_baseDistance / kBaseDistanceFactor, kMinBaseDistance);
    m_terrain->SetTessBaseDistance(m_baseDistance);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 '\'' 키) ----
void TessellationControlComponent::IncreaseBaseDistance()
{
    m_baseDistance = std::min(m_baseDistance * kBaseDistanceFactor, kMaxBaseDistance);
    m_terrain->SetTessBaseDistance(m_baseDistance);
    RefreshInfoText();
}

// ---- 버튼용 동작 (예전 0 키) ----
void TessellationControlComponent::ResetToDefault()
{
    m_baseDistance = kDefaultBaseDistance;
    m_minFactor = kDefaultMinFactor;
    m_maxFactor = kDefaultMaxFactor;
    m_displacementEnabled = true;
    m_fractionalPartitioning = true;
    m_factorColorMode = false;
    m_frustumCullingEnabled = true;
    m_debugBoxesEnabled = false;
    m_frozen = false;
    ApplyAll();
    RefreshInfoText();
}

std::wstring TessellationControlComponent::FormatThousands(size_t value)
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

void TessellationControlComponent::RefreshInfoText()
{
    if (m_infoText == nullptr || m_terrain == nullptr)
    {
        return;
    }

    if (m_terrain->IsTessellationPipelineFailed())
    {
        m_infoText->SetText(
            L"[하드웨어 테셀레이션] HS/DS 셰이더 컴파일에 실패했습니다.\n"
            L"BasicTerrain.hlsl 의 VSPatch/HSMain_Integer/HSMain_FracOdd/DSMain 을 확인하세요.");
        return;
    }

    const size_t patchCount = m_terrain->GetTessPatchCount();
    const size_t drawnPatches = m_terrain->GetTessDrawnPatchCount();
    const size_t estimatedTriangles = m_terrain->GetTessEstimatedTriangleCount();

    wchar_t buffer[900] = {};

    std::swprintf(buffer, 900,
        L"[하드웨어 테셀레이션]\n"
        L"디스플레이스먼트 %s   파티션 모드 %s   팩터 시각화 %s\n"
        L"패치 : 전체 %s 개 중 %s 개 그림   추정 삼각형 : %s 개\n"
        L"팩터 범위 : %.0f ~ %.0f   기준 거리 : %.0f\n"
        L"컬링 %s   박스 %s   프리즈 %s   (파이프라인 %s)",
        m_displacementEnabled ? L"켬" : L"끔 (매끈한 패치만)",
        m_fractionalPartitioning ? L"fractional_odd" : L"integer",
        m_factorColorMode ? L"켬" : L"끔",
        FormatThousands(patchCount).c_str(),
        FormatThousands(drawnPatches).c_str(),
        FormatThousands(estimatedTriangles).c_str(),
        m_minFactor, m_maxFactor,
        m_baseDistance,
        m_frustumCullingEnabled ? L"켬" : L"끔",
        m_debugBoxesEnabled ? L"켬" : L"끔",
        m_frozen ? L"켬" : L"끔",
        m_terrain->IsTessellationPipelineReady() ? L"준비됨" : L"대기 중");

    m_infoText->SetText(buffer);
}
