// gCoop.Debug.FieldSelfTest, second half: the save converter (coop/common/SaveImport.h, which works outside the game)
// must read a save exactly as FieldTable does. The live save goes through 2 Ship's own JSON (BenJsonConversions.hpp:
// what saves/fileN.json holds) and through the converter, and the results are compared with FieldTable field by field.
#include "FieldTable.h"

#include "common/SaveImport.h"
#include "common/WorldFields.h"

#include "2s2h/BenJsonConversions.hpp"

#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client::fields {

std::string ImportCrossCheck() {
    if (PlayLive()) {
        Play_SaveCycleSceneFlags(gPlayState); // the loaded scene's flags into cycleSceneFlags, as saving does
    }
    static Save copy; // too big for the stack
    copy = gSaveContext.save;
    // As func_8014546C before writing a save: the cycle's scene flags travel in permanentSceneFlags
    for (int i = 0; i < save::kSceneCount; i++) {
        PermanentSceneFlags& out = copy.saveInfo.permanentSceneFlags[i];
        const CycleSceneFlags& in = gSaveContext.cycleSceneFlags[i];
        out.chest = in.chest;
        out.switch0 = in.switch0;
        out.switch1 = in.switch1;
        out.clearedRoom = in.clearedRoom;
        out.collectible = in.collectible;
    }
    json file = { { "type", "2S2H_SAVE" }, { "version", 8 }, { "newCycleSave", { { "save", copy } } } };
    save::ImportOptions opts;
    opts.source = save::ImportSource::Cycle;
    opts.baseline = false;    // the save as it is
    opts.loaderRules = false; // its form too
    save::Imported imported;
    std::string err;
    if (!save::ImportSave(file, opts, imported, &err)) {
        return "SaveImport: " + err;
    }
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < world::kFieldCount; i++) {
        bytes.assign(world::kFields[i].size, 0);
        Read((int)i, bytes.data());
        if (bytes != imported.fields[i]) {
            return std::string("el conversor lee distinto el campo del mundo '") + world::kFields[i].name + "'";
        }
    }
    json own = ReadPlayer();
    const json& converted = imported.inv["fields"];
    for (auto it = own.begin(); it != own.end(); ++it) {
        if (!converted.contains(it.key()) || converted[it.key()] != it.value()) {
            return std::string("el conversor lee distinto el dato del jugador '") + it.key() + "'";
        }
    }
    return "";
}

} // namespace coop::client::fields
