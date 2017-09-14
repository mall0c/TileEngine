#include "gamelib/movement/QPhysics.hpp"
#include "gamelib/geometry/CollisionSystem.hpp"
#include "gamelib/geometry/flags.hpp"
#include "gamelib/geometry/AABB.hpp"
#include "gamelib/ecs/Entity.hpp"
#include "gamelib/ecs/CollisionComponent.hpp"
#include "gamelib/Game.hpp"
#include "gamelib/utils/json.hpp"

namespace gamelib
{
    QPhysics::QPhysics(int interval) :
        QPhysics(nullptr, 1)
    { }

    QPhysics::QPhysics(const math::AABBf* box, int interval) :
        UpdateComponent(name, interval),
        overbounce(1),
        ground(nullptr),
        _bbox(box)
    { }

    void QPhysics::accelerate(const math::Vec2f& wishdir, float wishspeed, float accel)
    {
        float addspeed = wishspeed - vel.dot(wishdir);

        if (addspeed <= 0)
            return;

        float accelspeed = accel * getSubsystem<Game>()->getFrametime() * wishspeed * friction;

        if (accelspeed > addspeed)
            accelspeed = addspeed;

        vel += wishdir * accelspeed;
    }

    void QPhysics::update(float elapsed)
    {
        if (!_bbox)
            return;

        _checkGround();

        if (!ground)   // Apply gravity
        {
            vel += gravityDirection * (gravity * elapsed);
        }
        else    // Apply friction
        {
            float speed = vel.abs();
            if (speed > 0)
            {
                float drop = (speed < stopSpeed) ? stopFriction : (speed * friction * elapsed);
                vel *= std::max(speed - drop, 0.0f) / speed;
            }
        }

        // TODO: basevelocity
        _clipmove(elapsed);
    }


    void QPhysics::_checkGround()
    {
        TraceResult tr = getSubsystem<CollisionSystem>()->trace(
                *_bbox, gravityDirection, _self, collision_solid);
        ground = static_cast<CollisionComponent*>(tr.obj);
    }

    void QPhysics::_clipmove(float elapsed)
    {
        auto newpos = _bbox->pos;
        auto originalvel = vel;
        float timeleft = elapsed;
        TraceResult trace;

        while (timeleft > 0)
        {
            if (vel == math::Vec2f())
                return;

            auto framevel = vel * timeleft;
            trace = getSubsystem<CollisionSystem>()->trace(*_bbox, framevel, nullptr, collision_solid);

            if (trace)
            {
                if (trace.isec.time > 0)
                {
                    vel = originalvel - trace.isec.normal * trace.isec.normal.dot(vel) * overbounce;
                    for (size_t i = 0; i < 2; ++i)
                        if (vel[i] < 0.2 && vel[i] >= -0.2)
                            vel[i] = 0;

                    // Magic Quake engine number that prevents getting stuck
                    newpos += framevel * (trace.isec.time - 0.03125);
                    timeleft -= timeleft * trace.isec.time;
                }
                else
                {
                    LOG_DEBUG("stuck");
                    LOG_DEBUG("normal: x: ", trace.isec.normal.x, " y: ", trace.isec.normal.y);
                    // _nudge(&newpos, trace);
                    // newpos += trace.isec.normal * 0.03125;
                    newpos += trace.isec.normal;
                    break;
                }

                if (vel.dot(originalvel) <= 0)
                    break;
            }
            else
            {
                // Whole distance covered
                newpos += framevel;
                break;
            }
        }

        getEntity()->getTransform().move(newpos - _bbox->pos);
    }

    void QPhysics::_nudge(math::Vec2f* newpos, const TraceResult& trace)
    {
        // for (int y : { -1, 0, 1 })
        //     for (int x : { -1, 0, 1 })
        //     {
        //         math::AABBf(*newpos, _bbox)
        //         if (getSubsystem<CollisionSystem>()->trace(*_bbox, math::Vec2f(x, y) * 0.125, nullptr, collision_solid))
        //             // if (!trace.obj->intersect(*newpos + math::Vec2f(x, y) * 0.125))
        //         {
        //             *newpos += math::Vec2f(x, y);
        //             return;
        //         }
        //     }
    }

    bool QPhysics::_init()
    {
        UpdateComponent::_init();
        // getEntity()->getTransform().add(this);
        return true;
    }

    void QPhysics::_quit()
    {
        UpdateComponent::_quit();
        // getEntity()->getTransform().remove(this);
    }

    void QPhysics::_refresh()
    {
        auto comp = getEntity()->find<CollisionComponent>();
        if (comp)
        {
            _bbox = &comp->getBBox();
            _self = comp;
        }
        // getEntity()->getTransform().remove(this);
    }

    bool QPhysics::loadFromJson(const Json::Value& node)
    {
        UpdateComponent::loadFromJson(node);

        // gamelib::loadFromJson(node["transform"], *static_cast<Transformable*>(this));
        gamelib::loadFromJson(node["vel"], vel);
        overbounce = node.get("overbounce", overbounce).asFloat();

        if (node.isMember("global"))
        {
            auto& global = node["global"];
            gamelib::loadFromJson(global["gravityDirection"], gravityDirection);
            gravity = global.get("gravity", gravity).asFloat();
            friction = global.get("friction", friction).asFloat();
            stopFriction = global.get("stopFriction", stopFriction).asFloat();
            stopSpeed = global.get("stopSpeed", stopSpeed).asFloat();
        }

        return true;
    }

    void QPhysics::writeToJson(Json::Value& node)
    {
        UpdateComponent::writeToJson(node);
        // gamelib::writeToJson(node["transform"], *static_cast<Transformable*>(this));
        gamelib::writeToJson(node["vel"], vel);
        node["overbounce"] = overbounce;
    }

    // void QPhysics::move(const math::Vec2f& rel)
    // {
    //     _bbox.pos += rel;
    // }
    //
    // const math::Point2f& QPhysics::getPosition() const
    // {
    //     return _bbox.getCenter().asPoint();
    // }
    //
    // const math::AABBf& QPhysics::getBBox() const
    // {
    //     return _bbox;
    // }


    math::Vec2f QPhysics::gravityDirection(0, 1);
    float QPhysics::gravity = 3000;
    float QPhysics::friction = 0.5;
    float QPhysics::stopFriction = 10;
    float QPhysics::stopSpeed = 100;
}
