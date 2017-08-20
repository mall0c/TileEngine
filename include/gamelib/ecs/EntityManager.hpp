#ifndef GAMELIB_ENTITYMANAGER_HPP
#define GAMELIB_ENTITYMANAGER_HPP

#include <memory>
#include "gamelib/utils/SlotMap.hpp"
#include "gamelib/ecs/Entity.hpp"

namespace gamelib
{
    class EntityManager
    {
        public:
            typedef SlotMapShort<Entity>::Handle Handle;

        public:
            auto add()                  -> Entity&;
            auto add(Entity&& ent)      -> Entity&;
            auto destroy(Handle handle) -> void;
            auto get(Handle) const      -> const Entity*;
            auto get(Handle)            -> Entity*;

            auto clear() -> void;

            // Returns the first entity with the given name
            auto find(const std::string& name) const -> const Entity*;
            auto find(const std::string& name)       -> Entity*;

        private:
            gamelib::SlotMapShort<Entity> _entities;
    };
}

#endif