#pragma once
#include "raylib.h"
#include "raymath.h"
#include <cstdint>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <bitset>
#include <cassert>
#include <functional>
#include <algorithm>
#include <typeindex>
#include <memory>

// ─── Entity ID ──────────────────────────────────────────────────────────────
// Pack generation (upper 8 bits) + index (lower 24 bits) into one uint32.
// Generation prevents stale-ID bugs when entities are recycled.

using EntityID = uint32_t;
constexpr EntityID NULL_ENTITY = 0;

inline uint32_t EntIndex(EntityID id)      { return id & 0x00FFFFFF; }
inline uint8_t  EntGeneration(EntityID id)  { return (id >> 24) & 0xFF; }
inline EntityID MakeEntityID(uint32_t idx, uint8_t gen) {
    return (static_cast<uint32_t>(gen) << 24) | (idx & 0x00FFFFFF);
}

// ─── Max components (bitset width) ──────────────────────────────────────────
constexpr size_t MAX_COMPONENTS = 64;
using ComponentMask = std::bitset<MAX_COMPONENTS>;

// ─── Component type registry ────────────────────────────────────────────────
// Assigns a unique sequential ID to each component type at first use.

class ComponentRegistry {
    inline static size_t nextID = 0;
    inline static std::unordered_map<std::type_index, size_t> typeMap;
public:
    template<typename T>
    static size_t GetID() {
        auto key = std::type_index(typeid(T));
        auto it = typeMap.find(key);
        if (it != typeMap.end()) return it->second;
        size_t id = nextID++;
        assert(id < MAX_COMPONENTS && "Component limit exceeded");
        typeMap[key] = id;
        return id;
    }
};

// ─── Sparse-set component pool ──────────────────────────────────────────────
// O(1) add/remove/get, cache-friendly iteration over dense array.

class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual void Remove(uint32_t index) = 0;
    virtual bool Has(uint32_t index) const = 0;
    virtual size_t Size() const = 0;
};

template<typename T>
class ComponentPool : public IComponentPool {
    std::vector<T>        dense;       // packed component data
    std::vector<uint32_t> denseToEntity; // dense idx -> entity index
    std::unordered_map<uint32_t, size_t> sparse; // entity index -> dense idx

public:
    T& Add(uint32_t entityIdx, const T& component = T{}) {
        assert(!Has(entityIdx));
        size_t denseIdx = dense.size();
        dense.push_back(component);
        denseToEntity.push_back(entityIdx);
        sparse[entityIdx] = denseIdx;
        return dense[denseIdx];
    }

    void Remove(uint32_t entityIdx) override {
        auto it = sparse.find(entityIdx);
        if (it == sparse.end()) return;

        size_t denseIdx = it->second;
        size_t lastIdx  = dense.size() - 1;

        if (denseIdx != lastIdx) {
            dense[denseIdx]         = std::move(dense[lastIdx]);
            denseToEntity[denseIdx] = denseToEntity[lastIdx];
            sparse[denseToEntity[denseIdx]] = denseIdx;
        }

        dense.pop_back();
        denseToEntity.pop_back();
        sparse.erase(it);
    }

    bool Has(uint32_t entityIdx) const override { return sparse.count(entityIdx) > 0; }
    size_t Size() const override { return dense.size(); }

    T& Get(uint32_t entityIdx) {
        assert(Has(entityIdx));
        return dense[sparse.at(entityIdx)];
    }

    const T& Get(uint32_t entityIdx) const {
        assert(Has(entityIdx));
        return dense[sparse.at(entityIdx)];
    }

    // Iteration over all components of this type
    T*       begin()       { return dense.data(); }
    T*       end()         { return dense.data() + dense.size(); }
    const T* begin() const { return dense.data(); }
    const T* end()   const { return dense.data() + dense.size(); }

    // Get the entity index for a dense-array position (useful during iteration)
    uint32_t GetEntityIndex(size_t denseIdx) const { return denseToEntity[denseIdx]; }
};

// ─── Entity Manager ─────────────────────────────────────────────────────────

class EntityManager {
    struct EntityRecord {
        uint8_t       generation = 0;
        bool          alive      = false;
        ComponentMask mask;
    };

    std::vector<EntityRecord>                      entities;
    std::vector<uint32_t>                          freeList;
    std::unordered_map<size_t, std::unique_ptr<IComponentPool>> pools;
    uint32_t                                       entityCount = 0;

    template<typename T>
    ComponentPool<T>& GetPool() {
        size_t id = ComponentRegistry::GetID<T>();
        auto it = pools.find(id);
        if (it == pools.end()) {
            pools[id] = std::make_unique<ComponentPool<T>>();
            it = pools.find(id);
        }
        return *static_cast<ComponentPool<T>*>(it->second.get());
    }

public:
    // ── Lifecycle ────────────────────────────────────────────────────────

    EntityID Create() {
        uint32_t idx;
        if (!freeList.empty()) {
            idx = freeList.back();
            freeList.pop_back();
            entities[idx].generation++;
            entities[idx].alive = true;
            entities[idx].mask.reset();
        } else {
            idx = static_cast<uint32_t>(entities.size());
            entities.push_back({ .generation = 0, .alive = true, .mask = {} });
        }
        entityCount++;
        return MakeEntityID(idx, entities[idx].generation);
    }

    void Destroy(EntityID id) {
        uint32_t idx = EntIndex(id);
        if (!IsAlive(id)) return;

        // Remove all components
        for (auto& [compID, pool] : pools) {
            if (entities[idx].mask.test(compID))
                pool->Remove(idx);
        }

        entities[idx].alive = false;
        entities[idx].mask.reset();
        freeList.push_back(idx);
        entityCount--;
    }

    bool IsAlive(EntityID id) const {
        uint32_t idx = EntIndex(id);
        return idx < entities.size() &&
               entities[idx].alive &&
               entities[idx].generation == EntGeneration(id);
    }

    uint32_t Count() const { return entityCount; }

    // ── Components ───────────────────────────────────────────────────────

    template<typename T>
    T& Add(EntityID id, const T& component = T{}) {
        assert(IsAlive(id));
        uint32_t idx = EntIndex(id);
        entities[idx].mask.set(ComponentRegistry::GetID<T>());
        return GetPool<T>().Add(idx, component);
    }

    template<typename T>
    void Remove(EntityID id) {
        assert(IsAlive(id));
        uint32_t idx = EntIndex(id);
        entities[idx].mask.reset(ComponentRegistry::GetID<T>());
        GetPool<T>().Remove(idx);
    }

    template<typename T>
    T& Get(EntityID id) {
        assert(IsAlive(id));
        return GetPool<T>().Get(EntIndex(id));
    }

    template<typename T>
    const T& Get(EntityID id) const {
        assert(IsAlive(id));
        return const_cast<EntityManager*>(this)->GetPool<T>().Get(EntIndex(id));
    }

    template<typename T>
    bool Has(EntityID id) const {
        if (!IsAlive(id)) return false;
        return entities[EntIndex(id)].mask.test(ComponentRegistry::GetID<T>());
    }

    // ── Query: iterate entities matching a component set ─────────────────
    // Usage: ecs.Each<Transform, Health>([](EntityID id, Transform& t, Health& h) { ... });

    template<typename... Ts, typename Func>
    void Each(Func&& func) {
        // Use the smallest pool as the driver to minimize iteration
        auto& driver = GetSmallestPool<Ts...>();

        for (size_t i = 0; i < driver.Size(); i++) {
            uint32_t idx = driver.GetEntityIndex(i);
            if (!entities[idx].alive) continue;

            // Check all required components
            bool hasAll = (GetPool<Ts>().Has(idx) && ...);
            if (!hasAll) continue;

            EntityID id = MakeEntityID(idx, entities[idx].generation);
            func(id, GetPool<Ts>().Get(idx)...);
        }
    }

private:
    // Pick the pool with fewest entries for tightest iteration
    template<typename First, typename... Rest>
    IComponentPool& GetSmallestPool() {
        IComponentPool* smallest = &GetPool<First>();
        if constexpr (sizeof...(Rest) > 0) {
            IComponentPool* others[] = { &GetPool<Rest>()... };
            for (auto* p : others) {
                if (p->Size() < smallest->Size()) smallest = p;
            }
        }
        return *smallest;
    }
};

// ─── Common game components ─────────────────────────────────────────────────
// Add/remove as your engine grows. Keep components as plain data.

struct TransformComponent {
    Vector3 position = {0, 0, 0};
    Vector3 rotation = {0, 0, 0}; // euler degrees
    Vector3 scale    = {1, 1, 1};

    Matrix ToMatrix() const {
        Matrix m = MatrixIdentity();
        m = MatrixMultiply(m, MatrixScale(scale.x, scale.y, scale.z));
        m = MatrixMultiply(m, MatrixRotateXYZ({DEG2RAD * rotation.x,
                                                DEG2RAD * rotation.y,
                                                DEG2RAD * rotation.z}));
        m = MatrixMultiply(m, MatrixTranslate(position.x, position.y, position.z));
        return m;
    }
};

struct RenderComponent {
    const char* modelKey    = nullptr; // key into ResourceManager
    const char* materialKey = nullptr; // key into ResourceManager
    Color       tint        = WHITE;
    bool        castsShadow = true;
    bool        visible     = true;
};

struct ColliderComponent {
    Vector3 size   = {1, 1, 1};
    Vector3 offset = {0, 0, 0}; // relative to transform
    bool    isTrigger = false;
};

struct HealthComponent {
    float current = 100.0f;
    float max     = 100.0f;
    bool  alive   = true;

    void Damage(float amount) {
        current = fmaxf(0.0f, current - amount);
        alive = current > 0.0f;
    }

    void Heal(float amount) {
        current = fminf(max, current + amount);
    }
};

struct VelocityComponent {
    Vector3 linear  = {0, 0, 0};
    Vector3 angular = {0, 0, 0}; // degrees/sec
};

struct AIComponent {
    enum class State : uint8_t { Idle, Patrol, Chase, Attack, Flee, Dead };
    State   state           = State::Idle;
    float   detectionRange  = 20.0f;
    float   attackRange     = 2.0f;
    float   moveSpeed       = 4.0f;
    EntityID target         = NULL_ENTITY;
    float   stateTimer      = 0.0f;
};

struct TagEnemy {};
struct TagPlayer {};
struct TagProjectile {};
struct TagPickup {};