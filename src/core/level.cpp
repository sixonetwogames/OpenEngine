#include "level.h"
#include "resource_manager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include "shader_utils.h"

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

static Vector2 ParseVec2(const json& j) {
    return { j[0].get<float>(), j[1].get<float>() };
}

// Resolve named preset, then apply any per-field JSON overrides on top.
static LevelDef::MatDef ParseMat(const json& j) {
    LevelDef::MatDef m;

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

// ── Billboard JSON parsing ──────────────────────────────────────────────────

static LevelDef::BillboardEntry ParseBillboard(const json& j) {
    LevelDef::BillboardEntry b;
    b.defName        = j.value("name", "");
    b.texture        = j.value("texture", "");
    b.lockY          = j.value("lockY", true);
    b.alphaThresh    = j.value("alphaThreshold", 0.1f);
    b.roughness      = j.value("roughness", 0.8f);
    b.metallic       = j.value("metallic", 0.0f);
    b.normalStrength = j.value("normalStrength", 0.0f);

    if (j.contains("size")) b.size = ParseVec2(j["size"]);

    // Sprite sheet (optional)
    if (j.contains("sheet")) {
        auto& sh = j["sheet"];
        b.sheet.cols        = sh.value("cols", 1);
        b.sheet.rows        = sh.value("rows", 1);
        b.sheet.totalFrames = sh.value("totalFrames", b.sheet.cols * b.sheet.rows);
        b.sheet.fps         = sh.value("fps", 0.0f);
        b.sheet.loop        = sh.value("loop", true);
    }

    // Instances: array of positions (or objects with position + scale)
    if (j.contains("instances")) {
        for (auto& inst : j["instances"]) {
            if (inst.is_array()) {
                b.positions.push_back(ParseVec3(inst));
                b.scales.push_back(1.0f);
            } else {
                b.positions.push_back(ParseVec3(inst["position"]));
                b.scales.push_back(inst.value("scale", 1.0f));
            }
        }
    }

    return b;
}

// ── LevelDef loading ────────────────────────────────────────────────────────

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

    if (root.contains("billboards")) {
        for (auto& b : root["billboards"])
            def.billboards.push_back(ParseBillboard(b));
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

    pbr.InitPlatform("pbr");

    auto& rm   = ResourceManager::Get();
    auto& fmat = def.floor.material;
    if (!fmat.diffuseTex.empty())
        floorDiffuse = rm.LoadTex("floor_d", fmat.diffuseTex.c_str());
    if (!fmat.normalTex.empty())
        floorNormal  = rm.LoadTex("floor_n", fmat.normalTex.c_str());

    pbr.SetTexture(TexSlot::Albedo, floorDiffuse);
    pbr.SetTexture(TexSlot::Normal, floorNormal);


    
    shadows.Init();
    billboards.Init();

    floorModel  = MakePBRPlane(def.floor.width, def.floor.depth);
    floorParams = BuildParams(fmat);
    pbr.Apply(floorModel);

    SpawnGeometry(collision, def);
    SpawnBillboards(def);
}

void Level::Unload() {
    for (auto& c : cubes) UnloadModel(c.model);
    cubes.clear();
    cachedCasters.clear();
    UnloadModel(floorModel);
    billboards.Unload();
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

// ── Billboard spawning from level def ───────────────────────────────────────

void Level::SpawnBillboards(const LevelDef& def) {
    TraceLog(LOG_INFO, "BILLBOARD: %d billboard defs in level", (int)def.billboards.size());

    for (const auto& entry : def.billboards) {
        if (!billboards.HasDef(entry.defName)) {
            BillboardDef bd;
            bd.texture        = LoadTexture(entry.texture.c_str());
            bd.size           = entry.size;
            bd.lockY          = entry.lockY;
            bd.alphaThresh    = entry.alphaThresh;
            bd.roughness      = entry.roughness;
            bd.metallic       = entry.metallic;
            bd.normalStrength = entry.normalStrength;
            bd.sheet          = entry.sheet;

            TraceLog(LOG_INFO, "BILLBOARD: Registered '%s' tex=%d (%dx%d) size=%.1fx%.1f",
                     entry.defName.c_str(), bd.texture.id,
                     bd.texture.width, bd.texture.height,
                     bd.size.x, bd.size.y);

            if (bd.texture.id == 0)
                TraceLog(LOG_WARNING, "BILLBOARD: Texture FAILED to load: %s", entry.texture.c_str());

            billboards.RegisterDef(entry.defName, bd);
        }

        uint16_t idx = billboards.LookupDef(entry.defName);
        for (size_t i = 0; i < entry.positions.size(); i++) {
            float scale = (i < entry.scales.size()) ? entry.scales[i] : 1.0f;
            billboards.Spawn({ idx, entry.positions[i], scale });
        }

        TraceLog(LOG_INFO, "BILLBOARD: Spawned %d instances of '%s' (total: %d)",
                 (int)entry.positions.size(), entry.defName.c_str(),
                 (int)billboards.InstanceCount());
    }
}

void Level::RebuildCasters() {
    meshCasters.clear();
    meshCasters.reserve(cubes.size());
    for (const auto& c : cubes)
        meshCasters.push_back({ c.position, c.size });
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
    billboards.CheckReload();

    shadows.SetSunDir(World::lightDir);
    pbr.SetSun(World::lightDir, World::lightColor, World::lightIntensity);
    pbr.SetSkylight(World::skylightColor, World::skylightIntensity);
    pbr.SetCamera(camPos);

    billboards.Update(GetFrameTime());
}

// ── Drawing ─────────────────────────────────────────────────────────────────

void Level::Draw(){
    pbr.Bind(floorParams);
    DrawModel(floorModel, {0, 0, 0}, 1.0f, WHITE);

    for (const auto& c : cubes) {
        pbr.Bind(c.pbr);
        DrawModel(c.model, c.position, 1.0f, WHITE);
    }
    cachedCasters.clear();
    if (castersDirty) RebuildCasters();
    cachedCasters.insert(cachedCasters.end(), meshCasters.begin(), meshCasters.end());
    billboards.GatherShadowCasters(cachedCasters);
    shadows.Draw(cachedCasters);
}

void Level::DrawBillboards(Camera camera) {
    billboards.Draw(camera);
}