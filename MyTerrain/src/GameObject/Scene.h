#pragma once
#include "GameObject.h"
#include <vector>
#include <memory>
#include <string>

// 씬(Scene)에 존재하는 모든 GameObject를 관리한다.
// Framework의 게임 루프에서 Start -> Update -> Render 순으로 호출되며,
// MarkForDestroy() 된 GameObject는 매 프레임 끝에 정리된다.
class Scene
{
public:
    Scene() = default;

    GameObject* CreateGameObject(const std::string& name = "GameObject");

    // 다음 프레임 정리 시점에 파괴되도록 예약한다
    void DestroyGameObject(GameObject* gameObject);

    void Start();
    void Update(float deltaTime);
    void Render();

    // Update/Render 이후 매 프레임 호출. MarkForDestroy 된 오브젝트를 실제로 제거한다.
    void LateUpdate();

    // 씬에 존재하는 모든 GameObject 파괴 (씬 전환/프로그램 종료 시 호출)
    void Clear();

    GameObject* FindByName(const std::string& name) const;

    const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return m_gameObjects; }

private:
    std::vector<std::unique_ptr<GameObject>> m_gameObjects;

    // Start()가 이미 호출된 이후에 새로 생성된 GameObject에도 Start를 호출해주기 위한 큐
    std::vector<GameObject*> m_pendingStart;
    bool m_sceneStarted = false;
};
