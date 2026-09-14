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

    // 4번 스플래팅 기법에서만 쓴다.
    //   x = 텍스처 타일링 배율 (worldPos.xz 에 곱해서 UV 로 쓴다)
    //   y = 경사 임계값 시작 (0 = 평지, 1 = 수직)
    //   z = 경사 임계값 끝
    //   w = 스플래팅 모드 (0 = 끔 -> 1~3번 기법과 완전히 동일하게 동작)
    float4   gSplatParams;

    // 6-2 지오머핑에서만 쓴다.
    //   x = morph 하지 않는 정점 레벨의 하한 (= 실제 최고 LOD 레벨).
    //       이 레벨 이상인 정점은 어차피 사라지지 않으므로 움직이지 않는다.
    //   y = 기준 거리 (레벨 0 이 유지되는 거리)
    //   z = morph 구간 폭 (레벨 경계 거리의 몇 %부터 morph 를 시작할지, 0~0.5)
    //   w = 지오머핑 켬/끔 (0 = 끔 -> 1~6-1번 기법과 완전히 동일하게 동작)
    float4   gLodParams;

    // xyz = LOD 기준 위치 (프리즈 중이면 얼려둔 카메라 위치)
    // w   = morph 계수 시각화 모드 (0 = 끔)
    float4   gLodOrigin;
};

// 7번 하드웨어 테셀레이션 전용 상수. HS/DS 에서만 읽는다(PS 는 시각화 모드 하나만 본다).
// CBTerrain 과 분리해 둔 이유는, 이 값들이 "패치" 개념에서만 의미가 있어서
// 1~6번 기법과 완전히 무관하기 때문이다 -- 항상 바인딩해 두지만 테셀레이션을
// 쓰지 않는 기법에서는 gTessOrigin.w 가 0 이라 PS 쪽 분기도 그냥 지나간다.
cbuffer CBTessellation : register(b1)
{
    float4x4 gViewProj;          // 뷰 * 투영 (DS 가 월드 좌표를 클립 공간으로 바꿀 때 쓴다)

    // x = 기준 거리(이 안쪽이면 최대 팩터), y = 최대 팩터, z = 최소 팩터, w = 예약
    float4   gTessFactorParams;

    // x = heightScale, y = heightOffset (HeightMap::Params 와 같은 값 -- DS 가 높이맵을
    //     다시 샘플링해 정점을 밀어 올릴 때 0~1 값을 월드 높이로 바꾸는 데 쓴다)
    // z = displacement 켬/끔 (0 = 끔 -> 코너 4개를 쌍선형 보간만 한, 완전히 매끈한 패치)
    // w = 밀어 올린 뒤 법선을 다시 계산할 때 쓰는 월드 단위 샘플 간격(epsilon)
    float4   gTessHeightParams;

    // xyz = 팩터 계산 기준 위치 (프리즈 중이면 얼려둔 카메라 위치)
    // w   = 팩터 시각화 모드 (0 = 끔, 1 = 켬 -> 파랑 낮음 / 빨강 높음)
    float4   gTessOrigin;
};

// 높이맵 텍스처. 3·4번 기법에서 바인딩된다(모드가 둘 다 0이면 읽지 않는다).
// 7번은 도메인 셰이더에서 같은 텍스처를 t0/s0 로 다시 읽어 정점을 밀어 올린다.
Texture2D    gHeightMap    : register(t0);
SamplerState gHeightMapSam : register(s0);

// 스플래팅 디퓨즈 텍스처 배열(모래/잔디/바위/눈, 이 순서로 슬라이스 0~3).
// 4번 기법에서만 바인딩되고, 그 외에는 비어 있다(모드가 0이라 읽지 않는다).
Texture2DArray gSplatTextures : register(t1);
SamplerState   gSplatSampler  : register(s1);

struct VSInput
{
    float3 position      : POSITION;
    float3 normal        : NORMAL;
    float2 uv            : TEXCOORD0;

    // 6-2 지오머핑용. x = 이 정점이 사라질 때의 목표 높이, y = 정점 레벨.
    // GridMesh::Generate 가 구워 넣는다.
    float2 morphData : TEXCOORD1;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD1;
    float  morph    : TEXCOORD2;   // 6-2 시각화용
};

//---------------------------------------------------------------
// 6-2 지오머핑
//---------------------------------------------------------------
// morph 계수를 "정점 자신의 레벨과 거리" 로만 계산하는 것이 이 기법의 핵심이다.
//
// 청크마다 하나의 계수를 쓰면, 같은 정점을 공유하는 두 청크가 (레벨이 다르거나
// 청크 중심까지의 거리가 달라서) 서로 다른 값을 얻고, 그 정점의 높이가 어긋나
// 이음매를 없애려다 새 이음매를 만든다. 정점 정보만으로 계산하면 어느 청크가
// 그리든 같은 값이 나오므로 경계가 항상 맞는다.
//
// 정점은 "정점 레벨 -> 정점 레벨 + 1" 전환에서 딱 한 번 사라진다. 그래서 그 전환
// 거리(기준 거리 * 2^정점레벨)에 닿는 순간 계수가 1 이 되도록 맞춰두면, 청크가
// 레벨을 올리는 바로 그 순간 사라질 정점들이 이미 거친 표면 위에 올라가 있다
// -- 그래서 전환이 눈에 띄지 않는다.
float ComputeMorph(float3 worldPos, float vertexLevel)
{
    if (gLodParams.w < 0.5f)
    {
        return 0.0f;
    }

    // 최고 레벨까지 살아남는 정점은 사라질 일이 없으니 움직이지 않는다.
    if (vertexLevel >= gLodParams.x)
    {
        return 0.0f;
    }

    float distanceToOrigin = distance(worldPos, gLodOrigin.xyz);

    float threshold  = gLodParams.y * exp2(vertexLevel);   // 이 정점이 사라지는 거리
    float morphStart = threshold * (1.0f - gLodParams.z);

    return saturate((distanceToOrigin - morphStart) / max(threshold - morphStart, 0.0001f));
}

//---------------------------------------------------------------
// Vertex Shader
//---------------------------------------------------------------
PSInput VSMain(VSInput input)
{
    PSInput output;

    // morph 계수는 "움직이기 전" 위치로 계산해야 한다. 그래야 이 정점을 공유하는
    // 두 청크가 같은 값을 얻는다.
    float3 basePos = mul(float4(input.position, 1.0f), gWorld).xyz;
    float  morph = ComputeMorph(basePos, input.morphData.y);

    float3 localPos = input.position;
    localPos.y = lerp(localPos.y, input.morphData.x, morph);

    output.position = mul(float4(localPos, 1.0f), gWorldViewProj);
    output.worldPos = mul(float4(localPos, 1.0f), gWorld).xyz;

    // 균등 스케일만 사용하므로 월드 행렬의 3x3 부분을 그대로 써도 된다.
    // 법선은 morph 하지 않는다 -- 부모 법선까지 정점에 넣으면 12바이트가 더 늘고,
    // 실루엣이 튀는 기하 팝핑과 달리 명암 변화는 거의 눈에 띄지 않기 때문이다.
    output.normal = normalize(mul(input.normal, (float3x3)gWorld));
    output.uv = input.uv;
    output.morph = morph;

    return output;
}

//---------------------------------------------------------------
// 높이맵 텍스처에서 정규화된 높이(0~1)를 읽는다. 3·4번 기법이 함께 쓴다.
// UV 식은 C++ 쪽 HeightMap::Evaluate 와 글자 그대로 같아야 한다.
//   u = worldX / worldSize + 0.5
//   v = 0.5 + worldZ / worldSize * (flipZ ? -1 : +1)
//---------------------------------------------------------------
float SampleHeight01(float3 worldPos)
{
    float2 uv;
    uv.x = worldPos.x * gHeightMapParams.x + 0.5f;
    uv.y = worldPos.z * gHeightMapParams.x * gHeightMapParams.y + 0.5f;

    return gHeightMap.SampleLevel(gHeightMapSam, uv, 0).r;
}

//=================================================================
// 7. 하드웨어 테셀레이션 (VSPatch -> HS -> 테셀레이터(고정 기능) -> DS)
//=================================================================
// 1~6-1번은 VSMain 이 최종 클립 공간 좌표(SV_POSITION)까지 만들었다. 여기서는
// VS 가 "월드 공간" 까지만 만들고(패치 컨트롤 포인트), 최종 좌표는 DS 가 만든다 --
// 쌍선형 보간은 클립 공간이 아니라 월드 공간에서 해야 원근 나눗셈 전후가 뒤섞이지
// 않는다(클립 공간 보간 후 투영하면 직선이 휘어 보인다).
//
// 컨트롤 포인트 4개의 순서는 PatchGrid::Build / GridMesh::Generate 의 i0,i1,i2,i3
// 규약과 같다 : (u,v) = (0,0) (1,0) (0,1) (1,1). SV_DomainLocation 이 그대로 이
// 순서의 쌍선형 보간 좌표가 된다.
struct VSPatchOutput
{
    float3 worldPos    : POSITION;
    float3 worldNormal : NORMAL;
    float2 uv          : TEXCOORD0;
};

VSPatchOutput VSPatch(VSInput input)
{
    VSPatchOutput output;

    output.worldPos = mul(float4(input.position, 1.0f), gWorld).xyz;
    output.worldNormal = normalize(mul(input.normal, (float3x3)gWorld));
    output.uv = input.uv;

    return output;
}

//-----------------------------------------------------------------
// 패치 상수 함수 : 변(edge) 4개 + 안쪽 팩터를 계산한다. 컨트롤 포인트당이 아니라
// 패치당 한 번만 불린다.
//
// 이음매가 저절로 안 생기는 이유 : 변 하나의 팩터를 "그 변의 두 월드 좌표 끝점"
// 만으로 계산하면, 그 변을 공유하는 이웃 패치도 같은 두 끝점을 넣고 같은 식을
// 돌리므로 항상 같은 값이 나온다. 6-2 처럼 "누가 누구에게 맞출지" CPU 에서 조율할
// 필요가 원천적으로 없다 -- 대신 이 함수는 반드시 그 변의 두 컨트롤 포인트만 보고
// 계산해야 한다(패치 중심이나 다른 변 정보가 섞이면 대칭이 깨진다).
//-----------------------------------------------------------------
float TessEdgeFactor(float3 worldA, float3 worldB)
{
    float3 mid = (worldA + worldB) * 0.5f;
    float  dist = distance(mid, gTessOrigin.xyz);

    const float baseDistance = max(gTessFactorParams.x, 0.0001f);
    const float maxFactor = gTessFactorParams.y;
    const float minFactor = gTessFactorParams.z;

    // 기준 거리 안쪽이면 최대 팩터, 그 뒤로는 거리에 반비례해서 줄어든다
    // (정수/fractional_odd 파티션 모드가 이 연속값을 알아서 양자화한다 --
    //  여기서 미리 floor 하지 않는 것이 핵심이다. 그래야 fractional 모드가
    //  삼각형이 갑자기 나타나는 대신 한 점에서 자라나오게 부드럽게 만들어준다).
    float factor = maxFactor * saturate(baseDistance / dist);
    return clamp(factor, minFactor, maxFactor);
}

struct HSConstantOutput
{
    float edgeTess[4]   : SV_TessFactor;
    float insideTess[2] : SV_InsideTessFactor;
};

HSConstantOutput PatchConstantHS(InputPatch<VSPatchOutput, 4> patch)
{
    HSConstantOutput output;

    // SV_TessFactor 의 쿼드 도메인 규약 : [0]=(0,0)-(0,1) 변, [1]=(0,0)-(1,0) 변,
    // [2]=(1,0)-(1,1) 변, [3]=(0,1)-(1,1) 변. 컨트롤 포인트 순서가 i0,i1,i2,i3 이므로
    // 각각 (i0,i2) (i0,i1) (i1,i3) (i2,i3) 이다.
    const float f0 = TessEdgeFactor(patch[0].worldPos, patch[2].worldPos);
    const float f1 = TessEdgeFactor(patch[0].worldPos, patch[1].worldPos);
    const float f2 = TessEdgeFactor(patch[1].worldPos, patch[3].worldPos);
    const float f3 = TessEdgeFactor(patch[2].worldPos, patch[3].worldPos);

    output.edgeTess[0] = f0;
    output.edgeTess[1] = f1;
    output.edgeTess[2] = f2;
    output.edgeTess[3] = f3;

    // 안쪽 팩터는 변 팩터의 평균으로 근사한다 (정확한 값이 아니어도 크랙이 없는
    // 성질과는 무관하다 -- 그 성질은 오직 변 팩터가 이웃과 일치하는 것에서 온다).
    const float inside = (f0 + f1 + f2 + f3) * 0.25f;
    output.insideTess[0] = inside;
    output.insideTess[1] = inside;

    return output;
}

// partition 모드(integer / fractional_odd 등)는 HS 함수에 붙는 컴파일타임 속성이라
// 런타임에 값 하나로 못 바꾼다. 그래서 몸통이 같은 HS 를 두 벌 컴파일해두고
// TerrainRenderer 가 어느 쪽을 바인딩할지로 전환한다 (HSMain_Integer / HSMain_FracOdd).
[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("PatchConstantHS")]
[maxtessfactor(64.0)]
VSPatchOutput HSMain_Integer(InputPatch<VSPatchOutput, 4> patch, uint id : SV_OutputControlPointID)
{
    return patch[id];
}

[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("PatchConstantHS")]
[maxtessfactor(64.0)]
VSPatchOutput HSMain_FracOdd(InputPatch<VSPatchOutput, 4> patch, uint id : SV_OutputControlPointID)
{
    return patch[id];
}

//-----------------------------------------------------------------
// Domain Shader : 테셀레이터가 만든 (u,v) 마다 한 번씩 불린다.
//-----------------------------------------------------------------
[domain("quad")]
PSInput DSMain(HSConstantOutput hsConst, float2 uv : SV_DomainLocation,
              const OutputPatch<VSPatchOutput, 4> patch)
{
    PSInput output;

    // ---- 위치 : 코너 4개를 (u,v) 로 쌍선형 보간 (i0,i1,i2,i3 = (0,0)(1,0)(0,1)(1,1)) ----
    float3 top = lerp(patch[0].worldPos, patch[1].worldPos, uv.x);
    float3 bottom = lerp(patch[2].worldPos, patch[3].worldPos, uv.x);
    float3 worldPos = lerp(top, bottom, uv.y);

    float3 topN = lerp(patch[0].worldNormal, patch[1].worldNormal, uv.x);
    float3 bottomN = lerp(patch[2].worldNormal, patch[3].worldNormal, uv.x);
    float3 normal = normalize(lerp(topN, bottomN, uv.y));

    float2 topUV = lerp(patch[0].uv, patch[1].uv, uv.x);
    float2 bottomUV = lerp(patch[2].uv, patch[3].uv, uv.x);
    float2 texcoord = lerp(topUV, bottomUV, uv.y);

    // ---- displacement : 코너 보간 대신 높이맵을 다시 샘플링한다 ----
    // 코너만 보간하면(=꺼진 상태) 테셀레이션을 아무리 늘려도 매끈한 곡면 조각만
    // 늘어날 뿐 새 지형 디테일은 안 생긴다. 실제로 더 촘촘해 보이려면 새로 생긴
    // 정점마다 원본 높이맵을 다시 읽어야 한다.
    if (gTessHeightParams.z > 0.5f)
    {
        const float h01 = SampleHeight01(worldPos);
        worldPos.y = h01 * gTessHeightParams.x + gTessHeightParams.y;

        // 법선도 다시 계산한다 (중앙 차분, 높이맵 4번 추가 샘플). 코너 법선을 그대로
        // 쓰면 방금 밀어 올린 표면과 어긋나 음영이 이상해진다. epsilon 은
        // gTessHeightParams.w (월드 단위) -- TessellationControlComponent 가 조절한다.
        const float e = max(gTessHeightParams.w, 0.001f);
        const float hL = SampleHeight01(worldPos + float3(-e, 0.0f, 0.0f)) * gTessHeightParams.x + gTessHeightParams.y;
        const float hR = SampleHeight01(worldPos + float3(e, 0.0f, 0.0f)) * gTessHeightParams.x + gTessHeightParams.y;
        const float hD = SampleHeight01(worldPos + float3(0.0f, 0.0f, -e)) * gTessHeightParams.x + gTessHeightParams.y;
        const float hU = SampleHeight01(worldPos + float3(0.0f, 0.0f, e)) * gTessHeightParams.x + gTessHeightParams.y;

        normal = normalize(float3(hL - hR, 2.0f * e, hD - hU));
    }

    output.position = mul(float4(worldPos, 1.0f), gViewProj);
    output.worldPos = worldPos;
    output.normal = normal;
    output.uv = texcoord;

    // PSMain 의 TEXCOORD2(morph) 자리를 그대로 빌려 쓴다 -- 6-2 의 morph 시각화와
    // 같은 파랑->빨강 컬러맵을 팩터 시각화에도 재사용하기 위해서다 (아래 참고).
    const float avgFactor = (hsConst.edgeTess[0] + hsConst.edgeTess[1] +
                             hsConst.edgeTess[2] + hsConst.edgeTess[3]) * 0.25f;
    output.morph = saturate(avgFactor / max(gTessFactorParams.y, 1.0f));

    return output;
}

//---------------------------------------------------------------
// 4번 텍스처 스플래팅 : 정점의 높이(h, 0~1)와 경사도(slope, 0=평지·1=수직)로
// 모래/잔디/바위/눈 네 레이어의 가중치를 계산한다. 반환값의 네 성분 합은 항상 1이다.
//
//   높이 구간(고정)   : 0.15~0.25 모래->잔디, 0.45~0.55 잔디->바위, 0.75~0.85 바위->눈
//   경사 구간(조절 가능, gSplatParams.yz) : 급해질수록 모래/잔디를 바위 쪽으로 밀어준다
//---------------------------------------------------------------
float4 SplatWeights(float h, float slope)
{
    float wSand  = 1.0f - smoothstep(0.15f, 0.25f, h);
    float wGrass = smoothstep(0.15f, 0.25f, h) * (1.0f - smoothstep(0.45f, 0.55f, h));
    float wRock  = smoothstep(0.45f, 0.55f, h) * (1.0f - smoothstep(0.75f, 0.85f, h));
    float wSnow  = smoothstep(0.75f, 0.85f, h);

    // 경사가 급해질수록 모래/잔디를 깎아 바위로 옮긴다 (넷의 합은 그대로 보존된다)
    float wCliff = smoothstep(gSplatParams.y, gSplatParams.z, slope);
    float toRock = wCliff * (wSand + wGrass);
    wSand  *= (1.0f - wCliff);
    wGrass *= (1.0f - wCliff);
    wRock  += toRock;

    return float4(wSand, wGrass, wRock, wSnow);
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

    if (gSplatParams.w > 0.5f)
    {
        // ---- 4번 텍스처 스플래팅 : 정점 높이/경사도로 뽑은 가중치로 4장을 섞는다 ----
        float h01 = SampleHeight01(input.worldPos);
        float slope = 1.0f - saturate(N.y);   // 0 = 평지, 1 = 수직 (up = (0,1,0) 이므로 dot(N,up) = N.y)

        float4 w = SplatWeights(h01, slope);

        // 타일링 : 텍스처 좌표를 월드 좌표에 직접 걸어서 지형 전체에 반복시킨다.
        // 배율이 너무 작으면(타일이 너무 크면) 흐릿해 보이고, 너무 크면 반복 패턴이 도드라진다.
        float2 tiledUV = input.worldPos.xz * gSplatParams.x;

        float3 cSand  = gSplatTextures.Sample(gSplatSampler, float3(tiledUV, 0.0f)).rgb;
        float3 cGrass = gSplatTextures.Sample(gSplatSampler, float3(tiledUV, 1.0f)).rgb;
        float3 cRock  = gSplatTextures.Sample(gSplatSampler, float3(tiledUV, 2.0f)).rgb;
        float3 cSnow  = gSplatTextures.Sample(gSplatSampler, float3(tiledUV, 3.0f)).rgb;

        albedo = cSand * w.x + cGrass * w.y + cRock * w.z + cSnow * w.w;
    }
    else if (gHeightMapParams.z > 0.5f)
    {
        // ---- 고도 색상 모드 : 높이맵 텍스처를 GPU 에서 직접 읽는다 ----
        float h01 = SampleHeight01(input.worldPos);
        albedo = ElevationRamp(h01);
    }
    else
    {
        // ---- 기본 : 셀 단위 체커 패턴 (격자 구조가 솔리드 모드에서도 보이도록) ----
        float2 cell = floor(input.worldPos.xz / max(gCellSize, 0.0001f));
        float  checker = frac((cell.x + cell.y) * 0.5f) * 2.0f;   // 0 또는 1

        albedo = lerp(gBaseColor.rgb, gBaseColor.rgb * 0.72f, checker);
    }

    // ---- 6-2 morph 계수 시각화 : 0 = 파랑(아직 안 움직임), 1 = 빨강(다음 레벨과 같아짐) ----
    if (gLodOrigin.w > 0.5f)
    {
        albedo = lerp(float3(0.24f, 0.46f, 0.94f), float3(0.94f, 0.31f, 0.22f), saturate(input.morph));
    }

    // ---- 7번 테셀레이션 팩터 시각화 : 0 = 파랑(최소 팩터), 1 = 빨강(최대 팩터) ----
    // DSMain 이 같은 TEXCOORD2(morph) 자리에 팩터를 정규화해서 넣어준다 -- 6-2 와
    // 동시에 켜질 일이 없으므로(서로 다른 기법) 색만 재사용해도 안전하다.
    if (gTessOrigin.w > 0.5f)
    {
        albedo = lerp(float3(0.24f, 0.46f, 0.94f), float3(0.94f, 0.31f, 0.22f), saturate(input.morph));
    }

    // 앰비언트 + 디퓨즈
    float3 color = albedo * (0.35f + 0.65f * ndl);

    return float4(color, gBaseColor.a);
}
