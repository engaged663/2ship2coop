// 2ship-coop-bot: fake player for testing with a single PC.
//   --mode mirror (default): copies the target's moves 0.3 s late, standing 60 units to its side.
//   --mode circle: walks in circles around the target.
//   --mode front: stands still 80 units in front of the target, facing it (easy to spot on screen).
// It also answers "!ping" in the chat, pays every /gift it makes and accepts every gift.
// Usage: 2ship-coop-bot [--host 127.0.0.1] [--port 7780] [--nick Bot] [--pass X] [--target Nick] [--mode mirror|circle|front]
#include "common/Events.h"
#include "common/PlayerState.h"
#include "common/Protocol.h"
#include "common/Transport.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <map>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace coop;

namespace {

struct Options {
    std::string host = "127.0.0.1";
    uint16_t port = kDefaultPort;
    std::string nick = "Bot";
    std::string pass;
    std::string target;
    std::string mode = "mirror";
};

struct Known {
    std::string nick;
    int scene = -1;
    std::string sceneName;
};

constexpr float kPi = 3.14159265f;
constexpr int kMirrorDelayFrames = 6; // 0.3 s at 20 Hz

Options ParseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        std::string v = i + 1 < argc ? argv[i + 1] : "";
        if (a == "--host") { o.host = v; i++; }
        else if (a == "--port") { o.port = (uint16_t)std::stoi(v); i++; }
        else if (a == "--nick") { o.nick = v; i++; }
        else if (a == "--pass") { o.pass = v; i++; }
        else if (a == "--target") { o.target = v; i++; }
        else if (a == "--mode") { o.mode = v; i++; }
        else {
            std::printf("Uso: 2ship-coop-bot [--host IP] [--port N] [--nick Bot] [--pass X] [--target Nick] "
                        "[--mode mirror|circle|front]\n");
            std::exit(0);
        }
    }
    return o;
}

class Bot {
  public:
    explicit Bot(Options o) : mOpt(std::move(o)) {
    }

    int Run() {
        std::string err;
        if (!mNet.Connect(mOpt.host, mOpt.port, &mPeer, &err)) {
            std::printf("No se pudo conectar: %s\n", err.c_str());
            return 1;
        }
        std::printf("Conectando a %s:%u como %s...\n", mOpt.host.c_str(), mOpt.port, mOpt.nick.c_str());
        auto nextFrame = std::chrono::steady_clock::now();
        while (!mDone) {
            std::vector<NetEvent> events;
            mNet.Service(5, events);
            for (auto& e : events) {
                Handle(e);
            }
            if (std::chrono::steady_clock::now() >= nextFrame) {
                nextFrame += std::chrono::milliseconds(50);
                Frame();
            }
        }
        return mExitCode;
    }

  private:
    void Send(const json& ev) {
        std::string text = SerializeEvent(ev);
        mNet.Send(mPeer, kChannelEvents, text.data(), text.size());
    }

    void Handle(NetEvent& e) {
        if (e.type == NetEvent::Connect) {
            Send({ { "t", ev::kHello }, { "proto", kProtocolVersion }, { "nick", mOpt.nick }, { "pass", mOpt.pass } });
            return;
        }
        if (e.type == NetEvent::Disconnect) {
            std::printf("Desconectado del servidor.\n");
            mDone = true;
            return;
        }
        if (e.channel == kChannelStream) {
            PlayerState st;
            if (DecodePlayerState(e.data.data(), e.data.size(), st) && st.playerId == mTargetId) {
                if (mTargetStates.empty() && !mLoggedFirstPose) {
                    std::printf("Recibiendo la pose del objetivo (escena %d)\n", st.sceneId);
                    mLoggedFirstPose = true;
                }
                mTargetStates.push_back(st);
                while (mTargetStates.size() > 40) {
                    mTargetStates.pop_front();
                }
            }
            return;
        }
        json evt;
        if (!ParseEvent(e.data.data(), e.data.size(), evt, nullptr)) {
            return;
        }
        HandleEvent(evt);
    }

    void HandleEvent(const json& evt) {
        std::string t = EventType(evt);
        if (t == ev::kWelcome) {
            mMyId = (int)GetInt(evt, "id");
            std::printf("Conectado (id %d). %s\n", mMyId, GetString(evt, "motd").c_str());
            for (auto& p : evt["players"]) {
                mKnown[(int)GetInt(p, "id")] = { GetString(p, "nick"), (int)GetInt(p, "scene", -1), GetString(p, "sceneName") };
            }
        } else if (t == ev::kReject || t == ev::kKicked) {
            std::printf("El servidor dijo: %s\n", GetString(evt, "reason").c_str());
            mExitCode = 1;
        } else if (t == ev::kJoin) {
            mKnown[(int)GetInt(evt, "id")] = { GetString(evt, "nick"), -1, "" };
        } else if (t == ev::kLeave) {
            mKnown.erase((int)GetInt(evt, "id"));
        } else if (t == ev::kLoc) {
            auto& k = mKnown[(int)GetInt(evt, "id")];
            k.scene = (int)GetInt(evt, "scene", -1);
            k.sceneName = GetString(evt, "sceneName");
        } else if (t == ev::kChat) {
            std::printf("<%s> %s\n", GetString(evt, "from").c_str(), GetString(evt, "text").c_str());
            if (GetString(evt, "text") == "!ping") {
                Send({ { "t", ev::kChat }, { "text", "pong" } });
            }
        } else if (t == ev::kPm) {
            std::string from = GetString(evt, "from");
            std::printf("[privado] %s -> %s: %s\n", from.c_str(), GetString(evt, "to").c_str(),
                        GetString(evt, "text").c_str());
            if (from != mOpt.nick) {
                Send({ { "t", ev::kCmd }, { "line", "/pm " + from + " \"recibido: " + GetString(evt, "text") + "\"" } });
            }
        } else if (t == ev::kSys) {
            std::printf("[%s] %s\n", GetString(evt, "level").c_str(), GetString(evt, "text").c_str());
        } else if (t == ev::kGiftDebit) {
            Send({ { "t", ev::kGiftPaid }, { "gid", GetInt(evt, "gid") }, { "paid", GetInt(evt, "amount") } });
        } else if (t == ev::kGiftCredit) {
            Send({ { "t", ev::kGiftRecv }, { "gid", GetInt(evt, "gid") }, { "accepted", GetInt(evt, "amount") } });
        }
    }

    // Picks the target, follows its scene, and sends our fake pose.
    void Frame() {
        if (mMyId == 0) {
            return;
        }
        int targetId = 0;
        for (auto& [id, k] : mKnown) {
            bool wanted = mOpt.target.empty() || k.nick == mOpt.target;
            if (wanted && id != mMyId) {
                targetId = id;
                break;
            }
        }
        if (targetId != mTargetId) {
            mTargetId = targetId;
            mTargetStates.clear();
            if (targetId != 0) {
                std::printf("Siguiendo a %s\n", mKnown[targetId].nick.c_str());
            }
        }
        if (mTargetId == 0) {
            return;
        }
        const Known& target = mKnown[mTargetId];
        if (target.scene >= 0 && target.scene != mScene) {
            mScene = target.scene;
            Send({ { "t", ev::kLoc }, { "scene", mScene }, { "room", 0 }, { "entrance", 0 }, { "sceneName", target.sceneName } });
        }
        if (mTargetStates.empty()) {
            return;
        }

        PlayerState out;
        if (mOpt.mode == "front") {
            out = mTargetStates.back();
            float yaw = out.rot.y * (2.f * kPi / 65536.f);
            out.pos[0] += 80.f * std::sin(yaw);
            out.pos[2] += 80.f * std::cos(yaw);
            out.rot.y = (int16_t)(out.rot.y + 0x8000);
        } else if (mOpt.mode == "circle") {
            out = mTargetStates.back();
            mAngle += 2.f * kPi / 120.f; // one lap every 6 s
            out.pos[0] += 90.f * std::sin(mAngle);
            out.pos[2] += 90.f * std::cos(mAngle);
            out.rot.y = (int16_t)((mAngle + kPi / 2.f) * 65536.f / (2.f * kPi));
        } else {
            size_t index = mTargetStates.size() > kMirrorDelayFrames ? mTargetStates.size() - 1 - kMirrorDelayFrames : 0;
            out = mTargetStates[index];
            float yaw = out.rot.y * (2.f * kPi / 65536.f);
            out.pos[0] += 60.f * std::cos(yaw);
            out.pos[2] -= 60.f * std::sin(yaw);
        }
        out.playerId = 0;
        out.seq = ++mSeq;
        out.sceneId = (int16_t)mScene;
        auto bytes = EncodePlayerState(out);
        mNet.Send(mPeer, kChannelStream, bytes.data(), bytes.size());
    }

    Options mOpt;
    Transport mNet;
    uint32_t mPeer = 0;
    bool mDone = false;
    int mExitCode = 0;
    int mMyId = 0;
    int mTargetId = 0;
    int mScene = -1;
    uint16_t mSeq = 0;
    float mAngle = 0.f;
    bool mLoggedFirstPose = false;
    std::map<int, Known> mKnown;
    std::deque<PlayerState> mTargetStates;
};

} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0); // show output immediately, even when redirected
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    return Bot(ParseArgs(argc, argv)).Run();
}
