#include "level.h"
#include "resource_manager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

// ── JSON parsing helpers ────────────────────────────────────────────────────

static Color ParseColor(const json& j) {
    return { (unsigned char)j[0], (unsigned char)j[1],
             (unsigned char)j[2], (unsigned char)j[3] };
}

static Vector3 ParseVec3(const json& j) {
    return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>() };
}

static LevelDef::MatDef ParseMat(const json& j) {
    LevelDef::MatDef m;
    m.albedo      = ParseColor(j["albedo"]);
    m.metallic    = j.value("metallic",    0.0f);
    m.roughness   = j.value("roughness",   1.0f);
    m.ambient     = j.value("ambient",     0.15f);
    m.useNormalMap = j.value("useNormalMap", true);
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

    auto& fl       = root["floor"];
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
    GenMeshTangents(&mesh);
    return LoadModelFromMesh(mesh);
}

Model Level::MakePBRCube(const Vector3& size) {
    Mesh mesh = GenMeshCube(size.x, size.y, size.z);
    GenMeshTangents(&mesh);
    return LoadModelFromMesh(mesh);
}

MaterialParams Level::BuildParams(const LevelDef::MatDef& m) {
    return {
        .albedo      = m.albedo,
        .metallic    = m.metallic,
        .roughness   = m.roughness,
        .ambient     = m.ambient,
        .useNormalMap = m.useNormalMap,
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
    pbr.SetFogPlanes(def.fog.near, def.fog.far);

    auto& rm   = ResourceManager::Get();
    auto& fmat = def.floor.material;
    if (!fmat.diffuseTex.empty())
        grassDiffuse = rm.LoadTex("grass_d", fmat.diffuseTex.c_str());
    if (!fmat.normalTex.empty())
        grassNormal  = rm.LoadTex("grass_n", fmat.normalTex.c_str());

    pbr.SetTexture(TexSlot::Albedo, grassDiffuse);
    pbr.SetTexture(TexSlot::Normal, grassNormal);

    shadows.Init();

    floorModel  = MakePBRPlane(def.floor.width, def.floor.depth);
    floorParams = BuildParams(fmat);
    pbr.Apply(floorModel);

    SpawnGeometry(collision, def);
}

void Level::Unload() {
    for (auto& c : cubes) UnloadModel(c.model);
    cubes.clear();
    UnloadModel(floorModel);
    shadows.Unload();
    pbr.Unload();
    if (hasMap) { UnloadImage(mapImage); hasMap = false; }
}

// ── Geometry spawning ───────────────────────────────────────────────────────

void Level::SpawnGeometry(CollisionSystem& collision, const LevelDef& def) {
    cubes.reserve(def.geometry.size());

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
        cubes.push_back({ geo.position, geo.size, mdl, BuildParams(geo.material) });
    }
}

// ── Shader / Lighting ───────────────────────────────────────────────────────

void Level::ReapplyShader() {
    pbr.Apply(floorModel);
    for (auto& c : cubes) pbr.Apply(c.model);
}

void Level::UpdateLighting(const LightingState& light, Vector3 camPos) {
    if (pbr.CheckReload()) ReapplyShader();
    pbr.SetFogPlanes(0.1f, 50.0f);
    shadows.CheckReload();

    shadows.SetSunDir(light.lightDir);
    pbr.SetSun(light.lightDir, light.lightColor, light.lightIntensity);
    pbr.SetSkylight(light.skylightColor, light.skylightIntensity);
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

    std::vector<ShadowCaster> casters;
    casters.reserve(cubes.size());
    for (const auto& c : cubes)
        casters.push_back({ c.position, c.size });
    shadows.Draw(casters);
}