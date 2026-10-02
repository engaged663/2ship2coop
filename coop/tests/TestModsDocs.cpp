// The generated reference is complete and the copy in the repository is the current one; the example works.
#include "TestMods.h"

#include "server/Mods/ModApi.h"
#include "server/Mods/ModDocs.h"
#include "server/Mods/ModEvents.h"

#include <sstream>

using namespace coop;
using namespace coop_test;

namespace {

std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream out;
    out << f.rdbuf();
    std::string text = out.str();
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    return text;
}

} // namespace

TEST_CASE(ModDocsNameEveryFunctionAndEvent) {
    std::string md = server::ModApiMarkdown();
    for (const server::ApiDef* def : server::AllApis()) {
        CHECK(md.find("`coop." + std::string(def->name) + "(") != std::string::npos);
    }
    for (const auto& ev : server::ModEventDefs()) {
        CHECK(md.find("### `" + std::string(ev.name) + "`") != std::string::npos);
    }
    for (const char* special : { "coop.on(", "coop.off(", "coop.emit(", "coop.timer.after(", "coop.timer.every(",
                                 "coop.timer.cancel(", "coop.commands.register(", "coop.commands.unregister(" }) {
        CHECK(md.find(special) != std::string::npos);
    }
    std::string ids = server::ModIdsMarkdown();
    CHECK(ids.find("MASK_BUNNY") != std::string::npos);
    CHECK(ids.find("EN_DODONGO") != std::string::npos);
    CHECK(ids.find("SOUTH_CLOCK_TOWN") != std::string::npos);
}

TEST_CASE(ModApiReferenceInTheRepositoryIsUpToDate) {
    std::filesystem::path docs = std::filesystem::path(COOP_SOURCE_DIR) / "docs" / "mods";
    if (!std::filesystem::exists(docs / "API.md")) {
        std::printf("    (skipped: %s not found)\n", (docs / "API.md").string().c_str());
        return;
    }
    // If this fails: build/coop/Release/2ship-coop-server.exe --mod-docs coop/docs/mods
    CHECK(ReadAll(docs / "API.md") == server::ModApiMarkdown());
    CHECK(ReadAll(docs / "IDS.md") == server::ModIdsMarkdown());
}

TEST_CASE(ExampleScriptLoadsAndItsCommandsWork) {
    std::filesystem::path example = std::filesystem::path(COOP_SOURCE_DIR) / "mods" / "ejemplo.lua";
    if (!std::filesystem::exists(example)) {
        std::printf("    (skipped: %s not found)\n", example.string().c_str());
        return;
    }
    server::ServerConfig cfg;
    cfg.mods.settings = { { "ejemplo", { { "consejosCadaMinutos", 0 } } } };
    ModServer m("coop_docs_example", cfg);
    std::string err;
    CHECK(m.Mods().LoadFile(example.string(), &err));
    CHECK(err.empty());
    auto a = Join(m.s, "Alice");
    CHECK(a->WaitFor("sys", m.s).has_value()); // the welcome
    CreateWorld(m.s, *a);
    Drain(m.s, { a.get() });
    a->Cmd("/curar");
    auto heal = a->WaitFor("mod", m.s);
    CHECK(heal.has_value());
    CHECK_EQ((*heal)["ops"][0]["op"].get<std::string>(), std::string("heal"));
    a->Cmd("/dado 6");
    CHECK(a->WaitFor("sys", m.s).has_value());
    a->Send({ { "t", "gev" }, { "k", "death" } });
    m.s.PumpFor(100);
    a->Cmd("/muertes");
    bool counted = a->WaitUntil(m.s, 1000, [&] {
        while (auto ev = a->TakeEvent("sys")) {
            if (std::string((*ev)["text"]).find("Alice: 1") != std::string::npos) {
                return true;
            }
        }
        return false;
    });
    CHECK(counted);
    CHECK(!m.Logged("error")); // nothing of the example failed along the way
}
