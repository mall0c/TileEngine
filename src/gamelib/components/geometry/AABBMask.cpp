#include "gamelib/components/geometry/AABBMask.hpp"
#include "gamelib/properties/PropComponent.hpp"

namespace gamelib
{
    constexpr const char* AABBMask::name;

    AABBMask::AABBMask(unsigned int flags_) :
        CollisionComponent(name),
        _comp(nullptr)
    {
        flags = flags_;
        _setSupportedOps(false, false, false);
        registerProperty(_props, "component", _comp, *this, PROP_METHOD(_comp, setComponent));
    }


    const math::AABBf& AABBMask::getBBox() const
    {
        return _comp->getTransform()->getBBox();
    }

    bool AABBMask::intersect(const math::Point2f& point) const
    {
        return getBBox().contains(point);
    }

    Intersection AABBMask::intersect(const math::Line2f& line) const
    {
        return line.intersect(getBBox());
    }

    Intersection AABBMask::intersect(const math::AABBf& rect) const
    {
        return getBBox().intersect(rect);
    }

    Intersection AABBMask::sweep(const math::AABBf& rect, const math::Vec2f& vel) const
    {
        return rect.sweep(vel, getBBox());
    }

    bool AABBMask::setComponent(const Component* c)
    {
        if (static_cast<Component*>(this) == c)
        {
            LOG_ERROR("Can't assign self");
            return false;
        }
        else if (c->getTransform())
        {
            _comp = c;
            return true;
        }
        else
        {
            LOG_ERROR("Component is not a Transformable");
            return false;
        }
    }

    const Component* AABBMask::getComponent() const
    {
        return _comp;
    }
}