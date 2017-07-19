#include "gamelib/res/ResourceManager.hpp"
#include "gamelib/utils/log.hpp"
#include "gamelib/utils/string.hpp"

namespace gamelib
{
    ResourceManager::ResourceManager(EventManager* evmgr) :
        _evmgr(evmgr)
    { }

    ResourceManager::ResourceManager(EventManager* evmgr, const std::string& searchpath) :
        _evmgr(evmgr)
    {
        setSearchpath(searchpath);
    }

    bool ResourceManager::loadFromJson(const Json::Value& node)
    {
        if (node.isMember("searchpath"))
            setSearchpath(node["searchpath"].asString());

        bool reload = node.get("forcereload", false).asBool();

        if (node.isMember("preload"))
        {
            auto& flist = node["preload"];

            if (reload)
            {
                clean();
                for (auto& fname : flist)
                    load(fname.asString());
            }
            else
            {
                // Store references to prevent cleanup of loaded resources
                std::vector<BaseResourceHandle> res;
                res.reserve(flist.size());
                for (auto& fname : flist)
                    res.push_back(get(fname.asString()));
                clean();
            }
        }

        return true;
    }

    void ResourceManager::writeToJson(Json::Value& node)
    {
        node["searchpath"] = _searchpath;
        node["forcereload"] = false;

        auto& files = node["preload"];
        files.resize(_res.size());

        Json::ArrayIndex i = 0;
        for (auto& it : _res)
            files[i++] = it.first;
    }

    BaseResourceHandle ResourceManager::load(const std::string& fname)
    {
        LOG_DEBUG("Loading file ", fname, "...");

        auto pos = fname.find_last_of('.');
        if (pos == std::string::npos)
        {
            LOG_ERROR("File has no extension: ", fname);
            return nullptr;
        }

        auto ext = fname.substr(pos + 1);
        auto it = _typemap.find(ext);
        if (it == _typemap.end())
        {
            LOG_ERROR("Unknown resource type: ", fname);
            return nullptr;
        }

        auto path = _searchpath + fname;
        adaptPath(path);

        // Call the associated loader
        auto res = it->second(path, this);
        if (!res)
        {
            LOG_ERROR("Failed to load resource ", path);
            return nullptr;
        }

        // Fire a reload event if the resource was reloaded
        if (_evmgr)
        {
            auto oldres = find(fname);
            if (oldres && oldres.use_count() > 1)
                if (_evmgr)
                    _evmgr->queueEvent(ResourceReloadEvent::create(fname, res));
        }

        _res[fname] = res;
        return res;
    }

    BaseResourceHandle ResourceManager::get(const std::string& fname)
    {
        auto ptr = find(fname);
        if (ptr)
            return ptr;
        return load(fname);
    }

    BaseResourceHandle ResourceManager::find(const std::string& fname)
    {
        auto it = _res.find(fname);
        if (it == _res.end())
            return nullptr;
        return it->second;
    }

    void ResourceManager::registerFileType(const std::string& ext, LoaderCallback cb)
    {
        _typemap[ext] = cb;
        LOG_DEBUG("Registered filetype ", ext);
    }

    void ResourceManager::setSearchpath(const std::string& path)
    {
        _searchpath = path;
        if (_searchpath.back() != '/' && _searchpath.back() != '\\')
            _searchpath.push_back('/');
        adaptPath(_searchpath);
    }

    void ResourceManager::clean()
    {
        LOG_DEBUG_WARN("Freeing unreferenced resources");
        for (auto it = _res.begin(); it != _res.end();)
        {
            if (it->second.use_count() == 1)
            {
                LOG_DEBUG_WARN("\t", it->first);
                it = _res.erase(it);
            }
            else
                ++it;
        }
    }

    void ResourceManager::clear()
    {
        _res.clear();
        LOG_DEBUG_WARN("Freeing all resources");
    }

    void ResourceManager::destroy()
    {
        _res.clear();
        _typemap.clear();
        _searchpath.clear();
        LOG_DEBUG_WARN("ResourceManager destroyed");
    }



    ResourceReloadEvent::ResourceReloadEvent(const std::string& fname_, BaseResourceHandle ptr) :
        fname(fname_),
        res(ptr)
    { }
}