#pragma once
#include "raylib.h"
#include "collision.h"
#include "material_instance.h"
#include "shadow_system.h"
#include "billboard_system.h"
#include "rendering/materials/materials.h"
#include "world.h"
#include <string>
#include <vector>

// ── Parsed level definition ─────────────────────────────────────────────────

struct LevelDef {
    struct Shader { std::string vertex, fragment; };
    struct Fog    { float near = 0.1f, far = 50.0f; };

    struct MatDef {
        std::string preset;
        Color       albedo      = WHITE;
        float       metallic    = 0.0f;
        float       roughness   = 1.0f;
        bool        useNormalMap = true;
        float       normalStrength = 1.0f;
        float       brightness     = 1.0f;
        float       contrast       = 1.0f;
        bool        useWorldUVs    = true;
        float       tiling         = 1.0f;
        float       tileU          = 1.0f;
        float       tileV          = 1.0f;
        bool        useRoughnessMap = false;
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

    // ── Billboard entries ────────────────────────────────────────────────
    struct BillboardEntry {
        std::string defName;           // registry key (e.g. "tree1")
        std::string texture;           // path to diffuse
        Vector2     size{3.0f, 5.0f};
        bool        lockY = true;
        float       alphaThresh    = 0.1f;
        float       roughness      = 0.8f;
        float       metallic       = 0.0f;
        float       normalStrength = 0.0f;
        bool        spherical      = false;
        float       sphereSpeed    = 0.4f;
        bool        collision      = false;
        SpriteSheet sheet;
        // Spawn positions
        std::vector<Vector3> positions;
        std::vector<float>   scales;    // parallel to positions, default 1.0
    };

    Shader                      shader;
    Fog                         fog;
    Floor                       floor;
    std::vector<Geometry>       geometry;
    std::vector<BillboardEntry> billboards;

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

    void UpdateLighting(Vector3 camPos);
    void Draw();
    void DrawBillboards(Camera camera);

    MaterialInstance& GetPBR()        { return pbr; }
    ShadowSystem&     GetShadows()    { return shadows; }
    BillboardSystem&  GetBillboards() { return billboards; }

private:
    MaterialInstance pbr;
    ShadowSystem     shadows;
    BillboardSystem  billboards;

    Model          floorModel{};
    MaterialParams floorParams{};

    std::vector<LevelCube>    cubes;
    std::vector<ShadowCaster> cachedCasters;
    std::vector<ShadowCaster> meshCasters;
    bool                      castersDirty = true;

    Image   mapImage{};
    bool    hasMap = false;
    Vector3 offset{};

    Texture2D floorDiffuse{};
    Texture2D floorNormal{};

    void SpawnGeometry(CollisionSystem& collision, const LevelDef& def);
    void SpawnBillboards(CollisionSystem& collision, const LevelDef& def);
    void ReapplyShader();
    void RebuildCasters();

    static Model MakePBRCube(const Vector3& size);
    static Model MakePBRPlane(float width, float depth);
    static MaterialParams BuildParams(const LevelDef::MatDef& m);
};