#include "editor/editor/tools/SelectTool.hpp"
#include "gamelib/utils/log.hpp"
#include "gamelib/core/event/EventManager.hpp"
#include "gamelib/core/ecs/RenderComponent.hpp"
#include "gamelib/core/ecs/CollisionComponent.hpp"
#include "gamelib/core/ecs/EntityManager.hpp"
#include "gamelib/core/geometry/CollisionSystem.hpp"
#include "editor/editor/tools/ToolUtils.hpp"
#include "editor/editor/EditorShared.hpp"
#include "editor/events/OnSelect.hpp"
#include "imgui.h"

namespace gamelib
{
    SelectTool::SelectTool() :
        _renderSolid(true)
    { }

    void SelectTool::onMousePressed()
    {
        select(EditorShared::getMouse().x, EditorShared::getMouse().y, 0);

        auto* ent = getSelected();
        if (ent)
        {
            _dragoffset = EditorShared::getMouse() - ent->getTransform().getPosition();
            ent->getTransform().refreshBBox();
        }
    }

    void SelectTool::onDrag()
    {
        auto* ent = getSelected();
        if (ent)
            ent->getTransform().setPosition(
                    EditorShared::snap(EditorShared::getMouse() - _dragoffset));
    }

    void SelectTool::render(sf::RenderTarget& target)
    {
        auto* ent = getSelected();
        if (ent)
        {
            drawRectOutline(target, ent->getTransform().getBBox());
            if (_renderSolid)
                drawCollisions(target, *ent, collision_solid);
        }
    }

    void SelectTool::drawGui()
    {
        ImGui::Checkbox("Show solid", &_renderSolid);
    }

    void SelectTool::select(Entity::Handle enthandle)
    {
        if (enthandle == _selected)
            return;

        auto old = _selected;

        if (getEntity(enthandle))
        {
            _selected = enthandle;
            LOG("Selected entity ", enthandle.index, enthandle.version);
        }
        else
        {
            _selected = Entity::Handle();
            LOG("Selection cleared");
        }

        EventManager::getActive()->triggerEvent(
                EventPtr(new OnSelectEvent(old, enthandle)));
    }

    void SelectTool::select(Entity* ent)
    {
        if (ent)
            select(ent->getHandle());
        else
            select(Entity::Handle());
    }

    void SelectTool::select(float x, float y, unsigned int flags)
    {
        Entity* top = nullptr;
        int depth = 0;
        CollisionSystem::getActive()->findAll(
                math::Point2f(x, y), flags, [&](Collidable* col) {

            auto ent = static_cast<CollisionComponent*>(col)->getEntity();
            auto ren = ent->find<RenderComponent>();
            if (!ren || !ren->isVisible())
                return false;

            // NOTE: If you came back here, because you revised the ECS
            // and now have the possibility of adding multiple render
            // components with different depths which results in wrong
            // results in the editor when selecting objects, you have
            // 2 options:
            //  - Add an option to mark a component as primary and check
            //    that one
            //  - Give render components a bounding box and check if the
            //    point lies in that bounding box.
            //    This option is probably better because you should use
            //    a quadtree for rendering anyway.
            //    Note that this also means you need to adapt the
            //    render code to use the render component bbox rather
            //    than the collision component bbox.
            int d = ren->getDepth();
            if (top == nullptr || d < depth)
            {
                depth = d;
                top = ent;
            }
            return false;
        });

        select(top);
    }

    const Entity* SelectTool::getSelected() const
    {
        return getEntity(_selected);
    }

    Entity* SelectTool::getSelected()
    {
        return getEntity(_selected);
    }
}