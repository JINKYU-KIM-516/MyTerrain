#pragma once

class Scene;

// 1. 기본 평면 그리드 (Basic Flat Grid)
//
// 앞으로 만들 모든 지형 기법의 출발점이 되는 화면.
// 높이가 전부 0인 NxN 격자 메시를 만들어 자유 비행 카메라로 둘러본다.
//
//   [카메라] W/A/S/D 이동, E/Q 상하, 마우스 우클릭 드래그로 시점 회전,
//            휠로 이동 속도, Shift 가속, R 로 초기 위치 복귀
//   [그리드] + / - 로 분할 수, [ / ] 로 셀 크기, Tab 으로 표시 모드 전환
void BuildFlatGridScene(Scene& scene);
