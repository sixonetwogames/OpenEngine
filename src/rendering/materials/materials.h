#pragma once
#include "material_instance.h"
#include <string>
#include <unordered_map>
#include <stdexcept>

// ─── Preset material params ─────────────────────────────────────────────────
//                                          albedo                met   rough  useNorm normStr bright contrast worldUV tiling tileU tileV roughMap
namespace Materials {
    inline constexpr MaterialParams FloorTile { {100,140,180,255}, 0.0f, 0.70f, false,  1.0f,  1.0f, 1.0f, false,  1.0f, 1.0f, 1.0f, false };
    inline constexpr MaterialParams Tower     { {100,140,180,255}, 0.0f, 0.5f, true,   2.0f,  0.9f, 1.0f, false,  2.5f, 2.0f, 4.0f, false };
    inline constexpr MaterialParams Metal     { {180,180,190,255}, 0.9f, 0.30f, false,  1.0f,  1.0f, 1.0f, true,   1.0f, 1.0f, 1.0f, false };
    inline constexpr MaterialParams Concrete  { {160,155,150,255}, 0.0f, 0.85f, false,  1.0f,  1.0f, 1.0f, true,   1.0f, 1.0f, 1.0f, false };
    inline constexpr MaterialParams Grass     { {130,160, 90,255}, 0.0f, 0.40f, true,  2.0f,  1.00f, 1.0f, true,   0.15f, 1.0f, 1.0f, false };
}

// ─── Preset with optional texture paths ─────────────────────────────────────

struct MaterialPreset {
    MaterialParams params;
    std::string    diffuseTex;
    std::string    normalTex;
};

// ─── Runtime registry ───────────────────────────────────────────────────────

class MaterialRegistry {
    std::unordered_map<std::string, MaterialPreset> map;

    MaterialRegistry() {
        Register("FloorTile", { Materials::FloorTile });
        Register("Tower",     { Materials::Tower, "assets/textures/stone/stone_D.png",
                                                  "assets/textures/stone/stone_N.png" });
        Register("Metal",     { Materials::Metal });
        Register("Concrete",  { Materials::Concrete });
        Register("Grass", { Materials::Grass, "assets/textures/landscape/grass_D.png",
                                              "assets/textures/landscape/grass_N.png" });
    }


public:
    static MaterialRegistry& Get() {
        static MaterialRegistry inst;
        return inst;
    }

    void Register(const std::string& name, const MaterialPreset& p) { map[name] = p; }

    const MaterialPreset& Lookup(const std::string& name) const {
        auto it = map.find(name);
        if (it == map.end())
            throw std::runtime_error("Unknown material: " + name);
        return it->second;
    }

    bool Has(const std::string& name) const { return map.count(name); }
};