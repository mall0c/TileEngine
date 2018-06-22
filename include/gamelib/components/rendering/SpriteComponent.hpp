#ifndef GAMELIB_SPRITE_COMPONENT_HPP
#define GAMELIB_SPRITE_COMPONENT_HPP

#include "gamelib/core/ecs/RenderComponent.hpp"
#include "gamelib/components/update/AnimationComponent.hpp"
#include "gamelib/core/res/SpriteResource.hpp"

// Has to be a render component because it needs custom imgui support

/*
 * Config file structure:
 * {
 *     <RenderComponent entries>,
 *
 *     "sprite": "filename"
 *
 *     # Animation overrides (optional)
 *     "length": length,
 *     "speed": speed,
 *     "offset": offset (-1 for random)
 * }
 */

namespace gamelib
{
    class AnimationComponent;

    class SpriteComponent : public RenderComponent
    {
        // HACK: allows AnimationComponent to register its props in this component
        friend class AnimationComponent;

        public:
            constexpr static const char* name = "SpriteComponent";

        public:
            SpriteComponent();

            auto change(const std::string& fname)      -> void;
            auto change(SpriteResource::Handle sprite) -> void;
            auto setIndex(int index)              -> void;

            auto getSpriteName() const -> const std::string&;
            auto getAnimation() const  -> const AnimationComponent&;
            auto getTexture() const    -> TextureResource::Handle;

            auto loadFromJson(const Json::Value& node) -> bool;
            auto writeToJson(Json::Value& node)        -> void;

            auto render(sf::RenderTarget& target, const sf::RenderStates& states_) const -> void;

        protected:
            auto _init()    -> bool;
            auto _quit()    -> void;
            auto _initShape() -> void;

        private:
            SpriteResource::Handle _sprite;
            AnimationComponent _ani;
    };
}

#endif