#pragma once
#include "material_instance.h"

namespace Materials {
    //                                     albedo                met   rough  amb   useNorm normStr bright contrast worldUV tiling roughMap
    inline constexpr MaterialParams FloorTile { {100,140,180,255}, 0.0f, 0.70f, 0.12f, true,  1.0f,  1.0f, 1.0f, true,  1.0f, false };
    inline constexpr MaterialParams Tower     { {100,140,180,255}, 0.0f, 0.55f, 0.10f, false, 1.0f,  1.0f, 1.0f, false, 1.0f, false };
    inline constexpr MaterialParams Metal     { {180,180,190,255}, 0.9f, 0.30f, 0.08f, true,  1.0f,  1.0f, 1.0f, true,  1.0f, false };
    inline constexpr MaterialParams Concrete  { {160,155,150,255}, 0.0f, 0.85f, 0.15f, true,  1.0f,  1.0f, 1.0f, true,  1.0f, false };
    inline constexpr MaterialParams Grass     { {130,160, 90,255}, 0.0f, 0.50f, 0.12f, true,  1.0f,  1.0f, 1.0f, true,  2.0f, false };
}