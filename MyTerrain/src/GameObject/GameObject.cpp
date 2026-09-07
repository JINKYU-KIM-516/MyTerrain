#include "GameObject.h"

namespace
{
    unsigned int g_nextInstanceId = 1;
}

GameObject::GameObject(const std::string& name)
    : m_name(name)
    , m_instanceId(g_nextInstanceId++)
{
    // 모든 GameObject는 생성과 동시에 Transform 컴포넌트를 기본으로 가진다
    m_transform = AddComponent<Transform>();
}

GameObject::~GameObject()
{
    if (!m_destroyed)
    {
        Destroy();
    }
}

void GameObject::Start()
{
    for (auto& component : m_components)
    {
        if (!component->m_started)
        {
            component->Start();
            component->m_started = true;
        }
    }
    m_started = true;
}

void GameObject::Update(float deltaTime)
{
    if (!m_active)
    {
        return;
    }

    // 게임 도중 동적으로 추가된 컴포넌트를 위해 Start를 먼저 보장한다
    for (auto& component : m_components)
    {
        if (!component->m_started)
        {
            component->Start();
            component->m_started = true;
        }
    }

    for (auto& component : m_components)
    {
        if (component->IsEnabled())
        {
            component->Update(deltaTime);
        }
    }
}

void GameObject::Render()
{
    if (!m_active)
    {
        return;
    }

    for (auto& component : m_components)
    {
        if (component->IsEnabled())
        {
            component->Render();
        }
    }
}

void GameObject::Destroy()
{
    if (m_destroyed)
    {
        return;
    }

    // 자식 Transform이 남아있다면 부모 연결을 끊어 댕글링을 방지한다
    // (SetParent 호출이 m_children을 직접 수정하므로 반드시 복사본을 순회한다)
    if (m_transform)
    {
        std::vector<Transform*> children = m_transform->GetChildren();
        for (Transform* child : children)
        {
            child->SetParent(nullptr);
        }
        m_transform->SetParent(nullptr);
    }

    for (auto& component : m_components)
    {
        component->Destroy();
    }

    m_destroyed = true;
}
