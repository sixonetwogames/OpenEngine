#include "level.h"
#include "resource_manager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;
using namespace Materials;

// ── JSON parsing helpers ────────────────────────────────────────────────────

static Color ParseColor(const json& j) {
    return { (unsigned char)j[0], (unsigned char)j[1],
             (unsigned char)j[2], (unsigned char)j[3] };
}

static Vector3 ParseVec3(const json& j) {
    return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>() };
}

// Resolve named preset, then apply any per-field JSON overrides on top.
static LevelDef::MatDef ParseMat(const json& j) {
    LevelDef::MatDef m;

    // Start from preset if "name" is present
    if (j.contains("name")) {
        const auto& preset = MaterialRegistry::Get().Lookup(j["name"].get<std::string>());
        const auto& base   = preset.params;
        m.preset         = j["name"];
        m.albedo         = base.albedo;
        m.metallic       = base.metallic;
        m.roughness      = base.roughness;
        m.useNormalMap    = base.useNormalMap;
        m.normalStrength  = base.normalStrength;
        m.brightness      = base.brightness;
        m.contrast        = base.contrast;
        m.useWorldUVs     = base.useWorldUVs;
        m.tiling          = base.tiling;
        m.tileU           = base.tileU;
        m.tileV           = base.tileV;
        m.useRoughnessMap = base.useRoughnessMap;
        m.diffuseTex      = preset.diffuseTex;
        m.normalTex       = preset.normalTex;
    }

    // Override any explicitly-specified fields
    if (j.contains("albedo"))         m.albedo         = ParseColor(j["albedo"]);
    if (j.contains("metallic"))       m.metallic       = j["metallic"];
    if (j.contains("roughness"))      m.roughness      = j["roughness"];
    if (j.contains("useNormalMap"))    m.useNormalMap    = j["useNormalMap"];
    if (j.contains("normalStrength")) m.normalStrength  = j["normalStrength"];
    if (j.contains("brightness"))     m.brightness      = j["brightness"];
    if (j.contains("contrast"))       m.contrast        = j["contrast"];
    if (j.contains("useWorldUVs"))    m.useWorldUVs     = j["useWorldUVs"];
    if (j.contains("tiling"))         m.tiling          = j["tiling"];
    if (j.contains("tileU"))          m.tileU           = j["tileU"];
    if (j.contains("tileV"))          m.tileV           = j["tileV"];
    if (j.contains("useRoughnessMap"))m.useRoughnessMap = j["useRoughnessMap"];

    if (j.contains("textures")) {
        m.diffuseTex = j["textures"].value("diffuse", "");
        m.normalTex  = j["textures"].value("normal",  "");
    }
    return m;
}

LevelDef LevelDef::LoadFromFile(const char* path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error(std::string("Cannot open level: ") + path);
    json root = json::parse(f);

    LevelDef def;
    def.shader.vertex   = root["shader"]["vertex"];
    def.shader.fragment = root["shader"]["fragment"];
    def.fog.near        = root["fog"].value("near", 0.1f);
    def.fog.far         = root["fog"].value("far",  50.0f);

    auto& fl           = root["floor"];
    def.floor.width    = fl.value("width", 250.0f);
    def.floor.depth    = fl.value("depth", 250.0f);
    def.floor.material = ParseMat(fl["material"]);

    for (auto& g : root["geometry"]) {
        LevelDef::Geometry geo;
        geo.type      = g.value("type", "cube");
        geo.position  = ParseVec3(g["position"]);
        geo.size      = ParseVec3(g["size"]);
        geo.collision = g.value("collision", true);
        if (g.contains("wireColor")) geo.wireColor = ParseColor(g["wireColor"]);
        geo.material  = ParseMat(g["material"]);
        def.geometry.push_back(std::move(geo));
    }
    return def;
}

// ── Model helpers ───────────────────────────────────────────────────────────

Model Level::MakePBRPlane(float width, float depth) {
    Mesh mesh = GenMeshPlane(width, depth, 1, 1);
    return LoadModelFromMesh(mesh);
}

Model Level::MakePBRCube(const Vector3& size) {
    Mesh mesh = GenMeshCube(size.x, size.y, size.z);
    return LoadModelFromMesh(mesh);
}

// Now copies ALL MaterialParams fields
MaterialParams Level::BuildParams(const LevelDef::MatDef& m) {
    return {
        .albedo          = m.albedo,
        .metallic        = m.metallic,
        .roughness       = m.roughness,
        .useNormalMap     = m.useNormalMap,
        .normalStrength  = m.normalStrength,
        .brightness      = m.brightness,
        .contrast        = m.contrast,
        .useWorldUVs     = m.useWorldUVs,
        .tiling          = m.tiling,
        .tileU           = m.tileU,
        .tileV           = m.tileV,
        .useRoughnessMap = m.useRoughnessMap,
    };
}

// ── Load / Unload ───────────────────────────────────────────────────────────

void Level::Load(CollisionSystem& collision,
                 const char* levelDefPath,
                 const char* mapImagePath,
                 Vector3 mapOffset) {
    offset = mapOffset;

    if (mapImagePath) {
        mapImage = LoadImage(mapImagePath);
        hasMap   = true;
    }

    LevelDef def = LevelDef::LoadFromFile(levelDefPath);

    pbr.Init(def.shader.vertex.c_str(), def.shader.fragment.c_str());

    auto& rm   = ResourceManager::Get();
    auto& fmat = def.floor.material;
    if (!fmat.diffuseTex.empty())
        floorDiffuse = rm.LoadTex("floor_d", fmat.diffuseTex.c_str());
    if (!fmat.normalTex.empty())
        floorNormal  = rm.LoadTex("floor_n", fmat.normalTex.c_str());

    pbr.SetTexture(TexSlot::Albedo, floorDiffuse);
    pbr.SetTexture(TexSlot::Normal, floorNormal);

    shadows.Init();

    floorModel  = MakePBRPlane(def.floor.width, def.floor.depth);
    floorParams = BuildParams(fmat);
    pbr.Apply(floorModel);

    SpawnGeometry(collision, def);
}

void Level::Unload() {
    for (auto& c : cubes) UnloadModel(c.model);
    cubes.clear();
    cachedCasters.clear();
    UnloadModel(floorModel);
    shadows.Unload();
    pbr.Unload();
    if (hasMap) { UnloadImage(mapImage); hasMap = false; }
}

// ── Geometry spawning ───────────────────────────────────────────────────────

void Level::SpawnGeometry(CollisionSystem& collision, const LevelDef& def) {
    cubes.reserve(def.geometry.size());
    auto& rm = ResourceManager::Get();

    for (const auto& geo : def.geometry) {
        if (geo.collision) {
            collision.SpawnCube({
                .position     = geo.position,
                .size         = geo.size,
                .color        = BLANK,
                .wireColor    = geo.wireColor,
                .hasCollision = true,
            });
        }

        Model mdl = MakePBRCube(geo.size);
        pbr.Apply(mdl);

        // Per-cube textures (from preset or JSON override)
        if (!geo.material.diffuseTex.empty()) {
            Texture2D tex = rm.LoadTex(geo.material.diffuseTex.c_str(), geo.material.diffuseTex.c_str());
            mdl.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = tex;
        }
        if (!geo.material.normalTex.empty()) {
            Texture2D tex = rm.LoadTex(geo.material.normalTex.c_str(), geo.material.normalTex.c_str());
            mdl.materials[0].maps[MATERIAL_MAP_NORMAL].texture = tex;
        }

        cubes.push_back({ geo.position, geo.size, mdl, BuildParams(geo.material) });
    }
    castersDirty = true;
}

void Level::RebuildCasters() {
    cachedCasters.clear();
    cachedCasters.reserve(cubes.size());
    for (const auto& c : cubes)
        cachedCasters.push_back({ c.position, c.size });
    castersDirty = false;
}

// ── Shader / Lighting ───────────────────────────────────────────────────────

void Level::ReapplyShader() {
    pbr.Apply(floorModel);
    for (auto& c : cubes) pbr.Apply(c.model);
}

void Level::UpdateLighting(Vector3 camPos) {
    if (pbr.CheckReload()) ReapplyShader();
    pbr.SetFogPlanes(World::fogNear, World::fogFar);
    shadows.CheckReload();

    shadows.SetSunDir(World::lightDir);
    pbr.SetSun(World::lightDir, World::lightColor, World::lightIntensity);
    pbr.SetSkylight(World::skylightColor, World::skylightIntensity);
    pbr.SetCamera(camPos);
}

// ── Drawing ─────────────────────────────────────────────────────────────────

void Level::Draw() const {
    pbr.Bind(floorParams);
    DrawModel(floorModel, {0, 0, 0}, 1.0f, WHITE);

    for (const auto& c : cubes) {
        pbr.Bind(c.pbr);
        DrawModel(c.model, c.position, 1.0f, WHITE);
    }

    // Use cached casters — only rebuilt when geometry changes
    if (castersDirty) const_cast<Level*>(this)->RebuildCasters();
    shadows.Draw(cachedCasters);
}