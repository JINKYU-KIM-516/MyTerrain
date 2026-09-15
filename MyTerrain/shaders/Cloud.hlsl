//==============================================================
// Cloud.hlsl
//
// 9. 동적 왜곡 구름 (Perturbed Clouds)
//
//  - 정점: Sky.hlsl과 똑같이 SkyDome::Generate가 만든 반구 위의 방향 벡터 하나뿐이다
//    (CloudRenderer가 SkyDome::Generate를 반지름만 다르게 재사용한다 -- 8번 문서/설명
//    문서가 미리 적어둔 경로 그대로. 새 지오메트리 모듈은 필요 없었다).
//  - 정점 셰이더는 Sky.hlsl과 같은 "z = w" 트릭으로 NDC 깊이를 항상 1.0으로 고정한다.
//    스카이(8번)를 먼저 그리고 이 셰이더를 그 다음에 그리면, 둘 다 깊이가 정확히
//    1.0이라 LESS_EQUAL 비교가 통과해서 구름이 하늘 위에 알파 블렌딩으로 얹힌다.
//  - 픽셀 셰이더는 텍스처 없이, 방향 벡터를 그대로 3D 좌표로 삼아 값노이즈 기반 fbm을
//    두 번 도메인 워핑(domain warping)해서 구름 밀도를 만든다. 밀도를 커버리지
//    임계값으로 다시 한 번 걸러 알파로 쓴다 -- 그래서 이 드로우는 프로젝트 최초로
//    알파 블렌딩(BlendState)이 켜져 있다 (CloudRenderer 참고).
//
// BasicTerrain.hlsl / Sky.hlsl 과는 완전히 분리된 셰이더 파일이라 상수버퍼(b0)도
// 별개다 -- 이 파일이 있든 없든 1~8번 기법은 전혀 영향받지 않는다.
//==============================================================

cbuffer CBCloud : register(b0)
{
    float4x4 gWorldViewProj;

    float4 gColorAndCoverage;      // rgb = 낮 구름색, w = 커버리지 임계값(0~1, 낮을수록 구름 많음)
    float4 gSunDirAndBrightness;   // xyz = 태양 방향(카메라 기준 단위벡터), w = 주/야 밝기(0=밤,1=낮)
    float4 gWarpParams;            // x = 노이즈 스케일, y = 워프 강도, z = 시간(초), w = 가장자리 소프트니스
    float4 gWindAndRim;            // xy = 바람 방향(단위벡터, xz평면), z = 바람 속도, w = 림 라이트 지수
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

    // Sky.hlsl 과 같은 깊이 고정 트릭. 스카이보다 반지름이 작더라도(CloudRenderer 기본값
    // 900 vs 스카이 1000) NDC 깊이는 여기서 다시 1.0으로 덮어써지므로 실제로는 반지름
    // 차이가 깊이에 영향을 주지 않는다 -- 그리는 순서(지형 -> 스카이 -> 구름)만이
    // "구름이 하늘 위에 얹힌다"는 결과를 결정한다.
    output.position.z = output.position.w;

    output.direction = input.direction;
    return output;
}

// ---------------- 해시 기반 3D 값노이즈 ----------------
// 텍스처 없이 순수 계산만으로 만든 노이즈다. SkyDome 정점이 이미 "방향(단위벡터)"을
// 들고 있으므로, 구면 UV를 따로 만들 필요 없이 이 방향을 그대로 3D 좌표로 사용한다 --
// 극(zenith) 부근에서 UV가 몰리는 이음매 문제 자체가 애초에 생기지 않는다.
float hash13(float3 p)
{
    p = frac(p * 0.1031f);
    p += dot(p, p.yzx + 19.19f);
    return frac((p.x + p.y) * p.z);
}

float ValueNoise3D(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    float3 u = f * f * (3.0f - 2.0f * f); // smoothstep 보간(6t^5-15t^4+10t^3 만큼 비싸지 않아도 충분하다)

    float n000 = hash13(i + float3(0, 0, 0));
    float n100 = hash13(i + float3(1, 0, 0));
    float n010 = hash13(i + float3(0, 1, 0));
    float n110 = hash13(i + float3(1, 1, 0));
    float n001 = hash13(i + float3(0, 0, 1));
    float n101 = hash13(i + float3(1, 0, 1));
    float n011 = hash13(i + float3(0, 1, 1));
    float n111 = hash13(i + float3(1, 1, 1));

    float nx00 = lerp(n000, n100, u.x);
    float nx10 = lerp(n010, n110, u.x);
    float nx01 = lerp(n001, n101, u.x);
    float nx11 = lerp(n011, n111, u.x);

    float nxy0 = lerp(nx00, nx10, u.y);
    float nxy1 = lerp(nx01, nx11, u.y);

    return lerp(nxy0, nxy1, u.z); // 0~1
}

// 옥타브를 누적하는 fbm. 정규화(normalizer)로 옥타브 수와 무관하게 0~1 범위를 유지한다
// (프로젝트 2번 펄린 노이즈 문서의 fBm 정규화와 같은 방식 -- CPU Noise::Perlin 은
// D3D 를 모르는 순수 함수라 셰이더에서 그대로 못 쓰므로, 여기서는 GPU 전용으로
// 새로 짠 노이즈다).
float Fbm3D(float3 p, int octaves)
{
    float sum = 0.0f;
    float amplitude = 0.5f;
    float freq = 1.0f;
    float normalizer = 0.0f;

    [loop]
    for (int i = 0; i < octaves; ++i)
    {
        sum += ValueNoise3D(p * freq) * amplitude;
        normalizer += amplitude;
        amplitude *= 0.5f;
        freq *= 2.0f;
    }

    return sum / max(normalizer, 0.0001f);
}

// 노이즈 좌표를 다른 두 fbm 결과로 만든 벡터만큼 밀어서 표본을 뽑는다
// (설명 문서의 q = fbm(p + ...), r = fbm(p + k1*q + ...) 2-pass 워프와 같은 것).
float2 Warp2D(float3 p)
{
    const float qx = Fbm3D(p + float3(11.3f, 5.7f, 2.1f), 4);
    const float qy = Fbm3D(p + float3(3.1f, 8.4f, 17.2f), 4);
    return float2(qx, qy) * 2.0f - 1.0f; // -1~1
}

float4 PSMain(PSInput input) : SV_TARGET
{
    const float3 dir = normalize(input.direction);

    // 바람: 방향(xz평면 단위벡터) * 속도 * 경과시간을 그대로 노이즈 좌표 오프셋으로 쓴다.
    const float2 wind = gWindAndRim.xy * gWindAndRim.z * gWarpParams.z;
    const float3 windOffset = float3(wind.x, 0.0f, wind.y);

    // ---- 2-pass 도메인 워핑 ----
    const float3 p0 = dir * gWarpParams.x + windOffset;
    const float2 warp1 = Warp2D(p0);

    const float3 p1 = p0 + gWarpParams.y * float3(warp1.x, 0.0f, warp1.y);
    const float2 warp2 = Warp2D(p1 * 1.7f + float3(3.7f, 1.1f, 0.0f));

    const float3 p2 = p1 + gWarpParams.y * 0.5f * float3(warp2.x, 0.0f, warp2.y);
    const float density = Fbm3D(p2, 5);

    // ---- 밀도 -> 커버리지(알파) ----
    // 임계값(gColorAndCoverage.w) 주변을 소프트니스(gWarpParams.w) 폭만큼 부드럽게 걸러서
    // 구름 가장자리가 칼같이 잘리지 않게 한다.
    const float coverage = smoothstep(gColorAndCoverage.w - gWarpParams.w,
                                       gColorAndCoverage.w + gWarpParams.w,
                                       density);

    // ---- 색: 주/야 밝기 보간 + 태양 방향 림 라이트 ----
    const float3 nightColor = float3(0.05f, 0.06f, 0.09f);
    const float3 dayColor = gColorAndCoverage.rgb;
    const float dayFactor = gSunDirAndBrightness.w;
    const float3 baseColor = lerp(nightColor, dayColor, dayFactor);

    const float3 sunDir = normalize(gSunDirAndBrightness.xyz);
    const float rim = pow(saturate(dot(dir, sunDir)), gWindAndRim.w);
    const float3 finalColor = baseColor + rim * dayFactor * 0.5f;

    return float4(finalColor, coverage);
}
