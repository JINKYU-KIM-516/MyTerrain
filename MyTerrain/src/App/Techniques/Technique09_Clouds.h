#pragma once

class Scene;

// 9. 동적 왜곡 구름 (Perturbed Clouds)
//
// 8번(스카이맵) 위에 절차적 구름 레이어를 하나 더 얹은 씬이다. 지형/스카이 구성은
// 8번과 완전히 같고(HeightMapControlComponent, SkyRenderer/SkyControlComponent 를
// 그대로 재사용), 구름은 새로 만든 CloudRenderer/CloudControlComponent 로 완전히
// 독립적으로 얹는다 -- 이 기법이 있든 없든 1~8번은 TerrainRenderer/Sky.hlsl 등을
// 전혀 건드리지 않으므로 영향받지 않는다.
//
//   [카메라]  이동 W/A/S/D   상하 E/Q   시점 마우스 우클릭 드래그   속도 휠   가속 Shift   리셋 R
//   [높이맵]  항목 선택 ↑/↓   값 조절 ←/→   다음 파일 N   고도 색상 C   다시 읽기 F5
//   [그리드]  분할 수 + / -   셀 크기 [ / ]   표시 모드 Tab
//   [스카이]  시간 되감기/감기 , / .   자동 재생 T   프리셋 순환 Y   태양 각크기 I/K   글로우 U/J
//   [구름]    표시 켬/끄기 L   커버리지 G/H   워프 세기 V/B   바람 속도 O/P
//             기본값(모두) 0                                                       [메뉴로] ESC
void BuildCloudScene(Scene& scene);
