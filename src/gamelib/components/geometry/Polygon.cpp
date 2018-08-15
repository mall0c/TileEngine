#include "gamelib/components/geometry/Polygon.hpp"
#include "gamelib/utils/json.hpp"
#include "gamelib/utils/log.hpp"

namespace gamelib
{
    constexpr const char* normaldir_hints[] = { "Both", "Left", "Right" };

    Polygon::Polygon(unsigned int flags) :
        Polygon(math::TriangleStrip, flags)
    { }

    Polygon::Polygon(math::PolygonType type, unsigned int flags_) :
        CollisionComponent(name),
        _polygon(type, math::NormalLeft)
    {
        flags = flags_;
        _supported = movable | scalable; // TODO: rotation
        _props.registerProperty("Normals", *reinterpret_cast<int*>(&_polygon.normaldir), 0, 3, normaldir_hints);
    }

    bool Polygon::intersect(const math::Point2f& point) const
    {
        return _polygon.intersect(point);
    }

    Intersection Polygon::intersect(const math::Line2f& line) const
    {
        return _polygon.intersect(line);
    }

    Intersection Polygon::intersect(const math::AABBf& rect) const
    {
        Intersection isec;
        _polygon.foreachSegment([&](const math::Line2f& seg) {
                isec = seg.intersect(rect);
                return isec;
            });
        return isec;
    }

    Intersection Polygon::sweep(const math::AABBf& rect, const math::Vec2f& vel) const
    {
        return rect.sweep(vel, _polygon);
    }


    void Polygon::move(const math::Vec2f& rel)
    {
        _polygon.move(rel);
        CollisionComponent::move(rel);
    }

    void Polygon::scale(const math::Vec2f& scale)
    {
        _polygon.setScale(getScale() * scale);
        CollisionComponent::scale(scale);
    }


    const math::Point2f& Polygon::getPosition() const
    {
        return _polygon.getOffset().asPoint();
    }

    const math::Vec2f& Polygon::getScale() const
    {
        return _polygon.getScale();
    }

    const math::AABBf& Polygon::getBBox() const
    {
        return _polygon.getBBox();
    }

    const math::Polygon<float>& Polygon::getPolygon() const
    {
        return _polygon;
    }

    bool Polygon::loadFromJson(const Json::Value& node)
    {
        CollisionComponent::loadFromJson(node);
        _polygon.clear();
        _polygon.type = static_cast<math::PolygonType>(node.get("type", _polygon.type).asInt());
        if (node.isMember("vertices"))
        {
            auto& vertices = node["vertices"];
            math::Point2f p;
            for (auto& i : vertices)
            {
                if (gamelib::loadFromJson(i, p))
                    _polygon.addRaw(p);
                else
                    LOG_WARN("Incorrect vertex format: ", node.toStyledString());
            }
        }
        return true;
    }

    void Polygon::writeToJson(Json::Value& node)
    {
        CollisionComponent::writeToJson(node);
        node["type"] = _polygon.type;
        auto& vertices = node["vertices"];
        vertices.resize(_polygon.size());
        for (size_t i = 0; i < _polygon.size(); ++i)
            gamelib::writeToJson(vertices[Json::ArrayIndex(i)], _polygon.getRaw(i));
    }

    void Polygon::add(const math::Point2f& point)
    {
        _polygon.add(point);
    }

    void Polygon::edit(size_t i, const math::Point2f& p)
    {
        _polygon.edit(i, p);
    }

    void Polygon::clear()
    {
        _polygon.clear();
    }

    size_t Polygon::size() const
    {
        return _polygon.size();
    }
}
