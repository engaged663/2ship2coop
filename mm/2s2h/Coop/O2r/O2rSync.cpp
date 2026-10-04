// [COOP] The server's game mods (.o2r): what is missing when "welcome" arrives, the question, the download to
// coop_mods/ (a ".part" per file, renamed once its SHA-256 is the announced one) and the hand-over to O2rLoader.
// Map: O2r.h.
#include "O2r.h"

#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Host/HostMode.h"

#include "common/Protocol.h"
#include "common/StreamIds.h"

#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/libultraship.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>

namespace coop::client {

namespace {

namespace fs = std::filesystem;

O2rState sState = O2rState::Idle;
std::vector<o2r::Entry> sRequired;        // the server's list, in its order of loading
std::vector<o2r::Download::Job> sMissing; // what is not in coop_mods/ yet
std::unique_ptr<o2r::Download> sDownload;
int64_t sLastChunkMs = 0;

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Next to 2ship.exe (the same folder as mods/): never one of the game's own mod folders.
std::string CacheDir() {
    return Ship::Context::GetPathRelativeToAppDirectory("coop_mods");
}

std::string FinalPath(const o2r::Entry& entry) {
    return CacheDir() + "/" + o2r::CacheFileName(entry);
}

std::string PartPath(const o2r::Entry& entry) {
    return FinalPath(entry) + ".part";
}

// coop_mods/<name>.<hash>.o2r.part while it arrives; renamed to .o2r once verified, removed otherwise.
class FileSink : public o2r::Download::Sink {
  public:
    bool Open(const o2r::Entry& entry) override {
        std::error_code ec;
        fs::create_directories(fs::path(CacheDir()), ec);
        mEntry = entry;
        mFile = std::fopen(PartPath(entry).c_str(), "wb");
        return mFile != nullptr;
    }
    bool Write(const uint8_t* data, size_t size) override {
        return mFile != nullptr && std::fwrite(data, 1, size, mFile) == size;
    }
    bool Close(bool keep) override {
        bool written = mFile != nullptr && std::fclose(mFile) == 0;
        mFile = nullptr;
        std::error_code ec;
        if (keep && written) {
            fs::rename(fs::path(PartPath(mEntry)), fs::path(FinalPath(mEntry)), ec);
            if (!ec) {
                return true;
            }
        }
        fs::remove(fs::path(PartPath(mEntry)), ec);
        return false;
    }

  private:
    o2r::Entry mEntry;
    FILE* mFile = nullptr;
};

FileSink sSink;

std::string Names(const std::vector<o2r::Entry>& list) {
    std::string out;
    for (const o2r::Entry& entry : list) {
        out += (out.empty() ? "" : ", ") + entry.name + " (" + o2r::FormatBytes(entry.size) + ")";
    }
    return out;
}

void Reset() {
    sDownload.reset(); // a file left half-way is thrown away
    sMissing.clear();
    sRequired.clear();
    sState = O2rState::Idle;
}

// Without the server's mods its world cannot be played: the player is told why and the connection ends.
void Fail(const std::string& why) {
    SPDLOG_WARN("[Coop] .o2r: {}", why);
    Reset();
    Chat_Add(ChatKind::Error, why);
    NetClient::Get().Disconnect(why);
}

const char* ErrorText(o2r::Download::Error error) {
    switch (error) {
        case o2r::Download::Error::Open:
            return "No se pudo crear el archivo del mod en la carpeta coop_mods.";
        case o2r::Download::Error::Write:
            return "No se pudo guardar el mod en la carpeta coop_mods (¿disco lleno?).";
        case o2r::Download::Error::Hash:
            return "El mod descargado no coincide con el del servidor.";
        default:
            return "El servidor ha enviado un trozo de mod que no se esperaba.";
    }
}

void StartLoading() {
    // From the first one this game does not have to the end: the server's last mod must be the last one loaded.
    size_t first = 0;
    while (first < sRequired.size() && O2rLoader_IsLoaded(sRequired[first].sha256)) {
        first++;
    }
    if (first == sRequired.size()) {
        sState = O2rState::Ready;
        return;
    }
    std::vector<O2rLoadItem> items;
    for (size_t i = first; i < sRequired.size(); i++) {
        items.push_back({ sRequired[i], FinalPath(sRequired[i]) });
    }
    O2rLoader_Queue(std::move(items));
    sState = O2rState::Loading;
}

void SendRequests() {
    for (const o2r::Download::Request& r : sDownload->TakeRequests()) {
        json ev = MakeEvent(ev::kO2rGet);
        ev["i"] = r.index;
        ev["off"] = r.offset;
        NetClient::Get().SendEvent(ev);
    }
    if (sDownload->Failed()) {
        Fail(ErrorText(sDownload->Fail()));
    }
}

void StartDownload() {
    sDownload = std::make_unique<o2r::Download>(sMissing, sSink);
    sState = O2rState::Downloading;
    sLastChunkMs = NowMs();
    SPDLOG_INFO("[Coop] .o2r: downloading {} file(s), {}", sMissing.size(),
                o2r::FormatBytes(sDownload->BytesTotal()));
    Chat_Add(ChatKind::Info,
             "Descargando los mods del servidor (" + o2r::FormatBytes(sDownload->BytesTotal()) + ")...");
    SendRequests();
}

void OnWelcome(const json& ev) {
    Reset();
    std::string why;
    auto list = ev.find("o2r");
    if (!o2r::ListFromJson(list != ev.end() ? *list : json(), sRequired, &why)) {
        SPDLOG_WARN("[Coop] .o2r: the server's list was refused: {}", why);
        Fail("El servidor ha enviado una lista de mods que no es válida.");
        return;
    }
    if (sRequired.empty()) {
        sState = O2rState::Ready;
        return;
    }
    uint64_t missingBytes = 0;
    for (size_t i = 0; i < sRequired.size(); i++) {
        if (!O2rLoader_IsLoaded(sRequired[i].sha256) && !O2r_InCache(sRequired[i])) {
            sMissing.push_back({ (uint8_t)i, sRequired[i] });
            missingBytes += sRequired[i].size;
        }
    }
    SPDLOG_INFO("[Coop] .o2r: the server uses {} ({} missing)", Names(sRequired), sMissing.size());
    Chat_Add(ChatKind::Info, "El servidor usa mods del juego (.o2r): " + Names(sRequired) + ".");
    if (sMissing.empty()) {
        StartLoading();
    } else if (HostMode_Enabled() || CVarGetInteger("gCoop.O2r.AutoDownload", 0) != 0) {
        StartDownload();
    } else {
        sState = O2rState::Asking;
        Chat_Add(ChatKind::Warn, "Te faltan " + o2r::FormatBytes(missingBytes) +
                                     ": acepta la descarga en la ventana de arriba o en F1 -> Co-op.");
    }
}

void OnChunk(const uint8_t* data, size_t size) {
    if (sState != O2rState::Downloading || sDownload == nullptr) {
        return;
    }
    o2r::Chunk chunk;
    if (!o2r::DecodeChunk(data, size, chunk)) {
        Fail(ErrorText(o2r::Download::Error::BadChunk));
        return;
    }
    if (!sDownload->OnChunk(chunk)) {
        Fail(ErrorText(sDownload->Fail()));
        return;
    }
    sLastChunkMs = NowMs();
    if (sDownload->Done()) {
        SPDLOG_INFO("[Coop] .o2r: downloaded {}", o2r::FormatBytes(sDownload->BytesTotal()));
        sDownload.reset();
        sMissing.clear();
        StartLoading();
        return;
    }
    SendRequests();
}

void OnLost(const std::string&) {
    Reset();
}

} // namespace

O2rState O2r_State() {
    return sState;
}

bool O2r_Ready() {
    return sState == O2rState::Ready;
}

const std::vector<o2r::Entry>& O2r_Required() {
    return sRequired;
}

std::vector<std::string> O2r_LoadedHashes() {
    std::vector<std::string> out;
    for (const o2r::Entry& entry : sRequired) {
        if (O2rLoader_IsLoaded(entry.sha256)) {
            out.push_back(entry.sha256);
        }
    }
    return out;
}

bool O2r_InCache(const o2r::Entry& entry) {
    std::error_code ec;
    uintmax_t size = fs::file_size(fs::path(FinalPath(entry)), ec);
    return !ec && size == entry.size;
}

void O2r_Accept(bool always) {
    if (sState != O2rState::Asking) {
        return;
    }
    if (always) {
        CVarSetInteger("gCoop.O2r.AutoDownload", 1);
        CVarSave();
    }
    StartDownload();
}

void O2r_Decline() {
    if (sState == O2rState::Asking || sState == O2rState::Downloading) {
        Fail("Sin los mods del servidor no puedes jugar en él: te has desconectado.");
    }
}

O2rProgress O2r_Progress() {
    O2rProgress progress;
    if (sDownload != nullptr) {
        const o2r::Entry* current = sDownload->Current();
        progress.name = current != nullptr ? current->name : "";
        progress.number = sDownload->Number();
        progress.count = sDownload->Count();
        progress.done = sDownload->BytesDone();
        progress.total = sDownload->BytesTotal();
        return progress;
    }
    progress.count = sMissing.size();
    for (const o2r::Download::Job& job : sMissing) {
        progress.total += job.entry.size;
    }
    return progress;
}

void O2r_FrameStart() {
    O2rLoader_AltFrame();
    if (sState == O2rState::Downloading && NowMs() - sLastChunkMs > kO2rStallMs) {
        Fail("El servidor ha dejado de enviar los mods (.o2r).");
        return;
    }
    if (sState != O2rState::Loading || O2rLoader_Busy()) {
        return;
    }
    for (const o2r::Entry& entry : sRequired) {
        if (!O2rLoader_IsLoaded(entry.sha256)) {
            std::error_code ec;
            fs::remove(fs::path(FinalPath(entry)), ec); // maybe damaged: it is downloaded again next time
            Fail("No se pudo cargar el mod del servidor " + entry.name + ".");
            return;
        }
    }
    sState = O2rState::Ready;
    SPDLOG_INFO("[Coop] .o2r: ready");
    Chat_Add(ChatKind::Ok, "Mods del servidor cargados: " + Names(sRequired) + ".");
}

COOP_ON_EVENT(o2rWelcome, ev::kWelcome, OnWelcome);
COOP_ON_STREAM(o2rChunk, kStreamO2r, OnChunk);
COOP_ON_LOST(o2rLost, OnLost);

} // namespace coop::client
