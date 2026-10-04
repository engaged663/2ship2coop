// The game mods (.o2r) a server shares: SHA-256, the list, the chunks, the download plan, the server's store and the
// whole download through a real server (docs/superpowers/specs/2026-10-03-coop-mods-o2r-design.md).
#include "TestWorld.h"

#include "common/O2r.h"
#include "common/Sha256.h"
#include "common/StreamIds.h"
#include "server/O2rStore.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <fstream>
#include <map>

using namespace coop;
using namespace coop_test;

namespace {

std::string Hex(const std::string& text) {
    return Sha256Hex(text.data(), text.size());
}

void WriteBytes(const std::filesystem::path& file, const std::string& bytes) {
    std::ofstream out(file, std::ios::binary);
    out.write(bytes.data(), (std::streamsize)bytes.size());
}

o2r::Entry EntryOf(const std::string& name, const std::string& bytes) {
    return { name, bytes.size(), Hex(bytes) };
}

} // namespace

TEST_CASE(Sha256MatchesTheNistVectors) {
    CHECK_EQ(Hex(""), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    CHECK_EQ(Hex("abc"), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK_EQ(Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
             std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    // A million 'a' in uneven pieces: the result must not depend on how the bytes arrive.
    Sha256 h;
    std::string a(1000000, 'a');
    size_t at = 0;
    size_t step = 1;
    while (at < a.size()) {
        size_t n = std::min(step, a.size() - at);
        h.Update(a.data() + at, n);
        at += n;
        step = step * 7 % 997 + 1;
    }
    CHECK_EQ(h.FinalHex(), std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

TEST_CASE(Sha256OfAFileIsTheOneOfItsBytes) {
    TempDir dir("coop_o2r_sha");
    std::string bytes(70000, 'x');
    bytes[123] = 'y';
    WriteBytes(dir.path / "a.bin", bytes);
    uint64_t size = 0;
    CHECK_EQ(Sha256File(dir.path / "a.bin", &size), Hex(bytes));
    CHECK_EQ(size, (uint64_t)70000);
    CHECK(Sha256File(dir.path / "missing.bin").empty());
}

TEST_CASE(O2rNamesAreSafeToShowAndToKeep) {
    using namespace o2r;
    CHECK(ValidName("N64 WW Mirror Shield (Large).o2r"));
    CHECK(ValidName("a.O2R"));
    CHECK(!ValidName(".o2r"));
    CHECK(!ValidName("x.zip"));
    CHECK(!ValidName("../x.o2r"));
    CHECK(!ValidName("a/b.o2r"));
    CHECK(!ValidName("a\\b.o2r"));
    CHECK(!ValidName(" x.o2r"));
    CHECK(!ValidName("x..y.o2r"));
    CHECK(!ValidName("con.o2r")); // a Windows device, also with more after a dot
    CHECK(!ValidName("LPT1.copia.o2r"));
    CHECK(ValidName(std::string(76, 'a') + ".o2r")); // 80 characters
    CHECK(!ValidName(std::string(77, 'a') + ".o2r"));
    CHECK_EQ(SafeName("N64 WW Mirror Shield (Large).o2r"), std::string("N64 WW Mirror Shield (Large).o2r"));
    CHECK_EQ(SafeName("Escudo Espa\xC3\xB1" "a.o2r"), std::string("Escudo Espa_a.o2r")); // one '_' per character
    CHECK_EQ(SafeName("a:b*c?.o2r"), std::string("a_b_c_.o2r"));
    CHECK_EQ(SafeName("CON.o2r"), std::string("_CON.o2r"));
    CHECK_EQ(SafeName(".oculto.o2r"), std::string("_oculto.o2r"));
    CHECK_EQ(SafeName("fin..o2r"), std::string("fin_.o2r"));
    std::string longName = SafeName(std::string(200, 'x') + ".o2r");
    CHECK_EQ(longName.size(), (size_t)kMaxO2rName);
    CHECK(ValidName(longName));
    CHECK(ValidSha256(Hex("x")));
    CHECK(!ValidSha256(std::string(64, 'A')));
    CHECK(!ValidSha256("abc"));
    Entry e{ "N64 WW Mirror Shield (Large).o2r", 49844, std::string(64, 'a') };
    CHECK_EQ(CacheFileName(e), std::string("N64 WW Mirror Shield (Large).aaaaaaaaaaaaaaaa.o2r"));
    CHECK_EQ(FormatBytes(512), std::string("512 B"));
    CHECK_EQ(FormatBytes(49844), std::string("48.7 KB"));
    CHECK_EQ(FormatBytes(3u << 20), std::string("3.0 MB"));
    CHECK_EQ(FormatBytes(5ull << 30), std::string("5.0 GB"));
}

TEST_CASE(O2rListGoesThroughJsonAndRefusesBadEntries) {
    using namespace o2r;
    std::vector<Entry> list = { EntryOf("a.o2r", "hello"), EntryOf("b.o2r", "world") };
    std::vector<Entry> back;
    std::string why;
    CHECK(ListFromJson(ListToJson(list), back, &why));
    CHECK_EQ(back.size(), (size_t)2);
    CHECK_EQ(back[1].name, std::string("b.o2r"));
    CHECK_EQ(back[1].size, (uint64_t)5);
    CHECK_EQ(back[1].sha256, Hex("world"));
    CHECK(ListFromJson(json(), back, &why)); // a welcome without "o2r": nothing to download
    CHECK(back.empty());
    auto bad = [](const json& value) {
        std::vector<Entry> out = { Entry{ "old.o2r", 1, std::string(64, 'b') } };
        std::string reason;
        bool ok = ListFromJson(value, out, &reason);
        return !ok && out.empty() && !reason.empty();
    };
    auto one = [](json name, json size, json sha) {
        return json::array({ json{ { "name", name }, { "size", size }, { "sha256", sha } } });
    };
    CHECK(bad(json::object()));
    CHECK(bad(one("../x.o2r", 5, Hex("x"))));
    CHECK(bad(one("x.o2r", 0, Hex("x"))));
    CHECK(bad(one("x.o2r", -5, Hex("x"))));
    CHECK(bad(one("x.o2r", kMaxO2rBytes + 1, Hex("x"))));
    CHECK(bad(one("x.o2r", "5", Hex("x"))));
    CHECK(bad(one("x.o2r", 5, std::string(64, 'A'))));
    CHECK(bad(one(5, 5, Hex("x"))));
    CHECK(bad(json::array({ 5 })));
    CHECK(bad(ListToJson({ EntryOf("x.o2r", "1"), EntryOf("X.O2R", "2") }))); // the same name in another case
    CHECK(bad(ListToJson({ EntryOf("x.o2r", "1"), EntryOf("y.o2r", "1") }))); // the same file twice
    std::vector<Entry> many;
    for (int i = 0; i <= kMaxO2rFiles; i++) {
        many.push_back(EntryOf("m" + std::to_string(i) + ".o2r", std::to_string(i)));
    }
    CHECK(bad(ListToJson(many)));
    many.pop_back();
    CHECK(ListFromJson(ListToJson(many), back, &why)); // exactly the limit
}

TEST_CASE(O2rChunksGoThroughTheirBinaryForm) {
    using namespace o2r;
    std::string data(1000, 'q');
    data[999] = 'z';
    auto packet = EncodeChunk(3, 0x01020304u, (const uint8_t*)data.data(), data.size());
    CHECK_EQ(packet.size(), kChunkHeader + 1000);
    CHECK_EQ(packet[0], kStreamO2r);
    Chunk c;
    CHECK(DecodeChunk(packet.data(), packet.size(), c));
    CHECK_EQ(c.index, (uint8_t)3);
    CHECK_EQ(c.offset, 0x01020304u);
    CHECK_EQ(c.size, (size_t)1000);
    CHECK(std::memcmp(c.data, data.data(), 1000) == 0);
    CHECK(!DecodeChunk(packet.data(), kChunkHeader, c)); // a header without bytes
    std::string big(kO2rChunkBytes + 1, 'b');
    auto tooBig = EncodeChunk(0, 0, (const uint8_t*)big.data(), big.size());
    CHECK(!DecodeChunk(tooBig.data(), tooBig.size(), c));
    packet[0] = kStreamActors;
    CHECK(!DecodeChunk(packet.data(), packet.size(), c)); // another stream
}

namespace {

// Keeps what a download writes, as the game's file sink would keep its files.
struct MemorySink : o2r::Download::Sink {
    std::map<std::string, std::string> kept;
    std::string name;
    std::string bytes;
    bool failOpen = false;
    int discarded = 0;
    bool Open(const o2r::Entry& e) override {
        name = e.name;
        bytes.clear();
        return !failOpen;
    }
    bool Write(const uint8_t* data, size_t size) override {
        bytes.append((const char*)data, size);
        return true;
    }
    bool Close(bool keep) override {
        if (keep) {
            kept[name] = bytes;
        } else {
            discarded++;
        }
        return true;
    }
};

std::string Bytes(size_t n, uint32_t seed) {
    std::string out(n, '\0');
    for (size_t i = 0; i < n; i++) {
        seed = seed * 1664525u + 1013904223u;
        out[i] = (char)(seed >> 24);
    }
    return out;
}

// The chunk the server sends for a request.
o2r::Chunk ChunkOf(uint8_t index, const std::string& file, uint64_t offset) {
    size_t n = (size_t)std::min<uint64_t>(kO2rChunkBytes, file.size() - offset);
    return { index, (uint32_t)offset, (const uint8_t*)file.data() + offset, n };
}

} // namespace

TEST_CASE(O2rDownloadKeepsTheWindowFullAndVerifiesEachFile) {
    std::string a = Bytes(kO2rChunkBytes * 20 + 123, 7); // 21 chunks
    std::string b = Bytes(10, 9);
    MemorySink sink;
    o2r::Download d({ { 2, EntryOf("a.o2r", a) }, { 5, EntryOf("b.o2r", b) } }, sink);
    CHECK_EQ(d.BytesTotal(), (uint64_t)(a.size() + b.size()));
    CHECK_EQ(d.Count(), (size_t)2);
    std::map<uint8_t, const std::string*> files = { { 2, &a }, { 5, &b } };
    auto first = d.TakeRequests();
    CHECK_EQ(first.size(), (size_t)kO2rWindow);
    CHECK_EQ(first[0].index, (uint8_t)2);
    CHECK_EQ(first[0].offset, (uint32_t)0);
    CHECK_EQ(first[1].offset, (uint32_t)kO2rChunkBytes);
    CHECK(d.TakeRequests().empty()); // the window is full
    CHECK(d.Current() != nullptr && d.Current()->name == "a.o2r");
    CHECK_EQ(d.Number(), (size_t)1);
    std::deque<o2r::Download::Request> pending(first.begin(), first.end());
    for (int guard = 0; !d.Done() && !d.Failed() && guard < 1000; guard++) {
        CHECK(!pending.empty());
        o2r::Download::Request r = pending.front();
        pending.pop_front();
        CHECK(d.OnChunk(ChunkOf(r.index, *files[r.index], r.offset)));
        for (const auto& more : d.TakeRequests()) {
            pending.push_back(more);
        }
    }
    CHECK(d.Done());
    CHECK(pending.empty());
    CHECK(sink.kept["a.o2r"] == a);
    CHECK(sink.kept["b.o2r"] == b);
    CHECK_EQ(d.BytesDone(), d.BytesTotal());
    CHECK(d.Current() == nullptr);
    CHECK(!d.OnChunk(ChunkOf(5, b, 0))); // nothing is expected any more
    CHECK(d.Done());
}

TEST_CASE(O2rDownloadFailsOnAnUnexpectedChunkOrAWrongHash) {
    std::string a = Bytes(kO2rChunkBytes * 2, 3);
    { // a chunk that is not the next one
        MemorySink sink;
        o2r::Download d({ { 0, EntryOf("a.o2r", a) } }, sink);
        d.TakeRequests();
        CHECK(!d.OnChunk(ChunkOf(0, a, kO2rChunkBytes)));
        CHECK(d.Failed());
        CHECK(d.Fail() == o2r::Download::Error::BadChunk);
        CHECK_EQ(sink.discarded, 1);
        CHECK(sink.kept.empty());
        CHECK(d.TakeRequests().empty());
        CHECK(d.Current() == nullptr);
    }
    { // another file's index, and a chunk cut short
        MemorySink sink;
        o2r::Download d({ { 0, EntryOf("a.o2r", a) } }, sink);
        d.TakeRequests();
        CHECK(!d.OnChunk(ChunkOf(1, a, 0)));
        CHECK(d.Fail() == o2r::Download::Error::BadChunk);
        MemorySink sink2;
        o2r::Download d2({ { 0, EntryOf("a.o2r", a) } }, sink2);
        d2.TakeRequests();
        o2r::Chunk shortChunk = ChunkOf(0, a, 0);
        shortChunk.size -= 1;
        CHECK(!d2.OnChunk(shortChunk));
        CHECK(d2.Fail() == o2r::Download::Error::BadChunk);
    }
    { // a chunk nobody asked for
        MemorySink sink;
        o2r::Download d({ { 0, EntryOf("a.o2r", a) } }, sink);
        CHECK(!d.OnChunk(ChunkOf(0, a, 0)));
        CHECK(d.Fail() == o2r::Download::Error::BadChunk);
    }
    { // the bytes do not match the announced hash: nothing is kept
        MemorySink sink;
        std::string other = a;
        other[5] ^= 1;
        o2r::Download d({ { 0, EntryOf("a.o2r", a) } }, sink);
        d.TakeRequests();
        CHECK(d.OnChunk(ChunkOf(0, other, 0)));
        CHECK(!d.OnChunk(ChunkOf(0, other, kO2rChunkBytes)));
        CHECK(d.Fail() == o2r::Download::Error::Hash);
        CHECK(sink.kept.empty());
        CHECK_EQ(sink.discarded, 1);
    }
    { // the file cannot be created
        MemorySink sink;
        sink.failOpen = true;
        o2r::Download d({ { 0, EntryOf("a.o2r", a) } }, sink);
        CHECK(d.TakeRequests().empty());
        CHECK(d.Fail() == o2r::Download::Error::Open);
    }
    { // a download dropped half-way throws its file away
        MemorySink sink;
        {
            o2r::Download d({ { 0, EntryOf("a.o2r", a) } }, sink);
            d.TakeRequests();
            CHECK(d.OnChunk(ChunkOf(0, a, 0)));
        }
        CHECK_EQ(sink.discarded, 1);
        CHECK(sink.kept.empty());
    }
    { // nothing to download: done at once
        MemorySink sink;
        o2r::Download d({}, sink);
        CHECK(d.Done());
        CHECK(d.TakeRequests().empty());
        CHECK_EQ(d.BytesTotal(), (uint64_t)0);
    }
}

TEST_CASE(O2rStoreReadsTheFolderInOrder) {
    TempDir dir("coop_o2r_store");
    WriteBytes(dir.path / "b.o2r", "bbbb");
    WriteBytes(dir.path / "A.o2r", "aaaa");
    WriteBytes(dir.path / "notes.txt", "x");
    WriteBytes(dir.path / "empty.o2r", "");
    WriteBytes(dir.path / "c.O2R", "cc");
    server::O2rStore store;
    std::string warnings;
    store.Load({ dir.path.string(), { "*" } }, &warnings);
    CHECK_EQ(store.Entries().size(), (size_t)3); // empty.o2r is left out, with a warning
    CHECK_EQ(store.Entries()[0].name, std::string("A.o2r"));
    CHECK_EQ(store.Entries()[1].name, std::string("b.o2r"));
    CHECK_EQ(store.Entries()[2].name, std::string("c.o2r"));
    CHECK_EQ(store.Entries()[1].sha256, Hex("bbbb"));
    CHECK_EQ(store.Entries()[1].size, (uint64_t)4);
    CHECK(warnings.find("empty.o2r") != std::string::npos);
    warnings.clear();
    store.Load({ dir.path.string(), { "c", "b.o2r", "*", "!a", "missing" } }, &warnings);
    CHECK_EQ(store.Entries().size(), (size_t)2);
    CHECK_EQ(store.Entries()[0].name, std::string("c.o2r"));
    CHECK_EQ(store.Entries()[1].name, std::string("b.o2r"));
    CHECK(warnings.find("missing") != std::string::npos);
    std::vector<uint8_t> out;
    CHECK(store.ReadChunk(1, 1, 2, out));
    CHECK(std::string(out.begin(), out.end()) == "bb");
    CHECK(store.ReadChunk(1, 2, 100, out));
    CHECK_EQ(out.size(), (size_t)2);
    CHECK(!store.ReadChunk(1, 4, 100, out));
    CHECK(!store.ReadChunk(5, 0, 10, out));
    CHECK(store.Accepts(json{ { "o2r", json::array({ Hex("bbbb"), Hex("cc") }) } }));
    CHECK(!store.Accepts(json{ { "o2r", json::array({ Hex("cc") }) } }));
    CHECK(!store.Accepts(json::object()));
    CHECK(server::O2rStore::HasO2rFiles(dir.path.string()));
    CHECK(!server::O2rStore::HasO2rFiles(""));
    store.Load({ "", { "*" } }, &warnings);
    CHECK(store.Empty());
    CHECK(store.Accepts(json::object()));
}

TEST_CASE(ConfigHasTheO2rSection) {
    TempDir dir("coop_o2r_cfg");
    std::string path = dir.File("server.json");
    server::ServerConfig cfg;
    std::string warn;
    CHECK(server::LoadOrCreateConfig(path, cfg, &warn));
    CHECK_EQ(cfg.o2r.dir, std::string("o2r"));
    CHECK_EQ(cfg.o2r.files.size(), (size_t)1);
    std::ifstream created(path);
    json file = json::parse(created, nullptr, false);
    created.close();
    CHECK_EQ(file["o2r"]["dir"].get<std::string>(), std::string("o2r"));
    CHECK_EQ(file["o2r"]["files"][0].get<std::string>(), std::string("*"));
    WriteBytes(path, R"({ "o2r": { "dir": "paquetes", "files": ["b", "!c"] } })");
    CHECK(server::LoadOrCreateConfig(path, cfg, &warn));
    CHECK_EQ(cfg.o2r.dir, std::string("paquetes"));
    CHECK_EQ(cfg.o2r.files.size(), (size_t)2);
    WriteBytes(path, R"({ "o2r": { "dir": 5 } })");
    CHECK(server::LoadOrCreateConfig(path, cfg, &warn));
    CHECK_EQ(cfg.o2r.dir, std::string("o2r"));
    CHECK(warn.find("o2r.dir") != std::string::npos);
    const char* argv[] = { "server", "--o2r-dir", "otros" };
    server::ApplyCommandLine(cfg, (int)std::size(argv), (char**)argv, nullptr);
    CHECK_EQ(cfg.o2r.dir, std::string("otros"));
    const char* off[] = { "server", "--no-o2r" };
    server::ApplyCommandLine(cfg, (int)std::size(off), (char**)off, nullptr);
    CHECK(cfg.o2r.dir.empty());
    server::ServerConfig inCode; // tests and tools: no .o2r unless a folder is given
    CHECK(inCode.o2r.dir.empty());
}

TEST_CASE(PlayersAndHostsGetTheO2rListInTheirWelcome) {
    TempDir dir("coop_o2r_welcome");
    WriteBytes(dir.path / "Escudo.o2r", "12345");
    server::ServerConfig cfg;
    cfg.o2r.dir = dir.path.string();
    cfg.hostToken = "secreto";
    TestServer s(cfg);
    auto player = Join(s, "Ana");
    std::vector<o2r::Entry> list;
    CHECK(o2r::ListFromJson(player->welcome["o2r"], list, nullptr));
    CHECK_EQ(list.size(), (size_t)1);
    CHECK_EQ(list[0].name, std::string("Escudo.o2r"));
    CHECK_EQ(list[0].sha256, Hex("12345"));
    auto host = Connect(s);
    host->Send({ { "t", "hello" }, { "proto", kProtocolVersion }, { "nick", "#host" }, { "host", true },
                 { "token", "secreto" } });
    auto welcome = host->WaitFor("welcome", s);
    CHECK(welcome.has_value());
    CHECK((*welcome)["o2r"].is_array());
    player->Cmd("/mods");
    auto reply = player->WaitFor("sys", s);
    CHECK(reply.has_value());
    CHECK(GetString(*reply, "text").find("Escudo.o2r (5 B)") != std::string::npos);
}

TEST_CASE(AServerWithoutO2rSendsNoList) {
    TestServer s;
    auto c = Join(s, "Ana");
    CHECK(!c->welcome.contains("o2r"));
}

namespace {

// A server that shares one .o2r.
std::unique_ptr<TestServer> ServerWith(TempDir& dir, const std::string& name, const std::string& bytes) {
    WriteBytes(dir.path / name, bytes);
    server::ServerConfig cfg;
    cfg.o2r.dir = dir.path.string();
    return std::make_unique<TestServer>(cfg);
}

} // namespace

TEST_CASE(PlayersDownloadEachO2rThroughTheServer) {
    TempDir dir("coop_o2r_download");
    std::string mod = Bytes(kO2rChunkBytes * 3 + 77, 11);
    auto s = ServerWith(dir, "N64 WW Mirror Shield (Large).o2r", mod);
    auto c = Join(*s, "Ana");
    std::vector<o2r::Entry> list;
    CHECK(o2r::ListFromJson(c->welcome["o2r"], list, nullptr));
    CHECK_EQ(list.size(), (size_t)1);
    MemorySink sink;
    o2r::Download d({ { 0, list[0] } }, sink);
    auto ask = [&] {
        for (const auto& r : d.TakeRequests()) {
            c->Send({ { "t", "o2r_get" }, { "i", r.index }, { "off", r.offset } });
        }
    };
    ask();
    CHECK(c->WaitUntil(*s, 5000, [&] {
        while (!c->rawStreams.empty()) {
            std::vector<uint8_t> packet = c->rawStreams.front();
            c->rawStreams.pop_front();
            o2r::Chunk chunk;
            if (o2r::DecodeChunk(packet.data(), packet.size(), chunk) && d.OnChunk(chunk)) {
                ask();
            }
        }
        return d.Done() || d.Failed();
    }));
    CHECK(d.Done());
    CHECK(sink.kept["N64 WW Mirror Shield (Large).o2r"] == mod);
}

TEST_CASE(TheServerRefusesBadO2rRequests) {
    TempDir dir("coop_o2r_bad");
    auto s = ServerWith(dir, "a.o2r", "12345");
    auto c = Join(*s, "Ana");
    c->Send({ { "t", "o2r_get" }, { "i", 1 }, { "off", 0 } });   // there is no file 1
    c->Send({ { "t", "o2r_get" }, { "i", 0 }, { "off", 5 } });   // past its end
    c->Send({ { "t", "o2r_get" }, { "i", 0 }, { "off", -1 } });
    c->Send({ { "t", "o2r_get" }, { "i", "0" }, { "off", 0 } }); // not a number
    c->Send({ { "t", "o2r_get" }, { "i", 0 }, { "off", 3 } });   // the only good one: the last 2 bytes
    s->PumpFor(200);
    CHECK_EQ(c->rawStreams.size(), (size_t)1);
    o2r::Chunk chunk;
    CHECK(o2r::DecodeChunk(c->rawStreams.front().data(), c->rawStreams.front().size(), chunk));
    CHECK_EQ(chunk.offset, (uint32_t)3);
    CHECK(std::string((const char*)chunk.data, chunk.size) == "45");
    auto* rc = s->server->Players().ByNick("Ana");
    CHECK(rc != nullptr);
    CHECK_EQ(rc->invalidMessages, (uint32_t)4);
}

TEST_CASE(TheServerPacesO2rChunksAndLimitsItsQueue) {
    TempDir dir("coop_o2r_queue");
    auto s = ServerWith(dir, "a.o2r", Bytes(kO2rChunkBytes, 5));
    auto c = Join(*s, "Ana");
    const int asked = kO2rBurst + kO2rQueueMax + 8; // the burst is served, the queue fills, the rest is refused
    for (int i = 0; i < asked; i++) {
        c->Send({ { "t", "o2r_get" }, { "i", 0 }, { "off", 0 } });
    }
    auto* rc = s->server->Players().ByNick("Ana");
    CHECK(rc != nullptr);
    CHECK(c->WaitUntil(*s, 3000, [&] { return (int)c->rawStreams.size() + (int)rc->invalidMessages >= asked; }));
    s->PumpFor(100);
    CHECK(rc->invalidMessages > 0);
    CHECK_EQ((int)c->rawStreams.size() + (int)rc->invalidMessages, asked);
}

TEST_CASE(EnteringTheWorldNeedsTheServersO2r) {
    TempDir dir("coop_o2r_enter");
    auto s = ServerWith(dir, "a.o2r", "12345");
    auto c = Join(*s, "Ana");
    c->Send({ { "t", "world_enter" } });
    CHECK(c->WaitForSys(level::kError, *s));
    c->Send({ { "t", "world_enter" }, { "o2r", json::array({ std::string(64, '0') }) } });
    CHECK(c->WaitForSys(level::kError, *s));
    CHECK(!c->WaitFor("world_full", *s, 200).has_value());
    c->Send({ { "t", "world_enter" }, { "o2r", json::array({ Hex("12345") }) } });
    CHECK(c->WaitFor("world_full", *s).has_value());
}
