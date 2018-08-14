#include "gamelib/editor/Editor.hpp"
#include "gamelib/core/Game.hpp"
#include "gamelib/export.hpp"
#include "gamelib/utils/log.hpp"
#include "gamelib/utils/string.hpp"
#include "gamelib/core/rendering/flags.hpp"
#include "gamelib/core/rendering/Scene.hpp"
#include "gamelib/core/ecs/Entity.hpp"
#include "gamelib/core/ecs/serialization.hpp"
#include "gamelib/core/input/InputSystem.hpp"
#include "gamelib/core/event/EventManager.hpp"
#include "gamelib/events/SFMLEvent.hpp"
#include "gamelib/editor/tools/SpriteTool.hpp"
#include "gamelib/editor/tools/BrushTool.hpp"
#include "gamelib/editor/tools/VertexTool.hpp"
#include "gamelib/editor/tools/SelectTool.hpp"
#include "gamelib/editor/tools/EntityTool.hpp"
#include "gamelib/editor/ui/JsonView.hpp"
#include "gamelib/editor/ui/inputs.hpp"
#include "gamelib/editor/ui/LayerUI.hpp"
#include "gamelib/editor/ui/EntityList.hpp"
#include "gamelib/editor/ui/GlobalGameConfig.hpp"
#include "gamelib/editor/EditorShared.hpp"
#include "gamelib/components/editor/BrushComponent.hpp"
#include "imgui-SFML.h"
#include "imguifilesystem.h"

namespace gamelib
{
    constexpr const char* buttonStrings[] = {
        "Select Tool",
        "Sprite Tool",
        "Polygon Tool",
        "Vertex Tool",
        "Entity Tool"
    };

    // Strips BrushComponents from output
    void defaultExport(const std::string& fname);


    Editor::Editor() :
        GameState(gamestate_freeze),
        _exportcallback(defaultExport),
        _currenttool(nullptr),
        _camctrl(getSubsystem<Scene>()),
        _entsearch(false),
        _drag(false),
        _grid(32, 32),
        _snap(true),
        _run(false),
        _hidegui(true)
    { }

    bool Editor::init(Game* game)
    {
        LOG_DEBUG("Init Editor...");
        EditorShared::_editor = this;

        ImGui::SFML::Init(game->getWindow());

        auto evmgr = getSubsystem<EventManager>();
        evmgr->regCallback<SFMLEvent>(_eventCallback, this);

        _updateRunFlags();

        _tools[ToolSelect].reset(new SelectTool());
        _tools[ToolSprite].reset(new SpriteTool());
        _tools[ToolBrush].reset(new BrushTool());
        _tools[ToolVertex].reset(new VertexTool());
        _tools[ToolEntity].reset(new EntityTool());
        setTool(ToolBrush);

        return true;
    }

    void Editor::quit()
    {
        _resviewer.close();

        auto evmgr = getSubsystem<EventManager>();
        evmgr->unregCallback<SFMLEvent>(_eventCallback, this);

        _currenttool = nullptr;
        for (auto& i : _tools)
            i.reset();

        ImGui::SFML::Shutdown();
        LOG_DEBUG("Editor unloaded");
    }

    void Editor::update(float elapsed)
    {
        auto input = getSubsystem<InputSystem>();
        _mouseSnapped = EditorShared::snap(input->getMouse().world);

        if (input->isKeyPressed(sf::Keyboard::F5))
        {
            _run = !_run;
            _updateRunFlags();
        }

        if (!_run || !_hidegui)
        {
            input->markConsumed(ImGui::GetIO().WantCaptureKeyboard,
                    ImGui::GetIO().WantCaptureMouse);

            ImGui::SFML::Update(getSubsystem<Game>()->getWindow(), sf::seconds(elapsed));
            _drawGui();
        }

        if (!_run)
        {
            _handleInput();
            _camctrl.update(elapsed);
        }
    }

    void Editor::render(sf::RenderTarget& target)
    {
        if (_run && _hidegui)
            return;

        // necessary, otherwise things will disappear randomly
        getSubsystem<Game>()->getWindow().resetGLStates();

        if (_snap)
            _grid.render(target);

        if (_currenttool)
            _currenttool->render(target);

        ImGui::Render();
    }


    void Editor::setTool(Tools tool)
    {
        _currenttool = _tools[tool].get();
    }

    SelectTool& Editor::getSelectTool()
    {
        return *(SelectTool*)_tools[ToolSelect].get();
    }


    void Editor::pause()
    {
        _run = false;
        _updateRunFlags();
    }

    void Editor::unpause()
    {
        _run = true;
        _updateRunFlags();
    }


    bool Editor::load()
    {
        return _savefile.empty() ? false : gamelib::loadSave(_savefile);
    }

    bool Editor::load(const std::string& fname)
    {
        _savefile = fname;
        return load();
    }

    bool Editor::save()
    {
        return _savefile.empty() ? false : gamelib::save(_savefile);
    }

    bool Editor::save(const std::string& fname)
    {
        _savefile = fname;
        return this->save();
    }


    void Editor::registerExportCallback(ExportFunction callback)
    {
        _exportcallback = callback ? callback : defaultExport;
    }

    void Editor::_updateRunFlags()
    {
        auto scene = getSubsystem<Scene>();
        if (_run)
        {
            RMFLAG(scene->flags, render_noparallax | render_drawhidden);
            RMFLAG(flags, gamestate_freeze);
        }
        else
        {
            ADDFLAG(scene->flags, render_noparallax | render_drawhidden);
            ADDFLAG(flags, gamestate_freeze);
        }
    }


    void Editor::_eventCallback(Editor* self, EventPtr ev)
    {
        sf::Event& sfev = ev->get<SFMLEvent>()->ev;
        ImGui::SFML::ProcessEvent(sfev);
    }

    void Editor::_drawGui()
    {
        static bool testwindow = false;
        static bool jsonwindow = false;
        static bool toolbox = true;
        static bool layerbox = true;
        static bool entprops = true;
        static bool entlist = true;
        static bool gamecfg = false;
        static ImGuiFs::Dialog loaddlg;
        static ImGuiFs::Dialog savedlg;
        static ImGuiFs::Dialog exportdlg;

        bool chooseload = false;
        bool choosesave = false;
        bool chooseexport = false;

        auto selected = getSelectTool().getSelected();
        auto input = getSubsystem<InputSystem>();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save", nullptr, nullptr, !_savefile.empty()))
                    save();

                if (ImGui::MenuItem("Save as"))
                    choosesave = true;

                if (ImGui::MenuItem("Load"))
                    chooseload = true;

                if (ImGui::MenuItem("Reload"))
                    load();

                if (ImGui::MenuItem("Export"))
                    chooseexport = true;

                if (ImGui::MenuItem("Quit"))
                    getSubsystem<Game>()->close();

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Run", "F5", &_run))
                    _updateRunFlags();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                auto scene = getSubsystem<Scene>();
                if (ImGui::MenuItem("Show hidden", nullptr, scene->flags & render_drawhidden))
                    TOGGLEFLAG(scene->flags, render_drawhidden);

                if (ImGui::MenuItem("Enable parallax", nullptr, !(scene->flags & render_noparallax)))
                    TOGGLEFLAG(scene->flags, render_noparallax);

                ImGui::MenuItem("Hide Editor when running", nullptr, &_hidegui);

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Tools"))
            {
                if (ImGui::MenuItem("Resource Viewer"))
                    _resviewer.open();
                ImGui::MenuItem("Toolbox", nullptr, &toolbox);
                ImGui::MenuItem("Entity properties", nullptr, &entprops);
                ImGui::MenuItem("Entity list", nullptr, &entlist);
                ImGui::MenuItem("Layer settings", nullptr, &layerbox);
                ImGui::MenuItem("Json viewer", nullptr, &jsonwindow);
                ImGui::MenuItem("Global game config", nullptr, &gamecfg);
                ImGui::Separator();
                ImGui::MenuItem("Test window", nullptr, &testwindow);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Grid"))
            {
                ImGui::MenuItem("Snap to grid", "G", &_snap);
                if (ImGui::MenuItem("Increase", "Q"))
                    _grid.increase();
                if (ImGui::MenuItem("Decrease", "E"))
                    _grid.decrease();
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        { // Loading / Saving
            if (strlen(loaddlg.chooseFileDialog(chooseload, loaddlg.getLastDirectory())) > 0)
                load(loaddlg.getChosenPath());

            if (strlen(savedlg.saveFileDialog(choosesave, savedlg.getLastDirectory())) > 0)
                this->save(savedlg.getChosenPath());

            if (strlen(exportdlg.saveFileDialog(chooseexport, exportdlg.getLastDirectory())) > 0)
                _exportcallback(exportdlg.getChosenPath());
        }

        if (ImGui::BeginPopupContextVoid("Context Menu", sf::Mouse::Right))
        {
            if (ImGui::Selectable("Move entity here") && selected)
                selected->getTransform().setPosition(input->getMouse().world);
            ImGui::EndPopup();
        }

        { // Dialogues
            if (testwindow)
                ImGui::ShowTestWindow(&testwindow);

            if (jsonwindow)
                drawJsonView(&jsonwindow);

            if (layerbox)
                drawLayerUI(&layerbox);

            if (entlist)
                drawEntityList(&entlist);

            if (_entsearch)
                drawSearchPopup(&_entsearch);

            if (gamecfg)
                drawGameConfig(&gamecfg);

            _resviewer.draw();

            if (toolbox)
            {
                if (ImGui::Begin("Toolbox", &toolbox, ImVec2(250, 125)))
                {
                    for (size_t i = 0; i < NumTools; ++i)
                        if (ImGui::Button(buttonStrings[i]))
                            setTool(static_cast<Tools>(i));
                    ImGui::Separator();

                    _currenttool->drawGui();
                }
                ImGui::End();
            }

            auto selected = getSelectTool().getSelected();
            if (entprops && selected)
            {
                if (ImGui::Begin("Properties"))
                    inputEntityProps(*selected);
                ImGui::End();
            }
        }
    }

    void Editor::_handleInput()
    {
        auto input = getSubsystem<InputSystem>();
        Tool* realtool = _currenttool;  // backup

        { // Keyboard
            if (input->isKeyDown(sf::Keyboard::LControl))
            {
                _currenttool = &getSelectTool();

                if (input->isKeyPressed(sf::Keyboard::S))
                    save();
                else if (input->isKeyPressed(sf::Keyboard::R))
                    load();
            }

            if (input->isKeyPressed(sf::Keyboard::F))
                _entsearch = true;

            if (input->isKeyPressed(sf::Keyboard::Delete) && getSelectTool().getSelected())
            {
                getSelectTool().getSelected()->destroy();
                getSelectTool().select(nullptr);
            }

            if (input->isKeyPressed(sf::Keyboard::Q))
                _grid.increase();

            if (input->isKeyPressed(sf::Keyboard::E))
                _grid.decrease();

            if (input->isKeyPressed(sf::Keyboard::G))
                _snap = !_snap;

            if (input->isKeyPressed(sf::Keyboard::Z))
            {
                auto selected = getSelectTool().getSelected();
                if (selected)
                {
                    auto cam = getSubsystem<Scene>()->getCamera(0);
                    cam->center(selected->getTransform().getPosition().asVector());
                    cam->zoom = 1;
                }
            }
        }

        { // Mouse
            auto& mouse = input->getMouse();

            if (!input->isMouseConsumed())
            {
                if (input->getMouse().moved)
                {
                    if (_drag)
                        _currenttool->onDrag();
                    else
                        _currenttool->onMouseMove();
                }

                if (mouse.wheel)
                    getSubsystem<Scene>()->getCamera(0)->zoomTowards(mouse.world.x, mouse.world.y, mouse.wheel / -10.0);
            }

            if (input->isMousePressed(sf::Mouse::Left))
            {
                _currenttool->onMousePressed();
                _drag = true;
            }
            else if (input->isMouseReleased(sf::Mouse::Left))
            {
                _currenttool->onMouseRelease();
                _drag = false;
            }
        }

        _currenttool = realtool;
    }


    void defaultExport(const std::string& fname)
    {
        save(fname, [](Json::Value& node, Entity& ent) {
                writeToJson(node, ent, [](Json::Value& compnode, Component& comp) {
                        if (comp.getID() != BrushComponent::id)
                            comp.writeToJson(compnode);
                    });
            });
        LOG("Map exported to ", fname);
    }
}