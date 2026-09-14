#pragma once

class Scene;

// 8. 스카이맵 (SkyDome, SkyBox)
//
// 3번(높이맵 지형) 위에 SkyDome(반구 + 절차적 셰이더)을 얹은 씬이다. 지형 쪽은
// HeightMapControlComponent 를 그대로 재사용해 3번과 조작이 완전히 같고, 스카이는
// 새로 만든 SkyRenderer/SkyControlComponent 로 완전히 독립적으로 얹는다 -- 이
// 기법이 있든 없든 1~7번은 TerrainRenderer/BasicTerrain.hlsl 을 전혀 건드리지
// 않으므로 영향받지 않는다.
//
//   [카메라]  이동 W/A/S/D   상하 E/Q   시점 마우스 우클릭 드래그   속도 휠   가속 Shift   리셋 R
//   [높이맵]  항목 선택 ↑/↓   값 조절 ←/→   다음 파일 N   고도 색상 C   다시 읽기 F5
//   [그리드]  분할 수 + / -   셀 크기 [ / ]   표시 모드 Tab
//   [스카이]  시간 되감기/감기 , / .   자동 재생 T   프리셋 순환 Y
//             태양 각크기 I/K   글로우 U/J   기본값(정오) 0                    [메뉴로] ESC
void BuildSkyScene(Scene& scene);
