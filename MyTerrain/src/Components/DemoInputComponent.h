#pragma once
#include "../GameObject/Component.h"

// 프레임워크 동작 확인용 예제 컴포넌트.
// - WASD / 방향키로 GameObject의 Transform 위치를 이동시키고
// - 마우스 왼쪽 버튼을 누르는 동안 마우스 이동량으로 회전시키며
// - 각 생명주기(Start/Update/Destroy) 호출을 디버그 출력으로 확인할 수 있다.
class DemoInputComponent : public Component
{
public:
    void Start() override;
    void Update(float deltaTime) override;
    void Destroy() override;

private:
    float m_moveSpeed = 3.0f;      // 초당 이동 속도
    float m_rotateSpeed = 0.2f;    // 마우스 이동량 대비 회전 속도(도)
};
