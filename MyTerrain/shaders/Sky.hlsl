//==============================================================
// Sky.hlsl
//
// 8. 스카이맵 (SkyDome) - 절차적 하늘.
//
//  - 정점: SkyDome::Generate 가 만든 반구 위의 방향 벡터 하나뿐이다 (UV/법선 없음).
//  - 정점 셰이더가 클립 공간 z 를 w 로 덮어써 NDC 깊이가 항상 1.0(DirectX 에서
//    가장 먼 값)이 되게 한다 -- 어떤 반지름으로 그리든, 어떤 순서로 그리든 스카이는
//    항상 다른 불투명 오브젝트 뒤에 밀려난다 (SkyRenderer 의 깊이 상태는
//    DepthWriteMask=ZERO, DepthFunc=LESS_EQUAL 이어야 클리어된 깊이 1.0 과도 같아서
//    통과한다).
//  - 픽셀 셰이더는 텍스처 없이 지평선/천정 색을 방향의 y 성분으로 보간하고,
//    태양 방향과의 내적으로 태양 원반과 그 주변 글로우를 더한다.
//
// BasicTerrain.hlsl 과는 완전히 분리된 셰이더 파일이라 상수버퍼(b0)도 별개다 --
// 지형 셰이더의 CBTerrain/CBTessellation 과는 이름만 같은 레지스터일 뿐 아무 관계가
// 없고, 이 파일이 있든 없든 1~7번 기법(BasicTerrain.hlsl)은 전혀 영향받지 않는다.
//==============================================================

cbuffer CBSky : register(b0)
{
    float4x4 gWorldViewProj;

    float4 gHorizonColor;   // rgb = 지평선(카메라 높이) 색
    float4 gZenithColor;    // rgb = 천정(하늘 꼭대기) 색

    // xyz = 태양 방향(카메라에서 하늘을 향한 단위벡터), w = 태양 각반경의 cos 임계값
    float4 gSunDirectionAndSize;

    // rgb = 태양 색, w = 글로우(번짐) 지수 -- 클수록 좁고 진하게, 작을수록 넓고 은은하게
    float4 gSunColorAndGlow;
};

struct VSInput
{
    float3 direction : POSITION;
};

struct PSInput
{
    float4 position  : SV_POSITION;
    float3 direction : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;

    output.position = mul(float4(input.direction, 1.0f), gWorldViewProj);

    // 원근 분할(perspective divide) 후 NDC z 가 1.0(DirectX 의 가장 먼 값)이 되도록
    // 강제한다. "스카이는 항상 가장 멀리 있다" 는 규칙을 반지름 값이나 근/원 평면
    // 설정과 무관하게 보장하는 표준 트릭이다.
    output.position.z = output.position.w;

    output.direction = input.direction;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 dir = normalize(input.direction);

    // 지평선(y=0) -> 천정(y=1) 으로 갈수록 천정색에 가까워진다.
    // 지수(0.45)를 걸어서 지평선 근처의 색 변화를 더 완만하게 만든다 --
    // 1 그대로 쓰면 지평선 바로 위에서부터 천정색이 너무 빨리 섞여 들어온다.
    const float heightT = pow(saturate(dir.y), 0.45f);
    float3 skyColor = lerp(gHorizonColor.rgb, gZenithColor.rgb, heightT);

    const float3 sunDir = normalize(gSunDirectionAndSize.xyz);
    const float sunCosThreshold = gSunDirectionAndSize.w;
    const float sunDot = dot(dir, sunDir);

    // 태양 원반: 각반경 임계값 바로 위에서 급격히 밝아지는 좁은 디스크.
    const float sunDisk = smoothstep(sunCosThreshold - 0.0015f, sunCosThreshold + 0.0015f, sunDot);

    // 태양 글로우: 넓고 부드럽게 번지는 후광. 태양이 지평선 아래로 충분히 내려가면
    // (sunDir.y 가 -0.125 아래) 완전히 사라지도록 별도로 한 번 더 감쇠한다 --
    // 그렇지 않으면 자정에도 "반대편 하늘"에 은은한 얼룩이 남는다.
    const float belowHorizonFade = saturate(sunDir.y * 4.0f + 0.5f);
    const float sunGlow = pow(saturate(sunDot), gSunColorAndGlow.w) * belowHorizonFade;

    const float3 color = skyColor + gSunColorAndGlow.rgb * (sunDisk + sunGlow * 0.35f);

    return float4(color, 1.0f);
}
