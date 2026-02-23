#pragma once
#include "types.h"

class Renderer {
public:
    void Init(int width, int height, const char* title);
    void Shutdown();
    void BeginFrame();
    void EndFrame();
    void DrawDebugInfo(const Body& player);
};
