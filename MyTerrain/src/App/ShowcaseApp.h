#pragma once
#include <functional>
#include <string>
#include <vector>

class Framework;
class Scene;

// 하나의 "기법"에 대한 정보.
// 메뉴에는 "n. 기법명 (영어 기법명)" 형식으로 표시되고,
// 클릭하면 build(scene)이 호출되어 해당 기법의 화면이 구성된다.
struct TechniqueEntry
{
    std::wstring number;                     // 메뉴에 붙일 번호. 비워두면 등록 순서(0부터)를 쓴다
    std::wstring nameKo;                     // 한글 기법명
    std::wstring nameEn;                     // 영어 기법명
    std::function<void(Scene&)> build;       // 기법 화면(씬) 구성 함수
};

// 터레인 기법 쇼케이스 전체를 총괄하는 클래스.
//
// - 기법 목록을 등록/보관한다 (RegisterTechnique)
// - 메뉴 화면 <-> 기법 화면 사이의 씬 전환을 처리한다
// - 씬 전환은 프레임 도중이 아니라 프레임이 끝난 뒤에 수행된다
//   (컴포넌트의 Update 안에서 자기 자신이 속한 씬을 지우면 위험하기 때문)
class ShowcaseApp
{
public:
    static ShowcaseApp& GetInstance();

    ShowcaseApp(const ShowcaseApp&) = delete;
    ShowcaseApp& operator=(const ShowcaseApp&) = delete;

    void Initialize(Framework* framework);

    // 기법 등록. 기본적으로 등록 순서대로 메뉴에 0번부터 번호가 매겨진다.
    //
    // number 를 넘기면 그 문자열이 번호 자리에 그대로 들어간다. 기법 목록의 번호와
    // 등록 순서가 어긋날 때 쓴다 (예: 거리 LOD 는 6-1 / 6-2 로 두 화면이지만
    // 등록은 두 번 하므로, 그냥 두면 6 / 7 로 표시된다).
    //
    // 주의: 한 번이라도 number 를 쓰면 그 뒤 기법들은 자동 번호가 한 칸씩 밀린다.
    //       이후 기법들도 number 를 직접 넘기는 편이 안전하다.
    void RegisterTechnique(const std::wstring& nameKo, const std::wstring& nameEn,
                           const std::function<void(Scene&)>& build,
                           const std::wstring& number = L"");

    const std::vector<TechniqueEntry>& GetTechniques() const { return m_techniques; }

    // 게임 루프 시작 전에 호출. 메뉴 화면을 즉시 구성한다.
    void Start();

    // ---- 화면 전환 요청 (실제 전환은 프레임 종료 시점에 수행) ----
    void RequestMenu();
    void RequestTechnique(int index);
    void RequestExit();

    // Framework의 프레임 종료 콜백에서 호출된다
    void ProcessPendingRequest();

    Framework* GetFramework() const { return m_framework; }

private:
    ShowcaseApp() = default;

    void BuildMenuScene(Scene& scene);
    void BuildTechniqueScene(Scene& scene, int index);

private:
    enum class Request
    {
        None,
        Menu,
        Technique,
        Exit
    };

    Framework* m_framework = nullptr;
    std::vector<TechniqueEntry> m_techniques;

    Request m_request = Request::None;
    int m_requestedTechnique = -1;
};
