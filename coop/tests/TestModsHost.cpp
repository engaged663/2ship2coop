// The mod host on its own: the event table, subscriptions, Fire, timers and deferred work (no Lua here).
#include "TestMods.h"

#include "server/Mods/ModEvents.h"

#include <set>
#include <thread>

using namespace coop;
using namespace coop_test;
using server::ModEvent;

TEST_CASE(ModEventTableIsWellFormed) {
    std::set<std::string> names;
    for (const auto& def : server::ModEventDefs()) {
        CHECK(names.insert(def.name).second);
        CHECK(std::string(def.doc).size() > 10);
        ModEvent found;
        CHECK(server::FindModEvent(def.name, found));
        CHECK(found == def.id);
        std::set<std::string> fields;
        for (const auto& f : def.fields) {
            CHECK(fields.insert(f.name).second);
            CHECK(f.type == "int" || f.type == "number" || f.type == "string" || f.type == "bool" ||
                  f.type == "list" || f.type == "table");
            CHECK(!f.doc.empty());
        }
    }
    CHECK_EQ(names.size(), (size_t)ModEvent::Count);
    CHECK(server::ModEventInfo(ModEvent::Chat).cancelable);
    CHECK(!server::ModEventInfo(ModEvent::PlayerJoin).cancelable);
    CHECK_EQ(server::ModEventInfo(ModEvent::Chat).fields.size(), (size_t)3);
    CHECK(server::ModEventInfo(ModEvent::Chat).fields[2].writable);
    CHECK(!server::ModEventInfo(ModEvent::Chat).fields[0].writable);
}

TEST_CASE(ModFireRunsHandlersInOrderAndStopsWhenCancelled) {
    TestServer s;
    auto& host = s.server->Mods();
    FakeMod* a = AddFake(s, "a");
    FakeMod* b = AddFake(s, "b");
    std::string order;
    a->onEvent = [&](int handler, json& e, bool*) {
        order += "a" + std::to_string(handler);
        e["text"] = "cambiado";
    };
    b->onEvent = [&](int, json&, bool* cancel) {
        order += "b";
        *cancel = true;
    };
    std::string err;
    CHECK(!host.Wants(ModEvent::Chat));
    CHECK(host.Subscribe(*a, "chat", 1, &err) != 0);
    CHECK(host.Subscribe(*b, "chat", 1, &err) != 0);
    CHECK(host.Subscribe(*a, "chat", 2, &err) != 0); // never reached: b cancels first
    CHECK(host.Wants(ModEvent::Chat));
    CHECK(!host.Wants(ModEvent::PlayerJoin));
    json e = { { "player", 1 }, { "nick", "Ana" }, { "text", "hola" } };
    CHECK(!host.Fire(ModEvent::Chat, e));
    CHECK_EQ(order, std::string("a1b"));
    CHECK_EQ(e["text"].get<std::string>(), std::string("cambiado"));
}

TEST_CASE(ModFireKeepsOnlyWritableFieldsOfTheRightType) {
    TestServer s;
    auto& host = s.server->Mods();
    FakeMod* a = AddFake(s, "a");
    int calls = 0;
    a->onEvent = [&](int, json& e, bool* cancel) {
        calls++;
        e["nick"] = "Otro";  // not writable
        e["player"] = 99;    // not writable
        e["extra"] = true;   // not a field
        e["text"] = calls == 1 ? json(5) : json("ok"); // wrong type the first time
        *cancel = true;      // player_join cannot be cancelled
    };
    std::string err;
    CHECK(host.Subscribe(*a, "chat", 1, &err) != 0);
    CHECK(host.Subscribe(*a, "player_join", 2, &err) != 0);
    json j = { { "player", 1 }, { "nick", "Ana" }, { "ip", "1.2.3.4" } };
    CHECK(host.Fire(ModEvent::PlayerJoin, j)); // calls == 1: its cancel is ignored
    CHECK_EQ(j["nick"].get<std::string>(), std::string("Ana"));
    CHECK(!j.contains("text"));
    json e = { { "player", 1 }, { "nick", "Ana" }, { "text", "hola" } };
    CHECK(!host.Fire(ModEvent::Chat, e)); // calls == 2
    CHECK_EQ(e["nick"].get<std::string>(), std::string("Ana"));
    CHECK_EQ(e["player"].get<int>(), 1);
    CHECK(!e.contains("extra"));
    CHECK_EQ(e["text"].get<std::string>(), std::string("ok"));
    calls = 0;
    json wrong = { { "player", 1 }, { "nick", "Ana" }, { "text", "hola" } };
    host.Fire(ModEvent::Chat, wrong); // calls == 1: a number is not a text
    CHECK_EQ(wrong["text"].get<std::string>(), std::string("hola"));
}

TEST_CASE(ModUnknownEventsAreRefusedAndCustomOnesWork) {
    TestServer s;
    auto& host = s.server->Mods();
    FakeMod* a = AddFake(s, "a");
    std::string err;
    CHECK_EQ(host.Subscribe(*a, "chatt", 1, &err), (uint32_t)0);
    CHECK(!err.empty());
    int seen = 0;
    a->onEvent = [&](int, json& e, bool* cancel) {
        seen++;
        e["respuesta"] = 42; // everything is writable in an event between mods
        *cancel = true;
    };
    CHECK(host.Subscribe(*a, "economia:pago", 1, &err) != 0);
    json e = { { "cantidad", 10 } };
    CHECK(!host.FireCustom("economia:pago", e));
    CHECK_EQ(seen, 1);
    CHECK_EQ(e["respuesta"].get<int>(), 42);
    json other = json::object();
    CHECK(host.FireCustom("nadie:escucha", other));
}

TEST_CASE(ModUnsubscribeAndUnloadStopTheCalls) {
    TestServer s;
    auto& host = s.server->Mods();
    FakeMod* a = AddFake(s, "a");
    FakeMod* b = AddFake(s, "b");
    CHECK(AddFake(s, "A") == nullptr); // names are unique whatever the case
    int calls = 0;
    a->onEvent = [&](int, json&, bool*) { calls++; };
    b->onEvent = [&](int, json&, bool*) { calls += 10; };
    std::string err;
    uint32_t sub = host.Subscribe(*a, "player_join", 7, &err);
    host.Subscribe(*b, "player_join", 8, &err);
    json e = { { "player", 1 }, { "nick", "Ana" }, { "ip", "" } };
    host.Fire(ModEvent::PlayerJoin, e);
    CHECK_EQ(calls, 11);
    CHECK(!host.Unsubscribe(*b, sub)); // not its subscription
    CHECK(host.Unsubscribe(*a, sub));
    CHECK_EQ(a->released.size(), (size_t)1);
    CHECK_EQ(a->released[0], 7);
    host.Fire(ModEvent::PlayerJoin, e);
    CHECK_EQ(calls, 21);
    CHECK(host.Unload("B", &err)); // b is deleted here
    CHECK(host.Find("b") == nullptr);
    CHECK(!host.Wants(ModEvent::PlayerJoin));
    CHECK(!host.Unload("nadie", &err));
    CHECK(!err.empty());
    CHECK_EQ(host.All().size(), (size_t)1);
}

TEST_CASE(ModTimersFireOnceOrRepeatAndCanBeCancelled) {
    TestServer s;
    auto& host = s.server->Mods();
    FakeMod* a = AddFake(s, "a");
    uint32_t once = host.AddTimer(*a, 1, 20, false);
    uint32_t every = host.AddTimer(*a, 2, 20, true);
    uint32_t never = host.AddTimer(*a, 3, 20, false);
    CHECK(once != 0 && every != 0 && never != 0);
    CHECK(host.CancelTimer(*a, never));
    s.PumpFor(120);
    int ones = 0;
    int twos = 0;
    for (int h : a->timers) {
        ones += h == 1;
        twos += h == 2;
        CHECK(h != 3);
    }
    CHECK_EQ(ones, 1);
    CHECK(twos >= 3);
    CHECK(host.CancelTimer(*a, every));
    CHECK(!host.CancelTimer(*a, once)); // already gone
    size_t count = a->timers.size();
    s.PumpFor(60);
    CHECK_EQ(a->timers.size(), count);
}

TEST_CASE(ModHandlerErrorsAreLoggedAndThenSilenced) {
    TestServer s;
    auto& host = s.server->Mods();
    FakeMod* a = AddFake(s, "ruidoso");
    a->onTimer = [](int) { return false; };
    host.AddTimer(*a, 1, 10, true);
    s.PumpFor(400);
    int logged = 0;
    for (const std::string& line : s.log.Lines()) {
        logged += line.find("boom") != std::string::npos;
    }
    CHECK(a->timers.size() > 20); // it keeps running
    CHECK_EQ(logged, 20);         // but stops filling the log
    CHECK(Logged(s, "[ruidoso]"));
}

TEST_CASE(ModNestedEventsStopAtTheDepthLimit) {
    TestServer s;
    auto& host = s.server->Mods();
    FakeMod* a = AddFake(s, "a");
    int calls = 0;
    a->onEvent = [&](int, json&, bool*) {
        calls++;
        json again = json::object();
        host.FireCustom("bucle:x", again); // an event that fires itself
    };
    std::string err;
    host.Subscribe(*a, "bucle:x", 1, &err);
    json e = json::object();
    CHECK(host.FireCustom("bucle:x", e));
    CHECK_EQ(calls, 8);
}

TEST_CASE(ModUnloadFromInsideAHandlerWaitsForTheNextTick) {
    TestServer s;
    auto& host = s.server->Mods();
    FakeMod* a = AddFake(s, "a");
    std::string err;
    bool asked = false;
    a->onEvent = [&](int, json&, bool*) { asked = host.Unload("a", &err); };
    host.Subscribe(*a, "prueba:x", 1, &err);
    json e = json::object();
    host.FireCustom("prueba:x", e);
    CHECK(asked);
    CHECK(host.Find("a") != nullptr); // still here: its handler was running
    s.Pump();
    CHECK(host.Find("a") == nullptr);
}

TEST_CASE(ModDeferRunsOnTheServerThread) {
    TestServer s;
    auto& host = s.server->Mods();
    std::thread::id ranOn;
    std::thread worker([&] { host.Defer([&] { ranOn = std::this_thread::get_id(); }); });
    worker.join();
    CHECK(ranOn == std::thread::id());
    s.Pump();
    CHECK(ranOn == std::this_thread::get_id());
}

TEST_CASE(ModsHearTheServerStop) {
    TestServer s;
    auto& host = s.server->Mods();
    FakeMod* a = AddFake(s, "a");
    std::string reason;
    a->onEvent = [&](int, json& e, bool*) { reason = e.value("reason", ""); };
    std::string err;
    host.Subscribe(*a, "server_stop", 1, &err);
    s.server->Stop("mantenimiento");
    CHECK_EQ(reason, std::string("mantenimiento"));
}
