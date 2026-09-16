#pragma once

class Scene;

// 10. 무한 지형 청크 (Infinite Chunks)
//
// 카메라 주변의 청크만 메모리에 올려두고, 카메라가 움직이면 필요한 청크를 만들고
// 벗어난 청크를 버린다. 1~9번이 "고정된 한 장의 격자를 어떻게 덜 그릴까"였다면
// 10번은 "그 판 자체를 어떻게 늘렸다 줄일까"를 다룬다.
void BuildInfiniteChunksScene(Scene& scene);
