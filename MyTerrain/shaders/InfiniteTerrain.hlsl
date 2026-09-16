//==============================================================
// InfiniteTerrain.hlsl
//
// 10번 무한 지형 청크 전용 셰이더.
//
// BasicTerrain.hlsl 을 쓰지 않고 따로 둔 이유:
//   - BasicTerrain 의 CBTerrain 에는 1~7번 기법이 쌓아온 상수가 전부 들어있는데,
//     10번이 실제로 쓰는 것은 그중 절반도 안 된다 (높이맵/스플래팅/지오머핑/테셀레이션
//     전부 무관하다). 청크마다 상수 버퍼를 다시 올리는 기법이라 버퍼는 작을수록 좋다.
//   - 대신 10번에만 필요한 것이 하나 있다: 거리 안개. 로드 반경 끝에서 청크가
//     사라지는 경계를 가려주지 않으면 지형이 칼로 자른 듯 끊겨 보인다.
//     (Sky.hlsl / Cloud.hlsl 처럼 "기법이 늘 때 셰이더도 따로 늘린다"는 흐름 그대로다)
//
// 정점 형식은 GridMesh::Vertex 를 그대로 쓴다. morphData(TEXCOORD1)는 6-2 지오머핑
// 전용 필드라 10번에서는 읽지 않지만, 입력 레이아웃을 GridMesh::Vertex 와 맞추려면
// 자리는 선언해 두어야 한다.
//==============================================================

cbuffer CBInfinite : register(b0)
{
    float4x4 gWorld;            // 청크의 월드 행렬 (= 청크 중심으로의 평행이동)
    float4x4 gWorldViewProj;

    float4   gBaseColor;        // 청크 색 (청크 색상 모드/신규 강조에서 청크마다 달라진다)

    float3   gLightDir;         // 방향광이 나아가는 방향(정규화됨)
    float    gUseLighting;      // 1 = 조명/체커 적용, 0 = 단색(와이어프레임 패스)

    float3   gCameraPos;
    float    gCellSize;         // 체커 한 칸의 월드 크기

    // x = 안개 시작 거리, y = 안개 끝 거리, z = 예약, w = 안개 켬/끔
    float4   gFogParams;

    // rgb = 안개 색(= 배경 클리어 색과 같게 맞춘다), a = 예약
    float4   gFogColor;
};

struct VSInput
{
    float3 position  : POSITION;
    float3 normal    : NORMAL;
    float2 uv        : TEXCOORD0;
    float2 morphData : TEXCOORD1;   // 10번은 쓰지 않는다 (레이아웃을 맞추기 위한 자리)
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal   : NORMAL;
};

PSInput VSMain(VSInput input)
{
    PSInput output;

    output.position = mul(float4(input.position, 1.0f), gWorldViewProj);
    output.worldPos = mul(float4(input.position, 1.0f), gWorld).xyz;

    // 청크의 월드 행렬은 평행이동뿐이라 법선은 그대로 써도 되지만,
    // 나중에 스케일을 넣더라도 깨지지 않도록 3x3 부분을 곱해둔다.
    output.normal = normalize(mul(input.normal, (float3x3)gWorld));

    return output;
}

// 거리 안개. 로드 반경 밖에서 청크가 사라지는 경계를 배경색으로 덮어준다.
// 시작~끝 사이를 선형으로 섞는 가장 단순한 형태다 -- 안개 자체가 주제가 아니라
// "청크 경계를 가린다"는 목적만 달성하면 되기 때문이다.
float3 ApplyFog(float3 color, float3 worldPos)
{
    if (gFogParams.w < 0.5f)
    {
        return color;
    }

    float d = distance(worldPos, gCameraPos);
    float f = saturate((d - gFogParams.x) / max(gFogParams.y - gFogParams.x, 0.0001f));

    return lerp(color, gFogColor.rgb, f);
}

float4 PSMain(PSInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    // 와이어프레임 패스: 조명 없이 단색. 다만 안개는 걸어준다 --
    // 선만 멀리까지 또렷하게 남으면 안개로 가려둔 로드 경계가 도로 드러난다.
    if (gUseLighting < 0.5f)
    {
        return float4(ApplyFog(gBaseColor.rgb, input.worldPos), gBaseColor.a);
    }

    float3 N = normalize(input.normal);
    if (!isFrontFace)
    {
        N = -N;
    }

    float3 L = normalize(-gLightDir);
    float  ndl = saturate(dot(N, L));

    // 셀 단위 체커 (솔리드 모드에서도 격자 구조가 보이도록 -- 1~7번과 같은 방식).
    // 월드 좌표 기준이므로 청크가 달라도 무늬가 이어진다.
    float2 cell = floor(input.worldPos.xz / max(gCellSize, 0.0001f));
    float  checker = frac((cell.x + cell.y) * 0.5f) * 2.0f;   // 0 또는 1

    float3 albedo = lerp(gBaseColor.rgb, gBaseColor.rgb * 0.72f, checker);

    float3 color = albedo * (0.35f + 0.65f * ndl);
    color = ApplyFog(color, input.worldPos);

    return float4(color, gBaseColor.a);
}
