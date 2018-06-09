#include "gamelib/core/geometry/CollisionSystem.hpp"
#include <algorithm>

namespace gamelib
{
    constexpr const char* CollisionSystem::name;

    void CollisionSystem::add(Collidable* col)
    {
        _objs.push_back(col);
    }

    void CollisionSystem::remove(Collidable* col)
    {
        auto it = std::find(_objs.begin(), _objs.end(), col);
        if (it != _objs.end())
            _objs.erase(it);
    }

    void CollisionSystem::destroy()
    {
        _objs.clear();
    }

    size_t CollisionSystem::size() const
    {
        return _objs.size();
    }

    Collidable* CollisionSystem::intersect(const math::Point2f& point, unsigned int flags) const
    {
        return intersectAll(point, flags, [](Collidable*) { return true; });
    }

    Collidable* CollisionSystem::intersect(const math::AABBf& rect, unsigned int flags) const
    {
        return intersectAll(rect, flags, [](Collidable*) { return true; });
    }

    TraceResult CollisionSystem::trace(const math::Line2f& line, const Collidable* self, unsigned int flags) const
    {
        return trace(line, [](Collidable*, const Intersection&) { return true; }, self, flags);
    }

    TraceResult CollisionSystem::trace(const math::AABBf& rect, const math::Vec2f& vel, const Collidable* self, unsigned int flags) const
    {
        return trace(rect, vel, [](Collidable*, const Intersection&) { return true; }, self, flags);
    }


    TraceResult::TraceResult() :
        obj(nullptr)
    { }

    TraceResult::operator bool() const
    {
        return isec;
    }
}
