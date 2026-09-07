#include "Scene.h"
#include <algorithm>

GameObject* Scene::CreateGameObject(const std::string& name)
{
    auto gameObject = std::make_unique<GameObject>(name);
    GameObject* rawPtr = gameObject.get();

    m_gameObjects.push_back(std::move(gameObject));

    if (m_sceneStarted)
    {
        // 이미 게임 루프가 돌고 있는 도중에 생성된 경우, 다음 Update 전에 Start를 호출해준다
        m_pendingStart.push_back(rawPtr);
    }

    return rawPtr;
}

void Scene::DestroyGameObject(GameObject* gameObject)
{
    if (gameObject)
    {
        gameObject->MarkForDestroy();
    }
}

void Scene::Start()
{
    for (auto& go : m_gameObjects)
    {
        go->Start();
    }
    m_sceneStarted = true;
}

void Scene::Update(float deltaTime)
{
    // 이번 프레임에 새로 추가된 GameObject들의 Start를 먼저 호출
    for (GameObject* go : m_pendingStart)
    {
        go->Start();
    }
    m_pendingStart.clear();

    for (auto& go : m_gameObjects)
    {
        if (!go->IsPendingDestroy())
        {
            go->Update(deltaTime);
        }
    }
}

void Scene::Render()
{
    for (auto& go : m_gameObjects)
    {
        if (!go->IsPendingDestroy())
        {
            go->Render();
        }
    }
}

void Scene::LateUpdate()
{
    auto it = std::remove_if(m_gameObjects.begin(), m_gameObjects.end(),
        [](const std::unique_ptr<GameObject>& go)
        {
            return go->IsPendingDestroy();
        });

    // 제거되기 전 Destroy() 콜백을 호출해준다
    for (auto iter = it; iter != m_gameObjects.end(); ++iter)
    {
        (*iter)->Destroy();
    }

    m_gameObjects.erase(it, m_gameObjects.end());
}

void Scene::Clear()
{
    for (auto& go : m_gameObjects)
    {
        go->Destroy();
    }
    m_gameObjects.clear();
    m_pendingStart.clear();
    m_sceneStarted = false;
}

GameObject* Scene::FindByName(const std::string& name) const
{
    for (const auto& go : m_gameObjects)
    {
        if (go->GetName() == name)
        {
            return go.get();
        }
    }
    return nullptr;
}
