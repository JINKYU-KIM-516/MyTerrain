#include "PerlinNoise.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace Noise
{
    namespace
    {
        // 8방향 단위 gradient (45도 간격).
        // 값 노이즈처럼 코너에 "스칼라"를 두지 않고 "방향"을 두는 것이 펄린의 핵심이다.
        // 덕분에 정수 격자점에서 값이 항상 0 이 되고, 마루/골이 셀 내부에 생겨 자연스럽다.
        constexpr float kGradients[8][2] =
        {
            {  1.0f,      0.0f     },
            { -1.0f,      0.0f     },
            {  0.0f,      1.0f     },
            {  0.0f,     -1.0f     },
            {  0.70710678f,  0.70710678f },
            { -0.70710678f,  0.70710678f },
            {  0.70710678f, -0.70710678f },
            { -0.70710678f, -0.70710678f },
        };

        // 2D 펄린의 이론적 최대치는 약 0.707(√2/2) 이므로 √2 를 곱해 -1~1 로 맞춘다
        constexpr float kNormalize = 1.41421356f;

        // Ken Perlin 의 improved noise 에서 쓰는 5차 곡선.
        //   6t^5 - 15t^4 + 10t^3
        // 1차 미분뿐 아니라 2차 미분도 양 끝에서 0이라, 셀 경계에서 법선이 꺾이지 않는다.
        // (smoothstep 인 3t^2-2t^3 을 쓰면 솔리드 셰이딩에 격자 줄이 남는다)
        inline float Fade(float t)
        {
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        inline float Lerp(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        // 옥타브 수는 성능/의미 양쪽에서 상한이 있다
        inline int ClampOctaves(int octaves)
        {
            return std::clamp(octaves, 1, 12);
        }
    }

    const wchar_t* ToName(Type type)
    {
        switch (type)
        {
        case Type::FBM:    return L"fBm (기본)";
        case Type::Ridged: return L"Ridged (능선형)";
        case Type::Billow: return L"Billow (구름형)";
        default:           return L"알 수 없음";
        }
    }

    // ------------------------------------------------------------
    Perlin::Perlin(unsigned int seed)
    {
        Reseed(seed);
    }

    void Perlin::Reseed(unsigned int seed)
    {
        m_seed = seed;

        std::array<uint8_t, 256> base{};
        for (int i = 0; i < 256; ++i)
        {
            base[static_cast<size_t>(i)] = static_cast<uint8_t>(i);
        }

        std::mt19937 rng(seed);
        std::shuffle(base.begin(), base.end(), rng);

        for (int i = 0; i < 512; ++i)
        {
            m_perm[static_cast<size_t>(i)] = base[static_cast<size_t>(i & 255)];
        }
    }

    float Perlin::Lattice(int xi, int yi, float xf, float yf) const
    {
        const size_t hashed = static_cast<size_t>(m_perm[static_cast<size_t>(xi & 255)] + (yi & 255));
        const int index = m_perm[hashed & 511] & 7;

        return kGradients[index][0] * xf + kGradients[index][1] * yf;
    }

    float Perlin::Noise2D(float x, float y) const
    {
        // 1) 어느 셀 안인지 + 셀 안에서의 위치
        const float floorX = std::floor(x);
        const float floorY = std::floor(y);

        const int xi = static_cast<int>(floorX);
        const int yi = static_cast<int>(floorY);

        const float xf = x - floorX;
        const float yf = y - floorY;

        // 2) 네 코너의 기여도 (gradient 와 거리 벡터의 내적)
        const float n00 = Lattice(xi,     yi,     xf,          yf);
        const float n10 = Lattice(xi + 1, yi,     xf - 1.0f,   yf);
        const float n01 = Lattice(xi,     yi + 1, xf,          yf - 1.0f);
        const float n11 = Lattice(xi + 1, yi + 1, xf - 1.0f,   yf - 1.0f);

        // 3) fade 곡선으로 보간
        const float u = Fade(xf);
        const float v = Fade(yf);

        const float bottom = Lerp(n00, n10, u);
        const float top    = Lerp(n01, n11, u);

        return Lerp(bottom, top, v) * kNormalize;
    }

    float Perlin::FBM(float x, float y, int octaves, float lacunarity, float persistence) const
    {
        octaves = ClampOctaves(octaves);

        float sum = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float normalizer = 0.0f;

        for (int i = 0; i < octaves; ++i)
        {
            sum += Noise2D(x * frequency, y * frequency) * amplitude;
            normalizer += amplitude;

            frequency *= lacunarity;
            amplitude *= persistence;
        }

        return (normalizer > 0.0f) ? (sum / normalizer) : 0.0f;
    }

    float Perlin::Ridged(float x, float y, int octaves, float lacunarity, float persistence) const
    {
        octaves = ClampOctaves(octaves);

        float sum = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float normalizer = 0.0f;

        for (int i = 0; i < octaves; ++i)
        {
            // 0 을 지나는 지점(원래 노이즈의 중간값)이 뾰족한 능선이 된다.
            // 제곱해서 골짜기를 더 넓고 평평하게 만든다.
            float ridge = 1.0f - std::fabs(Noise2D(x * frequency, y * frequency));
            ridge *= ridge;

            sum += ridge * amplitude;
            normalizer += amplitude;

            frequency *= lacunarity;
            amplitude *= persistence;
        }

        const float value = (normalizer > 0.0f) ? (sum / normalizer) : 0.0f;  // 0 ~ 1

        // 다른 타입과 진폭 감각을 맞추기 위해 -1 ~ 1 로 옮긴다
        return value * 2.0f - 1.0f;
    }

    float Perlin::Billow(float x, float y, int octaves, float lacunarity, float persistence) const
    {
        octaves = ClampOctaves(octaves);

        float sum = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float normalizer = 0.0f;

        for (int i = 0; i < octaves; ++i)
        {
            // |n| 은 0 에서 꺾이므로 마루가 뭉실하고 골이 뾰족해진다 (Ridged 의 반대)
            const float billow = std::fabs(Noise2D(x * frequency, y * frequency)) * 2.0f - 1.0f;

            sum += billow * amplitude;
            normalizer += amplitude;

            frequency *= lacunarity;
            amplitude *= persistence;
        }

        return (normalizer > 0.0f) ? (sum / normalizer) : 0.0f;
    }

    float Perlin::Evaluate(float worldX, float worldZ, const Params& params) const
    {
        // 월드 좌표를 노이즈 격자 좌표로 옮긴다.
        // scale 이 "노이즈 한 칸 = 월드 몇 단위" 이므로 나눠주면 된다.
        const float inverseScale = 1.0f / std::max(params.scale, 0.0001f);

        const float x = worldX * inverseScale;
        const float z = worldZ * inverseScale;

        float value = 0.0f;

        switch (params.type)
        {
        case Type::Ridged:
            value = Ridged(x, z, params.octaves, params.lacunarity, params.persistence);
            break;
        case Type::Billow:
            value = Billow(x, z, params.octaves, params.lacunarity, params.persistence);
            break;
        case Type::FBM:
        default:
            value = FBM(x, z, params.octaves, params.lacunarity, params.persistence);
            break;
        }

        // 여기서 좌표를 한 번 더 노이즈로 밀면(domain warping) 강줄기처럼 휘어진 지형이 되고,
        // pow(value, k) 같은 재매핑을 넣으면 평야는 넓고 봉우리는 뾰족해진다.
        return value * params.amplitude;
    }
}
