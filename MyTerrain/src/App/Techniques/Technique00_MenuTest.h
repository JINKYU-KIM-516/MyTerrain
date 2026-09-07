#pragma once

class Scene;

// 0. 메뉴 구현 테스트 (Menu Test)
//
// 메뉴 화면의 이동/복귀 동작을 확인하기 위한 빈 기법 화면.
// 공통 UI(왼쪽 상단 "돌아가기" 버튼, ESC 키)는 ShowcaseApp이 이미 붙여주므로
// 여기서는 아무것도 구성하지 않는다.
void BuildMenuTestScene(Scene& scene);
