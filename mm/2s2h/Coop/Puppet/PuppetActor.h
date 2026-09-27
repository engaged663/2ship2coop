#ifndef COOP_PUPPET_ACTOR_H
#define COOP_PUPPET_ACTOR_H

// En_CoopPuppet: the in-world body of a remote player (see coop/README.md).
// The struct starts with a full Player (same trick as EnTest3/Kafei) so the engine's
// own Player_Draw can render it exactly like the local Link.

#ifdef __cplusplus
extern "C" {
#endif
#include "z64.h"
#ifdef __cplusplus
}
#endif

// Actor params: low 4 bits = remote player id, next 4 bits = form (PLAYER_FORM_*).
#define COOP_PUPPET_PARAMS(playerId, form) ((s16)(((playerId) & 0xF) | (((form) & 0xF) << 4)))
#define COOP_PUPPET_GET_PLAYER_ID(thisx) ((thisx)->params & 0xF)
#define COOP_PUPPET_GET_FORM(thisx) (((thisx)->params >> 4) & 0xF)

typedef struct EnCoopPuppet {
    /* 0x000 */ Player player; // must stay first
    u8 playerId;
    u8 sword;       // remote sword level, swapped into the save only while drawing
    u8 hidden;      // the remote player's room is not loaded here
    u8 hasState;    // at least one pose applied
    u8 initialized; // Init completed (Destroy may run after a failed spawn)
} EnCoopPuppet;

#endif // COOP_PUPPET_ACTOR_H
