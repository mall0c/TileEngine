#include "gamelib/imgui/inputs.hpp"
#include "gamelib/imgui/resources.hpp"
#include "gamelib/properties/Property.hpp"
#include "gamelib/core/ecs/Entity.hpp"
#include "gamelib/core/ecs/serialization.hpp"
#include "gamelib/components/RenderComponent.hpp"
#include "gamelib/core/rendering/flags.hpp"
#include "gamelib/core/rendering/Scene.hpp"
#include "gamelib/core/geometry/flags.hpp"
#include "gamelib/utils/conversions.hpp"
#include "imgui.h"
#include "imgui_internal.h"

namespace gamelib
{
    bool inputBitflags(const char* label, unsigned int* flags, int num, const char* const* names)
    {
        bool changed = false;
        if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (int i = 0; i < num; ++i)
                if (names[i])
                    if (ImGui::CheckboxFlags(names[i], flags, 1 << i))
                        changed = true;
            ImGui::TreePop();
        }
        return changed;
    }


    bool inputLayer(const char* label, Layer::Handle* handle)
    {
        int selected = 0;
        auto scene = Scene::getActive();
        std::vector<Layer::Handle> cache;

        int i = 1;
        cache.push_back(Layer::Handle());
        scene->foreachLayer([&](Layer::Handle h, Layer&) {
                if (handle && h == *handle)
                    selected = i;
                cache.push_back(h);
                ++i;
            });

        auto itemgetter = [](void* cache_, int index, const char** name) {
            auto& handles = *static_cast<decltype(cache)*>(cache_);
            if (handles[index].isNull())
                *name = "default";
            else
                *name = Scene::getActive()->getLayer(handles[index])->getName().c_str();
            return true;
        };

        if (ImGui::Combo(label, &selected, itemgetter, (void*)&cache, cache.size()))
        {
            *handle = cache[selected];
            return true;
        }

        return false;
    }


    bool inputSceneData(SceneData& sd)
    {
        int depth = sd.getDepth();
        float parallax = sd.getParallax();
        bool changed = false;

        inputBitflags("Render flags", &sd.flags, num_renderflags, str_renderflags);

        if (ImGui::InputInt("Depth", &depth, 1, 100))
        {
            sd.setDepth(depth);
            changed = true;
        }

        if (ImGui::InputFloat("Parallax", &parallax, 0.01, 0.1, 3))
        {
            sd.setParallax(parallax);
            changed = true;
        }

        return changed;
    }

    void inputCamera(Camera& cam)
    {
        ImGui::InputFloat2("Pos", &cam.pos.x, 2);
        ImGui::InputFloat2("Size", &cam.size.x, 2);
        ImGui::InputFloat("Zoom", &cam.zoom, 0.5, 1, 2);
        ImGui::Combo("Aspect Ratio", (int*)(&cam.ratio), str_aspectratios, NumRatios);
        ImGui::NewLine();
        ImGui::InputFloat2("Velocity", &cam.vel.x, 2);
        ImGui::NewLine();
        ImGui::InputFloat2("Viewport Start", &cam.viewport.x, 2);
        ImGui::InputFloat2("Viewport Stop", &cam.viewport.w, 2);
    }

    bool inputTransform(Transformable& trans)
    {
        static int mode = 0;

        TransformData data;
        bool changed = false;

        ImGui::RadioButton("Local", &mode, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Global", &mode, 1);

        if (mode == 0)
            data = trans.getLocalTransformation();
        else
            data = trans.getTransformation();

        if (trans.isMovable())
            if (ImGui::InputFloat2("Position", &data.pos[0], 2))
                changed = true;

        if (trans.isScalable())
            if (ImGui::InputFloat2("Scale", &data.scale[0], 2))
                changed = true;

        if (trans.isRotatable())
            if (ImGui::InputFloat("Rotation", &data.angle, 1, 10, 2))
                changed = true;

        if (mode == 0)
        {
            if (ImGui::InputFloat2("Origin", &data.origin[0], 2))
                changed = true;

            ImGui::SameLine();
            if (ImGui::Button("Center"))
            {
                data.origin = trans.getLocalBBox().getCenter().asPoint();
                changed = true;
            }
        }

        if (changed)
        {
            if (mode == 0)
                trans.setLocalTransformation(data);
            else
                trans.setTransformation(data);
        }

        return changed;
    }


    bool inputProperties(const PropertyContainer& props)
    {
        bool changed = false;
        for (auto it : props)
            if (inputProperty(it.first, it.second))
                changed = true;
        return changed;
    }

    bool inputProperty(const std::string& name, const PropertyHandle& handle)
    {
        if (handle.serializer)
            return handle.serializer->drawGui(handle, name);
        return false;
    }


    void inputEntityProps(Entity& ent)
    {
        // static constexpr const char* entityFlags[] { "Persistent (Unimplemented)", "Static (Unimplemented)" };

        ImGui::InputText("name", const_cast<char*>(ent.getName().c_str()), ent.getName().size(), ImGuiInputTextFlags_ReadOnly);
        ImGui::NewLine();

        if (ImGui::CollapsingHeader("Geometry", ImGuiTreeNodeFlags_DefaultOpen))
            inputTransform(ent.getTransform());

        // if (ImGui::CollapsingHeader("Flags"))
        //     inputBitflags(&ent.flags, 2, entityFlags);

        if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // static int index = 0;
            // static constexpr const char* comps[] { "ComponentA", "ComponentB", "ComponentC" };
            // ImGui::Combo("##newcomp", &index, comps, 3);
            // ImGui::SameLine();
            // ImGui::Button("Add");

            ImGui::PushItemWidth(0.5 * ImGui::GetWindowContentRegionWidth());
            ent.foreach([](Component* comp) {
                ImGui::PushID(comp);
                if (ImGui::TreeNode(comp->getName().c_str()))
                {
                    inputComponent(*comp);
                    ImGui::TreePop();
                }
                ImGui::PopID();
                return true;
            });
            ImGui::PopItemWidth();
        }
    }

    void inputComponent(Component& comp)
    {
        if (comp.getTransform())
            if (ImGui::TreeNodeEx("Geometry", ImGuiTreeNodeFlags_DefaultOpen))
            {
                inputTransform(*comp.getTransform());
                ImGui::TreePop();
            }

        inputProperties(comp.getProperties());
    }

    bool inputComponentSelect(const std::string& name, Component** ptr, const Entity& ent, const Component* self, unsigned int filter, int numfilters, const char* const* namefilters)
    {
        struct Cache
        {
            Component* ptr;
            std::string name;
        };

        std::vector<Cache> cache;
        int current = -1;

        for (auto& i : ent)
        {
            if (i.ptr.get() == self)
                continue;

            if (filter != 0 && i.ptr->getID() != filter)
                continue;

            if (numfilters > 0 && namefilters)
            {
                bool skip = true;
                for (int f = 0; f < numfilters; ++f)
                    if (i.ptr->getName() == namefilters[f])
                    {
                        skip = false;
                        break;
                    }

                if (skip)
                    continue;
            }

            if (i.ptr.get() == *ptr)
                current = cache.size();
            cache.push_back({ i.ptr.get(), generateName(i.ptr->getName(), i.id) });
        }

        auto getter = [](void* data, int index, const char** str) {
            auto& cache_ = (*static_cast<decltype(cache)*>(data));
            if (index < 0 || index >= cache_.size())
                return false;
            *str = cache_[index].name.c_str();
            return true;
        };

        if (ImGui::Combo(name.c_str(), &current, getter, &cache, cache.size()))
        {
            *ptr = cache[current].ptr;
            return true;
        }

        return false;
    }
}
