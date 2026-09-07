#pragma once
#include <array>
#include <cstdint>

// 지형 높이의 재료가 되는 "펄린 노이즈" 모듈.
//
// 왜 그냥 rand() 를 쓰지 않는가:
//   rand() 는 이웃한 두 점이 아무 상관이 없어서(백색 잡음) 지형이 되지 못한다.
//   노이즈 함수는 다음 세 가지를 동시에 만족하는 "연속적인 난수 필드"다.
//     - 결정적    : 같은 (x, z) 는 언제나 같은 값. 청크를 나눠 만들어도 경계가 이어진다
//     - 연속적    : 미분 가능해서 법선이 튀지 않는다
//     - 대역 제한 : 한 옥타브는 대략 한 가지 크기의 굴곡만 만든다
//
// 구성
//   Perlin::Noise2D  : 단일 옥타브 (-1 ~ 1)
//   Perlin::FBM      : 여러 옥타브를 겹친 것. 실제 지형처럼 보이게 하는 핵심
//   Perlin::Ridged   : 1 - |n| 을 제곱 -> 날카로운 능선(산맥)
//   Perlin::Billow   : |n| -> 뭉실한 언덕(모래언덕/구름)
//   Perlin::Evaluate : 월드 좌표를 받아 최종 높이(y)를 돌려준다
namespace Noise
{
    // 옥타브를 어떤 방식으로 합칠지
    enum class Type
    {
        FBM = 0,   // 기본. 부드러운 구릉
        Ridged,    // 능선형. 날카로운 산맥
        Billow,    // 구름형. 뭉실한 언덕
        Count
    };

    const wchar_t* ToName(Type type);

    // 지형 하나를 결정하는 파라미터 묶음
    struct Params
    {
        // 노이즈 격자 한 칸이 월드에서 몇 단위인지.
        // 키우면 굴곡이 넓어지고(대륙), 줄이면 잘아진다(자갈밭).
        float scale = 64.0f;

        // 겹칠 옥타브 수. 셀 크기보다 잘게 진동하는 옥타브는 낭비이므로
        // 분할 수에 맞춰 올리는 것이 맞다.
        int octaves = 6;

        // 옥타브마다 주파수를 몇 배로 할지 (보통 2.0)
        float lacunarity = 2.0f;

        // 옥타브마다 진폭을 몇 배로 할지 (보통 0.5). 키우면 거칠어진다.
        float persistence = 0.5f;

        // 최종 높이 배율 (월드 단위)
        float amplitude = 30.0f;

        // 시드. permutation 테이블을 섞는 값이라 바꾸면 완전히 다른 지형이 된다.
        unsigned int seed = 1337u;

        Type type = Type::FBM;
    };

    class Perlin
    {
    public:
        explicit Perlin(unsigned int seed = 1337u);

        void Reseed(unsigned int seed);
        unsigned int GetSeed() const { return m_seed; }

        // 단일 옥타브. 결과 범위는 약 -1 ~ 1 (정수 격자점에서는 정확히 0)
        float Noise2D(float x, float y) const;

        // ---- 옥타브 누적 (모두 -1 ~ 1 로 정규화해서 돌려준다) ----
        float FBM(float x, float y, int octaves, float lacunarity, float persistence) const;
        float Ridged(float x, float y, int octaves, float lacunarity, float persistence) const;
        float Billow(float x, float y, int octaves, float lacunarity, float persistence) const;

        // 월드 좌표 -> 최종 높이(y). GridMesh::HeightFunc 에 그대로 연결하면 된다.
        float Evaluate(float worldX, float worldZ, const Params& params) const;

    private:
        // 격자 코너 하나의 기여도 = dot(그 코너의 gradient, 코너로부터의 거리 벡터)
        float Lattice(int xi, int yi, float xf, float yf) const;

    private:
        // 0~255 를 섞어놓은 뒤 두 번 이어붙인 해시 테이블.
        // 두 번 이어붙이는 이유는 perm[xi] + zi 가 최대 510 까지 커지기 때문이다.
        std::array<uint8_t, 512> m_perm{};
        unsigned int m_seed = 0;
    };
}
