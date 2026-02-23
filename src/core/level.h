#pragma once
#include "raylib.h"
#include "collision.h"
#include "material_instance.h"
#include "shadow_system.h"
#include "rendering/materials/materials.h"
#include "world.h"
#include <string>
#include <vector>

// ── Parsed level definition ─────────────────────────────────────────────────

struct LevelDef {
    struct Shader { std::string vertex, fragment; };
    struct Fog    { float near = 0.1f, far = 50.0f; };

    struct MatDef {
        Color       albedo      = WHITE;
        float       metallic    = 0.0f;
        float       roughness   = 1.0f;
        float       ambient     = 0.15f;
        bool        useNormalMap = true;
        std::string diffuseTex;
        std::string normalTex;
    };

    struct Floor {
        float  width = 250.0f;
        float  depth = 250.0f;
        MatDef material;
    };

    struct Geometry {
        std::string type;
        Vector3     position;
        Vector3     size;
        bool        collision = true;
        Color       wireColor = DARKBLUE;
        MatDef      material;
    };

    Shader                shader;
    Fog                   fog;
    Floor                 floor;
    std::vector<Geometry> geometry;

    static LevelDef LoadFromFile(const char* path);
};

// ── Runtime cube instance ───────────────────────────────────────────────────

struct LevelCube {
    Vector3        position;
    Vector3        size;
    Model          model;
    MaterialParams pbr;
};

// ── Level framework ─────────────────────────────────────────────────────────

class Level {
public:
    void Load(CollisionSystem& collision,
              const char* levelDefPath,
              const char* mapImagePath = nullptr,
              Vector3 mapOffset        = {0, 0, 0});
    void Unload();

    void UpdateLighting(const LightingState& light, Vector3 camPos);
    void Draw() const;

    MaterialInstance& GetPBR()     { return pbr; }
    ShadowSystem&     GetShadows() { return shadows; }

private:
    MaterialInstance pbr;
    ShadowSystem     shadows;

    Model          floorModel{};
    MaterialParams floorParams{};

    std::vector<LevelCube> cubes;

    Image   mapImage{};
    bool    hasMap = false;
    Vector3 offset{};

    void SpawnGeometry(CollisionSystem& collision, const LevelDef& def);
    void ReapplyShader();

    static Model MakePBRCube(const Vector3& size);
    static Model MakePBRPlane(float width, float depth);

    static MaterialParams BuildParams(const LevelDef::MatDef& m);

    Texture2D grassDiffuse{};
    Texture2D grassNormal{};
};