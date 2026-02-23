#pragma once
#include "types.h"

using EntityID = unsigned int;

class EntityManager {
public:
    EntityID CreateEntity();
    void DestroyEntity(EntityID id);
    bool IsAlive(EntityID id) const;
    // Future: component attach/detach, queries, etc.
};
