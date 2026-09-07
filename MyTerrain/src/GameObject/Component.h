#pragma once

class GameObject;

// 모든 컴포넌트의 기반 클래스 (Unity의 MonoBehaviour와 유사한 역할)
// GameObject에 부착되어 Start -> Update -> Render 순으로 매 프레임 호출되며,
// GameObject가 파괴되거나 씬에서 제거될 때 Destroy가 호출된다.
class Component
{
public:
    friend class GameObject; // GameObject::AddComponent 에서 SetOwner 호출을 위해

    virtual ~Component() = default;

    // 최초 1회, 첫 Update 이전에 호출된다
    virtual void Start() {}

    // 매 프레임 논리 갱신 (deltaTime: 초 단위)
    virtual void Update(float deltaTime) {}

    // 매 프레임 렌더링 단계에서 호출된다
    virtual void Render() {}

    // 컴포넌트/게임오브젝트가 파괴될 때 1회 호출된다 (리소스 해제 등)
    virtual void Destroy() {}

    GameObject* GetGameObject() const { return m_gameObject; }

    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }

private:
    void SetOwner(GameObject* owner) { m_gameObject = owner; }

private:
    GameObject* m_gameObject = nullptr;
    bool m_enabled = true;
    bool m_started = false; // GameObject 내부에서 Start 호출 여부 추적용
};
