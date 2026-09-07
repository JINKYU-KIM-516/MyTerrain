#pragma once

class Scene;

// 2. 펄린 노이즈 지형 (Perlin Noise)
//
// 1번의 평면 그리드에 높이 함수만 끼워 넣은 화면.
// GridMesh::Generate 가 이미 높이 함수를 받도록 되어 있어서,
// 지형 쪽에서 새로 만든 것은 노이즈 모듈(Terrain/PerlinNoise)뿐이다.
//
//   [카메라] W/A/S/D 이동, E/Q 상하, 마우스 우클릭 드래그로 시점 회전,
//            휠로 이동 속도, Shift 가속, R 로 초기 위치 복귀
//   [노이즈] 위/아래로 항목 선택, 왼쪽/오른쪽으로 값 조절,
//            N 합성 방식, M 시드 무작위, 0 기본값 복귀
//   [그리드] + / - 로 분할 수, [ / ] 로 셀 크기, Tab 으로 표시 모드 전환
void BuildPerlinNoiseScene(Scene& scene);
