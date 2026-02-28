#pragma once
#include "billboard_system.h"

namespace BillboardPresets {

    inline BillboardDef Tree(Texture2D tex, Vector2 size = {3.0f, 5.0f}) {
        BillboardDef d;
        d.texture         = tex;
        d.size            = size;
        d.lockY           = true;
        d.alphaThresh     = 0.15f;
        d.roughness       = 0.85f;
        d.windEnabled     = true;
        d.castsShadow     = true;
        d.shadowSize      = { size.x * 0.1f, size.y, size.x * 0.1f };
        return d;
    }

    inline BillboardDef Sphere(Texture2D tex, Vector2 size = {1.0f, 1.0f},
                               float speed = 0.0f, float rough = 1.0f) {
        BillboardDef d;
        d.texture         = tex;
        d.size            = size;
        d.lockY           = false;
        d.spherical       = true;
        d.sphereSpeed     = speed;
        d.alphaThresh     = 0.0f;        // disc clipping handled by shader
        d.roughness       = rough;
        d.metallic        = 0.0f;
        d.normalStrength  = 1.0f;
        d.windEnabled     = false;
        d.castsShadow     = true;
        d.shadowSize      = { size.x * 0.5f, size.y, size.x * 0.5f };
        return d;
    }

}