#pragma once

#include <cstdint>
#include <vector>
#include "SceneCommand.h"
#include "2s2h/resource/type/Path.h"

namespace SOH {
class SetPathwaysMM : public SceneCommand<PathDataMM*> {
  public:
    using SceneCommand::SceneCommand;

    PathDataMM** GetPointer();
    size_t GetPointerSize();

    uint32_t numPaths;
    std::vector<PathDataMM*> paths;
    std::vector<uint32_t> pathCounts; // [COOP] entries of each list: the engine's Path* has no count
};

}; // namespace SOH
