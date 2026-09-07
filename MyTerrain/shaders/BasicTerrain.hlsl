//==============================================================
// BasicTerrain.hlsl
//
// 지형(그리드) 메시를 그리는 가장 기본적인 셰이더.
//  - 정점: 위치 / 법선 / UV
//  - 픽셀: 방향광 1개(램버트) + 셀 단위 체커 패턴
//
// 와이어프레임 패스에서는 gUseLighting = 0 으로 넘겨
// gBaseColor 를 그대로 출력한다(단색 선).
//
// 이 파일은 실행 중에 D3DCompileFromFile 로 컴파일된다.
// (ShaderUtil::CompileFromFile 참고)
//==============================================================

cbuffer CBTerrain : register(b0)
{
    float4x4 gWorld;            // 월드 행렬
    float4x4 gWorldViewProj;    // 월드 * 뷰 * 투영

    float4   gBaseColor;        // 기본 색상

    float3   gLightDir;         // 방향광이 나아가는 방향(정규화됨)
    float    gUseLighting;      // 1 = 조명/체커 적용, 0 = 단색

    float3   gCameraPos;        // 카메라 월드 위치 (추후 안개/LOD 용)
    float    gCellSize;         // 그리드 한 칸의 크기 (체커 패턴 기준)

    // 3번 높이맵 기법에서만 쓴다.
    //   x = 1 / 높이맵이 덮는 월드 크기
    //   y = Z 방향 부호 (이미지 위쪽을 +Z 에 두면 -1)
    //   z = 고도 색상 모드 (0 = 끔 -> 1·2번 기법과 완전히 동일하게 동작)
    //   w = 예약
    float4   gHeightMapParams;
};

// 높이맵 텍스처. 3번 기법에서만 바인딩되고, 그 외에는 비어 있다(모드가 0이라 읽지 않는다).
Texture2D    gHeightMap    : register(t0);
SamplerState gHeightMapSam : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD1;
};

//---------------------------------------------------------------
// Vertex Shader
//---------------------------------------------------------------
PSInput VSMain(VSInput input)
{
    PSInput output;

    output.position = mul(float4(input.position, 1.0f), gWorldViewProj);
    output.worldPos = mul(float4(input.position, 1.0f), gWorld).xyz;

    // 균등 스케일만 사용하므로 월드 행렬의 3x3 부분을 그대로 써도 된다
    output.normal = normalize(mul(input.normal, (float3x3)gWorld));
    output.uv = input.uv;

    return output;
}

//---------------------------------------------------------------
// 고도 램프 : 0~1 을 지형 색으로 (물 -> 모래 -> 풀 -> 흙 -> 바위 -> 눈)
//
// 색 자체가 목적이 아니라, GPU 가 읽은 높이가 CPU 가 정점에 구워 넣은 높이와
// 같은지 확인하는 것이 목적이다. 색 띠의 경계가 지형의 등고선을 따라가면 맞는 것이고,
// 밀리거나 뒤집혀 보이면 UV 규약이 어긋난 것이다.
//---------------------------------------------------------------
float3 ElevationRamp(float h)
{
    float3 water = float3(0.16f, 0.29f, 0.42f);
    float3 sand  = float3(0.76f, 0.70f, 0.48f);
    float3 grass = float3(0.31f, 0.47f, 0.26f);
    float3 dirt  = float3(0.44f, 0.36f, 0.24f);
    float3 rock  = float3(0.48f, 0.47f, 0.46f);
    float3 snow  = float3(0.94f, 0.95f, 0.97f);

    float3 color = lerp(water, sand,  smoothstep(0.04f, 0.12f, h));
    color = lerp(color, grass, smoothstep(0.12f, 0.24f, h));
    color = lerp(color, dirt,  smoothstep(0.36f, 0.52f, h));
    color = lerp(color, rock,  smoothstep(0.58f, 0.74f, h));
    color = lerp(color, snow,  smoothstep(0.82f, 0.93f, h));

    return color;
}

//---------------------------------------------------------------
// Pixel Shader
//---------------------------------------------------------------
float4 PSMain(PSInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    // 와이어프레임 패스: 조명 없이 단색으로 그린다
    if (gUseLighting < 0.5f)
    {
        return gBaseColor;
    }

    // 평면을 아래에서 봐도 어둡지 않도록 뒷면이면 법선을 뒤집는다
    float3 N = normalize(input.normal);
    if (!isFrontFace)
    {
        N = -N;
    }

    float3 L = normalize(-gLightDir);
    float  ndl = saturate(dot(N, L));

    float3 albedo;

    if (gHeightMapParams.z > 0.5f)
    {
        // ---- 고도 색상 모드 : 높이맵 텍스처를 GPU 에서 직접 읽는다 ----
        // UV 계산은 C++ 쪽 HeightMap::Evaluate 와 글자 그대로 같은 식이어야 한다.
        //   u = worldX / worldSize + 0.5
        //   v = 0.5 + worldZ / worldSize * (flipZ ? -1 : +1)
        float2 uv;
        uv.x = input.worldPos.x * gHeightMapParams.x + 0.5f;
        uv.y = input.worldPos.z * gHeightMapParams.x * gHeightMapParams.y + 0.5f;

        float h01 = gHeightMap.SampleLevel(gHeightMapSam, uv, 0).r;
        albedo = ElevationRamp(h01);
    }
    else
    {
        // ---- 기본 : 셀 단위 체커 패턴 (격자 구조가 솔리드 모드에서도 보이도록) ----
        float2 cell = floor(input.worldPos.xz / max(gCellSize, 0.0001f));
        float  checker = frac((cell.x + cell.y) * 0.5f) * 2.0f;   // 0 또는 1

        albedo = lerp(gBaseColor.rgb, gBaseColor.rgb * 0.72f, checker);
    }

    // 앰비언트 + 디퓨즈
    float3 color = albedo * (0.35f + 0.65f * ndl);

    return float4(color, gBaseColor.a);
}
