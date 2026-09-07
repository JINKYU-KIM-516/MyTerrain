#pragma once
#include "Component.h"
#include "Transform.h"
#include <vector>
#include <memory>
#include <string>
#include <type_traits>
#include <algorithm>

// Unity 스타일의 GameObject.
// 여러 Component 를 부착(Add)하여 동작을 구성하며,
// 생성 시 Transform 컴포넌트를 기본으로 항상 보유한다.
class GameObject
{
public:
    explicit GameObject(const std::string& name = "GameObject");
    ~GameObject();

    // ---------------- 컴포넌트 ----------------
    // T는 반드시 Component를 상속해야 한다
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");

        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T* rawPtr = component.get();
        rawPtr->SetOwner(this);

        m_components.push_back(std::move(component));

        // 이미 게임이 시작되어 다른 컴포넌트들의 Start가 끝난 뒤에 동적으로 추가된 경우를 대비해
        // GameObject의 Start()에서 한 번 더 각 컴포넌트의 시작 여부를 확인한다 (m_started 플래그 참고)
        return rawPtr;
    }

    template<typename T>
    T* GetComponent() const
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");

        for (const auto& component : m_components)
        {
            T* casted = dynamic_cast<T*>(component.get());
            if (casted != nullptr)
            {
                return casted;
            }
        }
        return nullptr;
    }

    template<typename T>
    std::vector<T*> GetComponents() const
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");

        std::vector<T*> result;
        for (const auto& component : m_components)
        {
            T* casted = dynamic_cast<T*>(component.get());
            if (casted != nullptr)
            {
                result.push_back(casted);
            }
        }
        return result;
    }

    template<typename T>
    bool RemoveComponent()
    {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
        static_assert(!std::is_same<T, Transform>::value, "Transform은 제거할 수 없습니다");

        auto it = std::find_if(m_components.begin(), m_components.end(),
            [](const std::unique_ptr<Component>& c) { return dynamic_cast<T*>(c.get()) != nullptr; });

        if (it != m_components.end())
        {
            (*it)->Destroy();
            m_components.erase(it);
            return true;
        }
        return false;
    }

    // Transform은 모든 GameObject가 항상 보유하는 필수 컴포넌트
    Transform* GetTransform() const { return m_transform; }

    // ---------------- 생명주기 (Scene에서 호출) ----------------
    void Start();
    void Update(float deltaTime);
    void Render();
    void RenderUI();
    void Destroy();

    // ---------------- 상태 ----------------
    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    bool IsActive() const { return m_active; }
    void SetActive(bool active) { m_active = active; }

    // Scene에게 "다음 프레임 정리 시점에 제거해달라"고 표시
    void MarkForDestroy() { m_pendingDestroy = true; }
    bool IsPendingDestroy() const { return m_pendingDestroy; }

    unsigned int GetInstanceId() const { return m_instanceId; }

private:
    std::string m_name;
    unsigned int m_instanceId = 0;

    Transform* m_transform = nullptr; // m_components 안에서 소유됨 (Owning: unique_ptr)
    std::vector<std::unique_ptr<Component>> m_components;

    bool m_active = true;
    bool m_started = false;
    bool m_pendingDestroy = false;
    bool m_destroyed = false;
};
