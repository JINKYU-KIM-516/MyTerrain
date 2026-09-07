#pragma once

class Scene;

// 3. 높이맵 지형 (Heightmap Terrain)
//
// 2번은 높이를 실행 중에 "계산"했고, 3번은 미리 그려둔 이미지에서 "읽어온다".
// GridMesh::Generate 에 넘기는 높이 함수의 알맹이만 바뀐 것이라
// 지형 쪽에서 새로 만든 것은 이미지 로더와 샘플러(Terrain/HeightMap)뿐이다.
//
// 높이맵 파일은 MyTerrain/heightmaps/ 폴더에 둔다.
//   - PNG / BMP / TIFF : Windows 내장 WIC 로 읽는다. 16비트 그레이스케일 PNG 권장
//   - RAW (.raw / .r16) : 헤더가 없는 원시 배열. 정사각형으로 보고 크기를 역산한다
//   - JPG 는 읽히기는 하지만 손실 압축 아티팩트가 법선에 그대로 드러나므로 권하지 않는다
//
//   [카메라] W/A/S/D 이동, E/Q 상하, 마우스 우클릭 드래그로 시점 회전,
//            휠로 이동 속도, Shift 가속, R 로 초기 위치 복귀
//   [높이맵] 위/아래로 항목 선택, 왼쪽/오른쪽으로 값 조절,
//            N 다음 파일, C 고도 색상, F5 다시 읽기, 0 기본값 복귀
//   [그리드] + / - 로 분할 수, [ / ] 로 셀 크기, Tab 으로 표시 모드 전환
void BuildHeightMapScene(Scene& scene);
