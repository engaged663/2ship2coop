#include "Dispatcher.h"

#include "NetClient.h"

#include <unordered_map>
#include <vector>

namespace coop::client {

static std::unordered_map<std::string, std::vector<EventFn>>& EventTable() {
    static std::unordered_map<std::string, std::vector<EventFn>> table;
    return table;
}

static std::unordered_map<uint8_t, std::vector<StreamFn>>& StreamTable() {
    static std::unordered_map<uint8_t, std::vector<StreamFn>> table;
    return table;
}

static std::vector<LostFn>& LostTable() {
    static std::vector<LostFn> table;
    return table;
}

void AddEventHandler(const std::string& type, EventFn fn) {
    EventTable()[type].push_back(fn);
}

void AddStreamHandler(uint8_t streamType, StreamFn fn) {
    StreamTable()[streamType].push_back(fn);
}

void AddLostHandler(LostFn fn) {
    LostTable().push_back(fn);
}

void ProcessNetwork() {
    std::vector<Inbound> inbound;
    NetClient::Get().Drain(inbound);
    for (const Inbound& in : inbound) {
        if (in.kind == Inbound::Event) {
            auto it = EventTable().find(EventType(in.event));
            if (it != EventTable().end()) {
                for (EventFn fn : it->second) {
                    fn(in.event);
                }
            }
        } else if (in.kind == Inbound::Stream) {
            if (in.stream.empty()) {
                continue;
            }
            auto it = StreamTable().find(in.stream[0]);
            if (it != StreamTable().end()) {
                for (StreamFn fn : it->second) {
                    fn(in.stream.data(), in.stream.size());
                }
            }
        } else {
            for (LostFn fn : LostTable()) {
                fn(in.reason);
            }
        }
    }
}

} // namespace coop::client
