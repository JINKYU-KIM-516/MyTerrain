#pragma once
#include "UIText.h"
#include <functional>

// 클릭 가능한 텍스트 버튼.
// UIText를 그대로 사용하되, 텍스트 영역 위에 마우스가 올라오면 밑줄로 강조하고
// 왼쪽 버튼을 누르는 순간 등록된 콜백을 호출한다.
// (텍스트 색은 사양대로 항상 하얀색을 유지한다)
class UIButton : public UIText
{
public:
    using ClickCallback = std::function<void()>;

    void Update(float deltaTime) override;

    void SetOnClick(const ClickCallback& callback) { m_onClick = callback; }

    bool IsHovered() const { return m_hovered; }

    // 클릭 판정 영역을 텍스트 주변으로 조금 넓혀준다 (픽셀)
    void SetPadding(float x, float y) { m_paddingX = x; m_paddingY = y; }

    // true 면 클릭 순간 1회가 아니라, 왼쪽 버튼을 누르고 있는 동안 매 프레임 콜백을
    // 반복 호출한다. 원래 키보드를 누르고 있으면 계속 반응하던 조작(예: 값 조절)을
    // 버튼으로 옮길 때 쓴다. 기본값은 false(클릭 1회 = 콜백 1회).
    void SetRepeatWhileHeld(bool repeat) { m_repeatWhileHeld = repeat; }

private:
    ClickCallback m_onClick;
    bool m_hovered = false;
    float m_paddingX = 8.0f;
    float m_paddingY = 4.0f;
    bool m_repeatWhileHeld = false;
};
