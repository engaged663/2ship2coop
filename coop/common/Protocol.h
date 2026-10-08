#pragma once
// Single source of truth for the co-op wire protocol (client + server).
// Bump kProtocolVersion whenever an event's meaning or the binary stream layout changes.
#include <cstddef>
#include <cstdint>

namespace coop {

constexpr uint32_t kProtocolVersion = 19; // v19: shared drops (item, item_take, item_gone, item_rest, items;
                                          // welcome sync "drops");
                                          // v18: saving (world_full reset "import"/"restore", inv "save",
                                          // /unlockall only in the world, inv "entrance" of converted saves);
                                          // v17: activity rooms (room_op, room, room_invite, room_chat, rooms),
                                          // room prizes and trips (act_reward/follow/tp "room", "all");
                                          // v16: continuous sounds (without the 0x800 bit) travel, "song";
                                          // v15: total sync (sounds in poses and actor records, scene + layer, live
                                          // scene flags, ambient music and quakes, the players' objects);
                                          // v14: the server's game mods (.o2r): list, download, world_enter check;
                                          // v13: mods (orders, forced settings, game reports), the clock's speed;
                                          // v12: effects echo, results, talk values; v11: groups; v10: the ending
constexpr uint16_t kDefaultPort = 7780; // UDP

constexpr int kMaxPlayers = 4;
constexpr int kMaxHosts = 4;           // headless games of the server (sub-project D): one per busy scene
inline constexpr const char* kHostNick = "#host"; // their nick (no player can take it: '#' is not allowed)
constexpr int kNickMin = 3;
constexpr int kNickMax = 16;
constexpr int kChatMaxChars = 150;    // after sanitizing, in UTF-8 code points
constexpr int kCommandMaxChars = 200; // raw "/..." line
constexpr size_t kMaxClientEventBytes = 12288;  // biggest event the server accepts (a whole world fits)
constexpr size_t kMaxServerEventBytes = 262144; // biggest event a game accepts from the server
constexpr int kRateLimitCount = 6; // chat + cmd messages...
constexpr int kRateLimitWindowMs = 3000; // ...per window
constexpr int kHandshakeTimeoutMs = 5000;
constexpr int kGiftTimeoutMs = 10000;
constexpr int kGiftMaxAmount = 999;
constexpr int kGiftOrphanMs = 60000;    // an unpaid gift that was cancelled still refunds a late payment
constexpr int kInvalidLogCount = 3;     // invalid packets logged per connection...
constexpr int kInvalidKickCount = 50;   // ...and how many get it kicked
constexpr size_t kMaxPacketBytes = 16384; // ENet refuses to reassemble anything bigger (server)
constexpr int kStreamBurst = 40;        // pose packets relayed per player: burst...
constexpr int kStreamPerSecond = 30;    // ...and sustained rate (clients send 20/s)
constexpr int kLocBurst = 10;           // location broadcasts per player: burst...
constexpr int kLocPerSecond = 5;        // ...and sustained rate

// Shared world (sub-project B)
constexpr int kWopsBurst = 60;          // world change packets per player: burst...
constexpr int kWopsPerSecond = 30;      // ...and sustained rate (games send at most 10/s)
constexpr int kInvBurst = 10;           // inventory uploads per player: burst...
constexpr int kInvPerSecond = 2;        // ...and sustained rate (games send one every 5 s)
constexpr size_t kMaxInventoryBytes = 6144; // with the whole world it must fit in one world_full (< kMaxPacketBytes)
constexpr int kMaxInventoryDepth = 8;       // nested objects/arrays in an inventory (the games send 3)
constexpr int kWorldEntryBurst = 6;         // entering/leaving the server's world per player: burst...
constexpr double kWorldEntryPerSecond = 0.5; // ...and sustained rate (a menu click, a reconnection)
constexpr int kClockBroadcastMs = 1000; // the clock goes to everyone in the world this often (and on changes)
constexpr int kWorldSaveMs = 10000;     // world.json and players/ are saved this often
constexpr int kBackupMs = 30 * 60000;   // a backup this often while the world changes (server.json "backupMinutes")
constexpr int kBackupKeep = 20;         // automatic backups kept (server.json "backupKeep"; manual ones always stay)
constexpr int kSotVoteMs = 30000;       // Song of Time vote
constexpr int kCycleComputeMs = 15000;  // a game has this long to compute the new cycle
constexpr int kWorldCreateMs = 15000;   // the first player has this long to create the world

// Shared enemies (sub-project C)
constexpr int kActorStreamBurst = 400;    // actor stream packets per player: burst...
constexpr int kActorStreamPerSecond = 300; // ...and sustained rate (D3: full memory, several parts per room)
constexpr int kHostActorStreamBurst = 2000;    // the server's own games simulate whole scenes
constexpr int kHostActorStreamPerSecond = 1500;
constexpr int kLeaseBurst = 160;           // lease_req/lease_drop/echo events per player: burst...
constexpr int kLeasePerSecond = 120;       // ...and sustained rate (a request per held actor twice a second: a
                                           // director's full hand of kMaxLeasesPerPlayer is 96 a second)
constexpr int kMaxLeasesPerPlayer = 48;    // a minigame's director borrows its NPC and props (the beavers' rings)
constexpr uint32_t kRuntimeKeyBits = 0xC0000000u; // keys of runtime/derived actors (only list actors are lent)
constexpr int kMaxActorId = 0x2FF;         // above every actor of the game's table
constexpr int kHitBurst = 30;             // hit + hurt events per player: burst...
constexpr int kHitPerSecond = 20;         // ...and sustained rate
constexpr int kDropBurst = 20;            // drop events per player: burst...
constexpr int kDropPerSecond = 10;        // ...and sustained rate
// Shared props (pots, grass, crates, rupees lying around) and the ending
constexpr int kPropBurst = 200;            // prop/item events per player: burst...
constexpr int kPropPerSecond = 100;        // ...and sustained rate (a spin attack in a field of grass)
constexpr size_t kMaxPropsPerScene = 4000; // what the server remembers as gone per occupied scene
// Shared drops (docs/superpowers/specs/2026-10-07-coop-drops-compartidos-design.md)
constexpr size_t kMaxItemsPerStage = 300;       // items lying in one stage that the server keeps for latecomers
constexpr size_t kMaxTakenItemsPerStage = 4000; // keys taken in one stage it remembers (the oldest are forgotten)
constexpr int kItemBurst = 200;                 // item, item_take, item_rest, items per player: burst...
constexpr int kItemPerSecond = 100;             // ...and sustained rate (a spin attack in a field of grass)
// The end of the game (EndingHandlers.cpp)
constexpr int kEndingBurst = 80;          // ending, ending_sync, cinema, rooftop events per player: burst...
constexpr int kEndingPerSecond = 40;      // ...and sustained rate (the cinema camera goes 20 times a second)
constexpr int kRooftopMaxMs = 600000;     // the rooftop's countdown (the original starts it at 5 minutes)
constexpr int kMaxHurtDamage = 0x40;      // 4 hearts: more than any enemy of the list does
// Groups and activities (GroupHandlers.cpp, ActivityHandlers.cpp)
constexpr int kInviteBurst = 10;          // /invitar, /aceptar, /rechazar per player: burst...
constexpr double kInvitePerSecond = 1.0;  // ...and sustained rate
constexpr int kActivityBurst = 120;       // act, act_hud, follow, talk, title per player: burst...
constexpr int kActivityPerSecond = 60;    // ...and sustained rate (act_hud goes 10 times a second)
constexpr int kRewardBurst = 10;          // act_reward per player: burst...
constexpr double kRewardPerSecond = 2.0;  // ...and sustained rate (a minigame's rupees go every half second)
constexpr int kMaxRewardRupees = 500;     // rupees in one act_reward
constexpr int kMaxActivityKey = 32;       // characters of an activity key ([a-z0-9_])
constexpr int kMaxActivityName = 64;      // characters of its name
constexpr int kInviteMs = 60000;          // an invitation lasts this long (server.json inviteSeconds)
// Groups' fixes (docs/superpowers/specs/2026-09-30-coop-grupos-limites-design.md)
constexpr int kEffectBurst = 60;             // effects packets per player: burst...
constexpr int kEffectPerSecond = 40;         // ...and sustained rate (a game sends at most one per frame)
constexpr int kCinemaActorsMs = 1000;        // a game's actor frames pass this long after its last "cinema": its
                                             // cutscene actors, for who watches (whoever owns the room)
constexpr size_t kMaxTalkVarsHex = 512;      // "vars" of a talk event: the talker's text values, in hex
constexpr int64_t kMaxActivityCs = 36000000; // centiseconds of a minigame's time (100 hours)
// Mods (docs/superpowers/specs/2026-10-02-coop-mods-api-design.md)
constexpr double kMinTimeSpeed = 0.1;        // server.json timeSpeed: how fast the three days pass...
constexpr double kMaxTimeSpeed = 10.0;       // ...1 is the original game
constexpr int kMaxModSettingName = 96;       // characters of the name of a game setting a server forces
constexpr int kMaxModSettings = 64;          // game settings forced on one player
constexpr int kModEventBurst = 60;           // gev events per player: burst...
constexpr int kModEventPerSecond = 30;       // ...and sustained rate (a spin attack through a field of rupees)
constexpr int kStatBurst = 10;               // stat events per player: burst...
constexpr int kStatPerSecond = 4;            // ...and sustained rate (a game sends at most 4 a second)
constexpr int kMaxModText = 400;             // characters of the text of a notify/message order
constexpr int kMaxModOpsPerEvent = 16;       // orders in one "mod" event
constexpr int kMaxModHealth = 2000;          // health and damage of an order or a report (20 hearts are 320)
constexpr int kMaxModRupees = 9999;          // rupees of an order (the biggest wallet holds 500; the rest is lost)
constexpr int kMaxModSpawnDistance = 2000;   // how far in front of Link an order may create an actor
constexpr int kLastModItem = 0xA3;           // the last id of the game's item table that is something to give
// Game mods (.o2r) the server shares (docs/superpowers/specs/2026-10-03-coop-mods-o2r-design.md)
constexpr int kMaxO2rFiles = 32;                 // .o2r files one server shares
constexpr uint64_t kMaxO2rBytes = 0xFFFFFFFFull; // one file (chunk offsets are 32 bits)
constexpr int kMaxO2rName = 80;                  // characters of its name
constexpr size_t kO2rChunkBytes = 16000;         // bytes of one chunk (with its header, under kMaxPacketBytes)
constexpr int kO2rWindow = 16;                   // chunks a game asks for before the first one arrives
constexpr int kO2rQueueMax = 32;                 // o2r_get waiting on the server per player (more = invalid)
constexpr int kO2rBurst = 32;                    // chunks served per player: burst...
constexpr int kO2rPerSecond = 256;               // ...and sustained rate (about 4 MB/s)
constexpr int kO2rStallMs = 20000;               // a game gives the download up after this long without a chunk
// Total sync (docs/superpowers/specs/2026-10-04-coop-sincronizacion-total-design.md)
constexpr int kSceneFlagBurst = 40;     // sflag + sflags per player: burst...
constexpr int kSceneFlagPerSecond = 20; // ...and sustained rate (a game sends at most one sflag a frame)
constexpr int kAmbientBurst = 40;       // ambient events per player: burst...
constexpr int kAmbientPerSecond = 20;   // ...and sustained rate (one per room a frame)
constexpr int kSongBurst = 4;           // song events per player: burst...
constexpr double kSongPerSecond = 0.5;  // ...and sustained rate (a song lasts several seconds)
constexpr int kMaxSong = 13;            // OCARINA_SONG_DOUBLE_TIME: the last song with music of its own
// Activity rooms (docs/superpowers/specs/2026-10-04-coop-salas-actividades-design.md)
constexpr int kRoomOpBurst = 30;          // room_op per player: burst...
constexpr double kRoomOpPerSecond = 10.0; // ...and sustained rate (a click each; "chat" also passes the chat limit)
constexpr int kRoomCountdownMs = 3000;    // everyone is ready: the room runs this long after (alone: at once)
constexpr int kRoomLobbyMs = 600000;      // a room waiting this long for its confirmations closes (its director's
                                          // game waits meanwhile)
constexpr int kRoomLingerMs = 120000;     // a room whose activity ended stays this long: prizes, chat, another round

enum Channel : uint8_t {
    kChannelEvents = 0, // reliable + ordered, JSON events
    kChannelStream = 1, // unreliable + sequenced, binary streams
    kChannelFiles = 2,  // reliable + ordered, binary, server -> game only: .o2r chunks (common/O2r.h)
    kChannelCount = 3,
};

// Event names (JSON field "t"). Direction and fields are documented in coop/README.md.
namespace ev {
inline constexpr const char* kHello = "hello";             // C->S proto, nick, pass, build (+ host, token: the server's own game)
inline constexpr const char* kWelcome = "welcome";         // S->C id, nick, motd, players[], o2r[{name, size, sha256}]
                                                           // (the game mods to have first: only if there are some),
                                                           // sync{sounds, ambient, playerObjects, sceneFlags,
                                                           // sceneObjects, ocarina} (what this server shares)
inline constexpr const char* kReject = "reject";           // S->C reason
inline constexpr const char* kKicked = "kicked";           // S->C reason
inline constexpr const char* kJoin = "join";               // S->C id, nick
inline constexpr const char* kLeave = "leave";             // S->C id, nick, reason
inline constexpr const char* kLoc = "loc";                 // C->S scene, room, layer, entrance, sceneName, timeStopped, busy; S->C id, scene, sceneName, entrance
inline constexpr const char* kChat = "chat";               // C->S text; S->C from, text
inline constexpr const char* kPm = "pm";                   // S->C from, to, text
inline constexpr const char* kCmd = "cmd";                 // C->S line
inline constexpr const char* kSys = "sys";                 // S->C text, level
inline constexpr const char* kTp = "tp";                   // S->C scene, entrance, room, pos[3], rot, target,
                                                           // roomTrip (an activity room's trip: retried)
inline constexpr const char* kGiftDebit = "gift_debit";    // S->C gid, to, amount
inline constexpr const char* kGiftPaid = "gift_paid";      // C->S gid, paid
inline constexpr const char* kGiftCredit = "gift_credit";  // S->C gid, from, amount
inline constexpr const char* kGiftRecv = "gift_recv";      // C->S gid, accepted
inline constexpr const char* kGiftRefund = "gift_refund";  // S->C gid, amount, reason
// Shared world (sub-project B)
inline constexpr const char* kWorldEnter = "world_enter";     // C->S o2r[sha256...] (asks to play in the server's
                                                              // world; o2r: the server's .o2r this game has loaded)
inline constexpr const char* kWorldLeave = "world_leave";     // C->S
inline constexpr const char* kWorldFull = "world_full";       // S->C create, fields{}, cycle, clock{}, you{inv, stale}, reset
                                                              // ("" entering; "sot", "moon": a new cycle, back to the
                                                              // clock tower; "import", "restore": a whole other world,
                                                              // each one to their own place)
inline constexpr const char* kWorldInit = "world_init";       // C->S fields{} (the creator, once)
inline constexpr const char* kWops = "wops";                  // C->S bits[], bytes[], adds[], cycle; S->C same + from
inline constexpr const char* kInv = "inv";                    // C->S inv{} (this player's own data, opaque), cycle (the world cycle it belongs to),
                                                              // save (true: on disk at once, "Save" in the pause menu)
inline constexpr const char* kClock = "clock";                // S->C abs, inv, stopped, jump
inline constexpr const char* kClockJump = "clock_jump";       // C->S (Song of Double Time)
inline constexpr const char* kClockSpeed = "clock_speed";     // C->S inv (Inverted Song of Time)
inline constexpr const char* kSotPropose = "sot_propose";     // C->S (Song of Time: starts a vote)
inline constexpr const char* kCycleCompute = "cycle_compute"; // S->C (run the end-of-cycle rules, send the world)
inline constexpr const char* kCycleResult = "cycle_result";   // C->S fields{}
// Shared enemies (sub-project C)
inline constexpr const char* kAuth = "auth"; // S->C scene, rooms[[room, id]...] (who simulates each room's enemies)
inline constexpr const char* kHit = "hit";   // C->S scene, room, key, col, elem, dmgFlags, effect, damage, hitEffect, pos[3], attackerId, form; S->C + from
inline constexpr const char* kHurt = "hurt"; // C->S to, kind, dmgFlags, effect, damage, hitEffect, pos[3], knock{}; S->C + from
inline constexpr const char* kDrop = "drop"; // C->S scene, room, pos[3], params, fn; S->C + from
// Server-side simulation (sub-project D)
inline constexpr const char* kHostFollow = "host_follow"; // S->C (host only) id: the player whose room to keep loaded
inline constexpr const char* kLeaseReq = "lease_req";   // C->S scene, room, key, dist, talking: lend me this NPC
inline constexpr const char* kLeaseDrop = "lease_drop"; // C->S scene, room, key
inline constexpr const char* kLeases = "leases";        // S->C scene, list [[room, key, id]]: who simulates which NPC
inline constexpr const char* kEcho = "echo";            // C->S->C scene, room, id, params, pos[3], rot[3] (+ from):
                                                        // every game creates its own copy (warps, hearts, fairies)
// Admin commands (sub-project D3: /unlockall, /give)
inline constexpr const char* kUnlockAll = "unlock_all"; // S->C nick: give this player every item, song and heart
inline constexpr const char* kGive = "give";            // S->C nick, amount: rupees to the wallet, the rest to the bank
// Shared props and the ending (PropSync.cpp, Ending.cpp)
inline constexpr const char* kProp = "prop";     // C->S scene, key, child: gone (child -1 a prop of the room's list,
                                                 // 0..15 a grass group's grass, -2 field grass,
                                                 // -4 grass that grows back was cut: not remembered);
                                                 // S->C + from
inline constexpr const char* kProps = "props";   // C->S scene (just arrived); S->C scene, list [[key, child]...]
// Shared drops (Sync/SharedDrops.cpp, Handlers/ItemHandlers.cpp)
inline constexpr const char* kItem = "item";          // C->S scene, key, id, params, pos[3] (+ a collectible: vy,
                                                      // speed, grav, scale, yaw, phase, timer, act): an item fell
                                                      // here (common/ItemState.h); S->C + from
inline constexpr const char* kItemTake = "item_take"; // C->S scene, key, after (taken already: fairies); S->C scene,
                                                      // key, ok (yours / someone was first)
inline constexpr const char* kItemGone = "item_gone"; // S->C scene, key, from: someone took it
inline constexpr const char* kItemRest = "item_rest"; // C->S scene, key, pos[3] (where its dropper's came to lie);
                                                      // S->C + from
inline constexpr const char* kItems = "items";        // C->S scene, layer (just arrived); S->C scene, list [item +
                                                      // age, rest], taken [keys of list items and twins]
// The end of the game, together (Ending.cpp, EndingMode.cpp, RooftopTimer.cpp, MajoraCinema.cpp)
inline constexpr const char* kEnding = "ending"; // C->S stage (1 the Clock Tower's rooftop, 2 Majora's lair,
                                                 // 3 Majora defeated, 4 the ending); S->C + from, nick
inline constexpr const char* kEndingSync = "ending_sync"; // C->S seg, scene, cs, frame: where this game is in the
                                                          // ending's cutscenes; S->C + from
inline constexpr const char* kEndingDone = "ending_done"; // C->S (the ending is over): the world starts a new cycle
inline constexpr const char* kRooftop = "rooftop";        // C->S ms (the rooftop's countdown started here);
                                                          // S->C ms (what is left; -1: stopped)
inline constexpr const char* kCinema = "cinema";          // C->S eye[3], at[3], fov, roll, fill[4], bgm, scope, blur:
                                                          // the camera of a cutscene; S->C + from
// Groups and activities (docs/superpowers/specs/2026-09-30-coop-grupos-actividades-design.md)
inline constexpr const char* kGroup = "group";          // S->C id (0: none), leader, members [[id, nick]...], activity, name
inline constexpr const char* kInvite = "invite";        // S->C from, nick, activity, name, ms
inline constexpr const char* kInviteEnd = "invite_end"; // S->C from, nick, reason (accepted, declined, expired, cancelled, full)
inline constexpr const char* kAct = "act";              // C->S state (here, start, end, result, none), key, name, score,
                                                        // cs, won; S->C + from, nick
inline constexpr const char* kActHud = "act_hud";       // C->S key, score, hidden, status, mstate, perfect, ammo, b, bomb,
                                                        // chu, arrows, frozen, sub, pos[3], rot, timer{}; S->C + from
inline constexpr const char* kActReward = "act_reward"; // C->S gi | rupees, room (to the room); S->C + from, nick
inline constexpr const char* kFollow = "follow";        // C->S entrance, cs, trans, scope, key, all (the whole room
                                                        // goes, wherever it is); S->C + from
inline constexpr const char* kTalk = "talk";            // C->S op (open, id, page, choice, close), id, page, choice,
                                                        // scope, vars; S->C + from
inline constexpr const char* kTitle = "title";          // C->S tex, x, y, w, h, scope: a boss's title card; S->C + from
// Activity rooms (docs/superpowers/specs/2026-10-04-coop-salas-actividades-design.md)
inline constexpr const char* kRoomOp = "room_op";       // C->S op (open: key, name, mode, place | ready: ready | invite:
                                                        // to | group | answer: room, accept | leave | kick: who | close |
                                                        // settings: open, travel, rewards | join: room | chat: text)
inline constexpr const char* kRoom = "room";            // S->C id (0: none, + reason), key, name, mode, place, state, ms,
                                                        // host, director, members [{id, nick, ready, here, score, cs,
                                                        // won}], invites [{id, nick, ms}], settings {open, travel, rewards}
inline constexpr const char* kRoomInvite = "room_invite";        // S->C room, from, nick, key, name, ms
inline constexpr const char* kRoomInviteEnd = "room_invite_end"; // S->C room, from, reason
inline constexpr const char* kRoomChat = "room_chat";   // S->C room, from, nick, text (the room's own chat)
inline constexpr const char* kRooms = "rooms";          // S->C list [{id, host, nick, key, name, count, state}]: the
                                                        // open rooms anyone in the world may join
// Mods (server: Mods/Api/ApiGame.cpp, Handlers/ModHandlers.cpp; game: Mods/)
inline constexpr const char* kMod = "mod";        // S->C ops [{op, ...}]: orders of the server's mods for this game
                                                  // (notify, message, sfx, item, take, rupees, heal, damage, kill,
                                                  // magic, spawn, warp: coop/README.md "Mods")
inline constexpr const char* kModCfg = "mod_cfg"; // S->C settings {name: number}: every game setting forced on it
inline constexpr const char* kGameEvent = "gev";  // C->S k (item: id | death | boss: actor |
                                                  // kill: actor, params, by, room, pos[3]): it happened in this game
inline constexpr const char* kStat = "stat";      // C->S hp, hpMax, mp, rupees (when they change, 4 a second at most)
// Game mods (.o2r) (server: O2rStore.h, Handlers/O2rHandlers.cpp; game: O2r/)
inline constexpr const char* kO2rGet = "o2r_get"; // C->S i, off: a chunk of the server's .o2r number i (welcome.o2r),
                                                  // from byte off; it comes back on kChannelFiles
// Total sync (docs/superpowers/specs/2026-10-04-coop-sincronizacion-total-design.md)
inline constexpr const char* kSceneFlag = "sflag";   // C->S scene, ops [[word, set, clear]...] (SceneFlags.h);
                                                     // S->C + from, to everyone in that scene + layer, sender too
inline constexpr const char* kSceneFlags = "sflags"; // C->S scene, words [11] (it just arrived); S->C scene, words
                                                     // (the temporary ones the server keeps for that scene + layer)
inline constexpr const char* kAmbient = "ambient";   // C->S scene, room, music [[kind, value]...], quake [[type, speed,
                                                     // y, x, fov, roll, duration]...] (Ambient.h); S->C + from
inline constexpr const char* kSong = "song";         // C->S scene, song (0..kMaxSong), form: our Link played that song
                                                     // right; S->C + from, to everyone else in that scene (they hear
                                                     // it and see its effect near its puppet: Sync/OcarinaEcho.cpp)
// Epona
inline constexpr const char* kEponaCall = "epona_call"; // C->S (+from stamped): one owner horse call
inline constexpr const char* kEponaPassenger = "epona_passenger"; // C->S owner, horse, mounted; S->owner +from
} // namespace ev

// Levels used by "sys" events.
namespace level {
inline constexpr const char* kInfo = "info";
inline constexpr const char* kOk = "ok";
inline constexpr const char* kWarn = "warn";
inline constexpr const char* kError = "error";
} // namespace level

} // namespace coop
