#include "GameIds.h"

#include "common/Text.h"

#include <cmath>
#include <map>
#include <vector>

namespace coop::ids {

namespace {

struct Entry {
    const char* name;
    int id;
    const char* title;  // "" for items
    const char* decomp; // a scene's name in the decomp, "" for the rest
    int entranceScene;  // scenes only
};

// The same list three times, each one keeping its own kind of line.
const Entry kItems[] = {
#define COOP_ITEM(name, id) { #name, id, "", "", -1 },
#define COOP_ACTOR(name, id, title)
#define COOP_SCENE(key, decompName, sceneId, entranceScene, title)
#include "common/GameIds.inc"
#undef COOP_ITEM
#undef COOP_ACTOR
#undef COOP_SCENE
};

const Entry kActors[] = {
#define COOP_ITEM(name, id)
#define COOP_ACTOR(name, id, title) { #name, id, title, "", -1 },
#define COOP_SCENE(key, decompName, sceneId, entranceScene, title)
#include "common/GameIds.inc"
#undef COOP_ITEM
#undef COOP_ACTOR
#undef COOP_SCENE
};

const Entry kScenes[] = {
#define COOP_ITEM(name, id)
#define COOP_ACTOR(name, id, title)
#define COOP_SCENE(key, decompName, sceneId, entranceScene, title) { #key, sceneId, title, #decompName, entranceScene },
#include "common/GameIds.inc"
#undef COOP_ITEM
#undef COOP_ACTOR
#undef COOP_SCENE
};

std::string Upper(std::string text) {
    for (char& c : text) {
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }
    }
    return text;
}

// One kind's entries with their two indexes, built on first use.
struct Book {
    std::vector<const Entry*> entries; // in the game's order
    const char* prefix;                // what the game's headers put before each name
    std::map<std::string, const Entry*> byName;
    std::map<int, const Entry*> byId;

    template <size_t N> Book(const Entry (&list)[N], const char* namePrefix) : prefix(namePrefix) {
        for (const Entry& e : list) {
            entries.push_back(&e);
            byName[e.name] = &e;
            byId[e.id] = &e;
        }
        for (const Entry& e : list) { // the decomp's names never hide a name of the list itself
            if (e.decomp[0] != '\0') {
                byName.emplace(e.decomp, &e);
            }
        }
    }

    const Entry* ByName(const std::string& text) const {
        std::string name = Upper(text);
        auto it = byName.find(name);
        if (it == byName.end() && name.rfind(prefix, 0) == 0) {
            it = byName.find(name.substr(std::char_traits<char>::length(prefix)));
        }
        return it != byName.end() ? it->second : nullptr;
    }

    const Entry* ById(int id) const {
        auto it = byId.find(id);
        return it != byId.end() ? it->second : nullptr;
    }
};

const Book& BookOf(Kind kind) {
    static const Book items(kItems, "ITEM_");
    static const Book actors(kActors, "ACTOR_");
    static const Book scenes(kScenes, "SCENE_");
    return kind == Kind::Item ? items : kind == Kind::Actor ? actors : scenes;
}

} // namespace

bool ParseKind(const std::string& text, Kind& out) {
    std::string kind = ToLower(text);
    if (kind == "item") {
        out = Kind::Item;
    } else if (kind == "actor") {
        out = Kind::Actor;
    } else if (kind == "scene") {
        out = Kind::Scene;
    } else {
        return false;
    }
    return true;
}

int Find(Kind kind, const json& nameOrId) {
    const Book& book = BookOf(kind);
    const Entry* entry = nullptr;
    if (nameOrId.is_string()) {
        entry = book.ByName(nameOrId.get_ref<const std::string&>());
    } else if (nameOrId.is_number_integer()) {
        int64_t id = nameOrId.get<int64_t>();
        entry = (id >= 0 && id <= 0xFFFF) ? book.ById((int)id) : nullptr;
    } else if (nameOrId.is_number_float()) { // 114 / 2 is 57.0 in Lua
        double id = nameOrId.get<double>();
        entry = (id >= 0.0 && id <= 65535.0 && std::floor(id) == id) ? book.ById((int)id) : nullptr;
    }
    return entry != nullptr ? entry->id : -1;
}

std::string NameOf(Kind kind, int id) {
    const Entry* entry = BookOf(kind).ById(id);
    return entry != nullptr ? entry->name : "";
}

std::string TitleOf(Kind kind, int id) {
    const Entry* entry = BookOf(kind).ById(id);
    return entry != nullptr ? entry->title : "";
}

int EntranceSceneOf(int sceneId) {
    const Entry* entry = BookOf(Kind::Scene).ById(sceneId);
    return entry != nullptr ? entry->entranceScene : -1;
}

json Table(Kind kind) {
    json out = json::object();
    for (const Entry* e : BookOf(kind).entries) {
        out[e->name] = e->id;
    }
    return out;
}

} // namespace coop::ids
