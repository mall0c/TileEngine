#include "gamelib/core/rendering/RenderSystem.hpp"
#include "gamelib/core/rendering/flags.hpp"
#include "gamelib/utils/log.hpp"
#include "gamelib/utils/conversions.hpp"
#include "math/geometry/PointSet.hpp"
#include "math/geometry/mesh_intersect.hpp"
#include "gamelib/json/json-rendering.hpp"

// Enables setting a breakpoint inside a macro by breaking here
template <typename... Args>
void _warn_macro(Args&&... args)
{
    LOG_WARN(args...);
}

#define ASSURE_VALID_RET(nodehandle, ret) \
if (!_nodes.isValid(nodehandle))    \
{   \
    _warn_macro("Trying to access invalid node handle");   \
    return ret; \
}

#define ASSURE_VALID(nodehandle) ASSURE_VALID_RET(nodehandle, )

#define CHECK_MESH_BOUNDS_RET(handle, offset, ret) \
if (!_nodes[handle].mesh.handle.isValid() || (offset) >= _nodes[handle].mesh.handle.size) \
{   \
    _warn_macro("Vertex index out of bounds"); \
    return ret; \
}

#define CHECK_MESH_BOUNDS(handle, offset) CHECK_MESH_BOUNDS_RET(handle, offset, )

#define ASSURE_LAYER_VALID_RET(layerhandle, ret) \
if (!_layers.isValid(layerhandle))    \
{   \
    _warn_macro("Trying to access invalid layer handle");   \
    return ret; \
}

#define ASSURE_LAYER_VALID(layerhandle) ASSURE_LAYER_VALID_RET(layerhandle, )


namespace gamelib
{
    math::AABBf calculateBBox(const sf::Vertex* vertices, size_t size)
    {
        if (size < 2)
            return math::AABBf();

        math::Vec2f min = convert(vertices[0].position),
                    max = min;

        for (size_t i = 1; i < size; ++i)
        {
            const math::Vec2f& p = convert(vertices[i].position);
            for (int k = 0; k < 2; ++k)
            {
                min[k] = std::min(min[k], p[k]);
                max[k] = std::max(max[k], p[k]);
            }
        }

        return math::AABBf(min.asPoint(), max - min);
    }

    class VertexPointSet: public math::AbstractPointSet<float>
    {
        public:
            VertexPointSet(const sf::Vertex* array, const RenderNode* node) :
                _node(node),
                _array(array)
            { }

            auto add(const math::Point2f&)            -> void final override { assert(false && "Not supported"); };
            auto edit(size_t, const math::Point2f&)   -> void final override { assert(false && "Not supported"); };
            auto insert(size_t, const math::Point2f&) -> void final override { assert(false && "Not supported"); };
            auto remove(size_t)                       -> void final override { assert(false && "Not supported"); };
            auto clear()                              -> void final override { assert(false && "Not supported"); };

            auto size() const -> size_t final override
            {
                return _node->mesh.size;
            }

            auto get(size_t i) const -> math::Point2f final override
            {
                return convert(_node->transform.transformPoint(_array[i].position)).asPoint();
            }

            auto getBBox() const -> math::AABBf final override
            {
                return _node->_globalBBox;
            }

        private:
            const RenderNode* _node;
            const sf::Vertex* _array;
    };



	RenderSystem::RenderSystem() :
        renderBoxes(false),
        _numrendered(0),
        _orderdirty(true)
	{ }


    auto RenderSystem::clear() -> void
    {
        LOG_DEBUG_WARN("Clearing all RenderNodes");
        _nodes.clear();
        _vertices.clear();
        _dirtylist.clear();
        _renderqueue.clear();
        _orderdirty = false;
        renderBoxes = false;
    }

    auto RenderSystem::destroy() -> void
    {
        LOG_DEBUG_WARN("Destroying RenderSystem");
        clear();
        _layers.clear();
        _root = RenderOptions();
        _numrendered = 0;
    }


    auto RenderSystem::createNode(Component* owner) -> NodeHandle
    {
        NodeHandle handle = _nodes.acquire();
        _nodes[handle].owner = owner;
        // don't add to queue, because there's no mesh yet
        LOG_DEBUG("Created RenderNode");
        return handle;
    }

    auto RenderSystem::removeNode(NodeHandle handle) -> void
    {
        ASSURE_VALID(handle);
        _nodes.destroy(handle);
        _orderdirty = true;
    }

    auto RenderSystem::getNode(NodeHandle handle) const -> const RenderNode*
    {
        return _nodes.get(handle);
    }

    auto RenderSystem::getNodeGlobalBBox(NodeHandle handle) const -> math::AABBf
    {
        ASSURE_VALID_RET(handle, math::AABBf());

        _updateNodeGlobalBBox(handle);
        return _nodes[handle]._globalBBox;
    }

    auto RenderSystem::getNodeGlobalOptions(NodeHandle handle) const -> RenderOptions
    {
        ASSURE_VALID_RET(handle, RenderOptions());

        const RenderNode& node = *getNode(handle);
        const RenderLayer* layer = _layers.get(node.layer);

        RenderOptions options = node.options;
        if (layer)
            options.inherit(layer->options);
        options.inherit(_root);

        return options;
    }

    auto RenderSystem::getNodeVisible(NodeHandle handle) const -> bool
    {
        ASSURE_VALID_RET(handle, false);
        CHECK_MESH_BOUNDS_RET(handle, 0, false);

        if (_nodes[handle].mesh.size == 0)
            return false;
        return getNodeGlobalOptions(handle).isVisible();
    }


    auto RenderSystem::setNodeOwner(NodeHandle handle, Component* owner) -> void
    {
        ASSURE_VALID(handle);
        _nodes[handle].owner = owner;
    }

    auto RenderSystem::setNodeOptions(NodeHandle handle, const RenderOptions& options) -> void
    {
        ASSURE_VALID(handle);
        _nodes[handle].options = options;
    }

    auto RenderSystem::setNodeOptions(
            NodeHandle handle,
            const unsigned int* flags,
            const float* parallax,
            const sf::BlendMode* blendMode,
            const TextureResource::Handle* texture,
            const sf::Shader* shader)
        -> void
    {
        ASSURE_VALID(handle);
        RenderOptions options = _nodes[handle].options;
        if (flags)     options.flags = *flags;
        if (parallax)  options.parallax = *parallax;
        if (blendMode) options.blendMode = *blendMode;
        if (texture)   options.texture = *texture;
        if (shader)    options.shader = shader;
        setNodeOptions(handle, options);
    }

    auto RenderSystem::setNodeDepth(NodeHandle handle, int depth) -> void
    {
        ASSURE_VALID(handle);
        _nodes[handle].depth = depth;
        _orderdirty = true;
    }

    auto RenderSystem::setNodeLayer(NodeHandle handle, LayerHandle layer) -> void
    {
        ASSURE_VALID(handle);

        if (!_layers.isValid(layer))
        {
            LOG_WARN("Assigning invalid layer");
            return;
        }

        _nodes[handle].layer = layer;
        _orderdirty = true;
    }

    auto RenderSystem::setNodeTransform(NodeHandle handle, const sf::Transform& transform) -> void
    {
        ASSURE_VALID(handle);
        _nodes[handle].transform = transform;
        _markBBoxDirty(handle);
    }


    auto RenderSystem::createNodeMesh(NodeHandle handle, size_t size, sf::PrimitiveType type) -> void
    {
        ASSURE_VALID(handle);

        RenderNode& node = _nodes[handle];

        // Add to queue if there was no mesh yet
        if (!node.mesh.handle.isValid())
        {
            _renderqueue.push_back(handle);
            _orderdirty = true;
        }

        _freeMesh(handle);
        node.mesh.handle = _vertices.allocate(size);
        node.mesh.primitiveType = type;
        node.mesh.size = size;
        // don't update bbox, because there is no vertex data yet
    }

    auto RenderSystem::getNodeMesh(NodeHandle handle, size_t offset) const -> const sf::Vertex*
    {
        ASSURE_VALID_RET(handle, nullptr);
        CHECK_MESH_BOUNDS_RET(handle, offset, nullptr);
        return _vertices.get(_nodes[handle].mesh.handle.index + offset);
    }

    auto RenderSystem::updateNodeMesh(
            NodeHandle handle, size_t size, size_t offset,
            const sf::Vector2f* vertices, const sf::Vector2f* uvs, const sf::Color* colors,
            bool updateSize)
        -> void
    {
        ASSURE_VALID(handle);
        CHECK_MESH_BOUNDS(handle, 0);
        size_t stop = offset + size;
        Mesh& mesh = _nodes[handle].mesh;

        if (mesh.handle.size < stop)
            LOG_WARN("Trying to assign more vertices than space allocated -> Clipping to maximum");

        stop = std::min(stop, mesh.handle.size);
        bool sizechanged = updateSize && stop != mesh.size;

        if (sizechanged)
            mesh.size = stop;

        for (size_t i = offset; i < stop; ++i)
        {
            sf::Vertex& v = *_vertices.get(mesh.handle.index + i);

            if (vertices) v.position  = vertices[i - offset];
            if (uvs)      v.texCoords = uvs[i - offset];
            if (colors)   v.color     = colors[i - offset];
        }

        if (vertices || sizechanged)
            _updateMeshBBox(handle);
    }

    auto RenderSystem::setNodeMeshType(NodeHandle handle, sf::PrimitiveType type) -> void
    {
        ASSURE_VALID(handle);
        _nodes[handle].mesh.primitiveType = type;
    }

    auto RenderSystem::setNodeMeshSize(NodeHandle handle, size_t size) -> void
    {
        ASSURE_VALID(handle);
        CHECK_MESH_BOUNDS(handle, size == 0 ? 0 : size - 1);
        _nodes[handle].mesh.size = size;
        _updateMeshBBox(handle);
    }


    auto RenderSystem::setRootOptions(const RenderOptions& options) -> void
    {
        _root = options;
    }

    auto RenderSystem::setRootOptions(
            const unsigned int* flags,
            const float* parallax,
            const sf::BlendMode* blendMode,
            const TextureResource::Handle* texture,
            const sf::Shader* shader)
        -> void
    {
        RenderOptions options = _root;
        if (flags)     options.flags = *flags;
        if (parallax)  options.parallax = *parallax;
        if (blendMode) options.blendMode = *blendMode;
        if (texture)   options.texture = *texture;
        if (shader)    options.shader = shader;
        setRootOptions(options);
    }

    auto RenderSystem::getRootOptions() const -> const RenderOptions&
    {
        return _root;
    }


	auto RenderSystem::getNumObjectsRendered() const -> size_t
	{
        return _numrendered;
	}

    auto RenderSystem::forceUpdate() const -> void
    {
        // Should be safe, because why would you instantiate a const RenderSystem?
        // Easier than to add mutable and const on half of the members.
        RenderSystem* self = const_cast<RenderSystem*>(this);
        self->_updateDirty();
        self->_updateQueue();
    }

    auto RenderSystem::render(sf::RenderTarget& target, const math::AABBf& rect) const -> size_t
    {
        return render(target, &rect);
    }

    auto RenderSystem::render(sf::RenderTarget& target, const math::AABBf* rect) const -> size_t
    {
        _numrendered = 0;

        if (_root.flags & render_invisible)
            return 0;

        forceUpdate();

        RenderOptions parent = _root;
        LayerHandle currentlayer;

        for (NodeHandle handle : _renderqueue)
        {
            const RenderNode& node = _nodes[handle];
            const Mesh& mesh = node.mesh;

            if (mesh.size == 0)
                continue;

            if (mesh.size == 1 && mesh.primitiveType != sf::Points)
                continue;

            if (mesh.size == 2 && mesh.primitiveType != sf::Points
                    && mesh.primitiveType != sf::Lines
                    && mesh.primitiveType != sf::LineStrip)
                continue;

            // Update current layer
            if (node.layer != currentlayer)
            {
                auto layerptr = _layers.get(node.layer);
                parent = !layerptr ? _root : layerptr->options.inherited(_root);
                currentlayer = node.layer;
            }

            RenderOptions options = node.options.inherited(parent);

            if (!options.isVisible())
                continue;

            sf::Transform trans;    // parallax transform
            math::AABBf bbox = node._globalBBox;
            float parallax = options.parallax;

            if (!(options.flags & render_noparallax) && !math::almostEquals(parallax, 1.0f))
            {
                // If a camera is given, scale around camera center, otherwise,
                // when rendering the whole scene, scale around object center.
                math::Point2f vcenter = (rect ? rect : &bbox)->getCenter();
                math::Vec2f translate = (bbox.getCenter() - vcenter) * (parallax - 1);
                bbox.pos += translate;

                if (options.flags & render_scaleparallax)
                {
                    trans.scale(parallax, parallax, vcenter.x, vcenter.y);
                    bbox.extend(bbox.size * parallax - bbox.size);
                }
                else
                    trans.translate(translate.x, translate.y);

                trans *= node.transform;

                if (renderBoxes)
                {
                    sf::RectangleShape noderect(convert(bbox.size));
                    noderect.setFillColor(sf::Color::Transparent);
                    noderect.setOutlineColor(sf::Color::White);
                    noderect.setOutlineThickness(1);
                    noderect.setPosition(convert(bbox.pos));
                    target.draw(noderect);
                }
            }
            else
                trans = node.transform;

            if (bbox.w == 0 || bbox.h == 0)
                LOG_WARN("RenderNode bounding box has 0 width or height: ", bbox.w, "x", bbox.h);

            // culling by bbox check
            if (rect && !math::intersect(*rect, bbox))
                continue;

            // TODO: wireframe

            target.draw(
                    _vertices.get(mesh.handle.index), mesh.size, mesh.primitiveType,
                    sf::RenderStates(options.blendMode, trans, options.texture.get(), options.shader));

            ++_numrendered;
        }

        return _numrendered;
    }

    auto RenderSystem::getNodeAtPosition(const math::Point2f& pos) const -> NodeHandle
    {
        forceUpdate();

        for (auto it = _renderqueue.rbegin(), end = _renderqueue.rend(); it != end; ++it)
        {
            NodeHandle handle = *it;

            if (!getNodeVisible(handle))
                continue;

            const RenderNode* node = getNode(handle);
            VertexPointSet pset(_vertices.get(node->mesh.handle.index), node);
            bool hit = false;

            switch (node->mesh.primitiveType)
            {
                case sf::Triangles:
                    hit = math::intersectTriangles(pos, pset);
                    break;
                case sf::TriangleStrip:
                    hit = math::intersectTriangleStrip(pos, pset);
                    break;
                case sf::TriangleFan:
                    hit = math::intersectTriangleFan(pos, pset);
                    break;
                case sf::Quads:
                    hit = math::intersectQuads(pos, pset);
                    break;
                default:
                    break;
            }

            if (hit)
                return handle;
        }

        return NodeHandle();
    }

    auto RenderSystem::createLayer(const std::string& name) -> LayerHandle
    {
        auto hnd = findLayer(name);
        if (hnd.isNull())
        {
            hnd = _layers.acquire();
            _layers[hnd] = RenderLayer();
            _layers[hnd].name = name;
            LOG_DEBUG("Created RenderLayer");
        }
        else
            LOG_DEBUG_WARN("Layer already exists: ", name, " -> using existing one");

        return hnd;
    }

    auto RenderSystem::removeLayer(LayerHandle handle) -> void
    {
        ASSURE_LAYER_VALID(handle);
        _layers.destroy(handle);
    }

    auto RenderSystem::getLayer(LayerHandle handle) const -> const RenderLayer*
    {
        return _layers.get(handle);
    }

    auto RenderSystem::beginLayers() const -> LayerCollection::const_iterator
    {
        return _layers.begin();
    }

    auto RenderSystem::endLayers() const -> LayerCollection::const_iterator
    {
        return _layers.end();
    }


    auto RenderSystem::findLayer(const std::string& name) const -> LayerHandle
    {
        for (auto it = _layers.begin(), end = _layers.end(); it != end; ++it)
            if (it->name == name)
                return it.handle();
        return LayerHandle();
    }

    auto RenderSystem::setLayerName(LayerHandle handle, const std::string& name) -> bool
    {
        ASSURE_LAYER_VALID_RET(handle, false);

        if (findLayer(name))
        {
            LOG_ERROR("A layer with that name already exists");
            return false;
        }

        _layers[handle].name = name;
        return true;
    }

    auto RenderSystem::setLayerDepth(LayerHandle handle, int depth) -> void
    {
        ASSURE_LAYER_VALID(handle);
        _layers[handle].depth = depth;
        _orderdirty = true;
    }

    auto RenderSystem::setLayerOptions(LayerHandle handle, const RenderOptions& options) -> void
    {
        ASSURE_LAYER_VALID(handle);
        _layers[handle].options = options;
    }

    auto RenderSystem::setLayerOptions(
            LayerHandle handle,
            const unsigned int* flags,
            const float* parallax,
            const sf::BlendMode* blendMode,
            const TextureResource::Handle* texture,
            const sf::Shader* shader)
        -> void
    {
        ASSURE_LAYER_VALID(handle);
        RenderOptions options = _layers[handle].options;
        if (flags)     options.flags = *flags;
        if (parallax)  options.parallax = *parallax;
        if (blendMode) options.blendMode = *blendMode;
        if (texture)   options.texture = *texture;
        if (shader)    options.shader = shader;
        setLayerOptions(handle, options);
    }


    auto RenderSystem::_updateDirty() -> void
    {
        if (_dirtylist.empty())
            return;

        // LOG_DEBUG("Recalculating global node transforms and bboxs: ", _dirtylist.size());

        for (NodeHandle handle : _dirtylist)
            if (_nodes.isValid(handle))
                _updateNodeGlobalBBox(handle);

        _dirtylist.clear();
    }

    auto RenderSystem::_updateNodeGlobalBBox(NodeHandle handle) const -> void
    {
        const RenderNode& node = _nodes[handle];

        if (!node._bboxdirty)
            return;

        if (node.mesh.size == 0)
            node._globalBBox = math::AABBf();
        else
            node._globalBBox = convert(
                    node.transform.transformRect(convert(node.mesh.bbox)));

        node._bboxdirty = false;
    }

    auto RenderSystem::_updateMeshBBox(NodeHandle handle) -> void
    {
        RenderNode& node = _nodes[handle];
        Mesh& mesh = node.mesh;
        mesh.bbox = calculateBBox(_vertices.get(mesh.handle.index), mesh.size);
        _markBBoxDirty(handle);
    }

    auto RenderSystem::_freeMesh(NodeHandle handle) -> void
    {
        ASSURE_VALID(handle);

        Mesh& mesh = _nodes[handle].mesh;

        if (mesh.handle.isValid())
        {
            LOG_DEBUG_WARN("Freeing mesh");
            _vertices.free(mesh.handle);
            mesh = Mesh();
            _nodes[handle]._globalBBox = math::AABBf();
            _nodes[handle]._bboxdirty = false;
        }
    }

    auto RenderSystem::_markBBoxDirty(NodeHandle handle) -> void
    {
        RenderNode& node = _nodes[handle];
        if (!node._bboxdirty)
        {
            node._bboxdirty = true;
            _dirtylist.push_back(handle);
        }
    }

	auto RenderSystem::_updateQueue() -> void
	{
        if (!_orderdirty)
            return;

        LOG_DEBUG("RenderSystem content changed -> sorting");

        auto begin = _renderqueue.begin(),
             end = _renderqueue.end();

        std::stable_sort(begin, end, [this](NodeHandle a, NodeHandle b) {
            const RenderNode *nodea = _nodes.get(a),
                            *nodeb = _nodes.get(b);

            if (!nodea)
                return false;
            if (!nodeb)
                return true;

            const RenderLayer *layera = getLayer(nodea->layer),
                              *layerb = getLayer(nodeb->layer);
            const int deptha = layera ? layera->depth : 0,
                      depthb = layerb ? layerb->depth : 0;

            if (deptha != depthb)
                return deptha > depthb;
            return nodea->depth > nodeb->depth;
        });

        auto it = std::find_if(begin, end, [this](NodeHandle handle) { return !_nodes.isValid(handle); });

        if (it != end)
        {
            LOG_DEBUG_WARN("Removing ", std::distance(it, end), " deleted nodes from queue");
            _renderqueue.erase(it, end);
        }

        _orderdirty = false;
	}

    auto RenderSystem::loadFromJson(const Json::Value& node) -> bool
    {
        // TODO: load root options

        if (node.isMember("layers"))
        {
            const auto& layers = node["layers"];
            for (auto it = layers.begin(), end = layers.end(); it != end; ++it)
            {
                auto h = createLayer(it.key().asString());

                if (!::gamelib::loadFromJson(*it, &_layers[h]))
                    // Don't remove because code might rely on it
                    LOG_ERROR("Couldn't load config for layer: ", getLayer(h)->name);
            }
        }

        _updateQueue();
        return true;
    }

    auto RenderSystem::writeToJson(Json::Value& node) const -> void
    {
        // TODO: write root options

        if (_layers.begin() != _layers.end())
        {
            auto& layers = node["layers"];
            for (const auto& i : _layers)
                ::gamelib::writeToJson(layers[i.name], i);
        }
    }

    auto RenderSystem::begin() const -> RenderQueue::const_iterator
    {
        return _renderqueue.begin();
    }

    auto RenderSystem::end() const -> RenderQueue::const_iterator
    {
        return _renderqueue.end();
    }
}
