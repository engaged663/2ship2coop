# 2 Ship 2 Harkinian — Online Co-op Mod

An online co-op mod for [2 Ship 2 Harkinian](README.md): up to 4 players share one Termina — the same
three-day clock, the same unique items and quest flags, the same enemies, props and NPCs — through a small
dedicated server.

> **Status:** work in progress. The server and protocol (v18) are covered by an automated test suite
> (460+ tests); the in-game side is verified by compiling and by manual play-testing, so expect rough edges.
> Bug reports are welcome.

You need your own legally obtained copy of the game, exactly as for 2 Ship itself. This repository contains no
copyrighted assets.

## What it does

- **Shared world:** one server-owned save. Chests, heart pieces, masks, songs, quest flags and owl statues are shared;
  each player keeps their own inventory. Progress lives on the server, never in your local saves, with automatic
  backups. See [Saving and backups](#saving-and-backups).
- **Bring your own save:** `2ship-coop-convert` turns a normal 2 Ship save into the server's world. See
  [Converting a base-game save](#converting-a-base-game-save).
- **Shared clock:** the three-day cycle belongs to the server and never stops; the Song of Time is a vote.
- **Shared enemies, props, NPCs and Epona** (including multiple horses and passengers between scenes).
- **Groups:** invite players into a group to share dialogues, cutscenes, minigames (together / each on their own /
  by turns), quests and boss cutscenes.
- **Chat and commands:** `/tp`, `/gift`, `/pm`, groups, moderation (`op`, `ban`, `kick`), world tools.
  Server texts in English, Spanish, Chinese and Russian (`"language"` in `server.json`, or `/lang`).
- **Server mods:** Lua scripts and DLL plugins on the server can change the whole experience — give items, heal or
  hurt, spawn enemies, warp players, add commands, bend the clock, force 2 Ship difficulty options — while players
  keep using the same `2ship.exe`. See [Mods](#mods).
- **Game mods (.o2r):** the server can share 2 Ship mods (models, textures...). On connecting, each game downloads the
  ones it lacks, loads them without restarting and only then enters the server's world. See
  [Game mods (.o2r)](#game-mods-o2r).
- **Total sync:** what happens in a scene sounds and looks the same in every game: the other Links' sounds (steps,
  sword, voice, items) and those of the enemies and NPCs, boss and miniboss music and quakes, the others' arrows,
  bombs, hookshot, Zora fins and Elegy statues, what they carry in their hands, their sword trail, the
  scene's switches/chests/cleared rooms live (walls and blocks that depend on them are recreated), platforms, lifts
  and push blocks (run by whoever stands on them) and their ocarina (the notes as they play them, and the song with
  its music and its effect when they play it right). Each part can be switched off in
  F1 → Co-op → *Sincronización* or in `server.json`.

## Quick start

1. Download a release (or build, see below) and extract the game and the server.
2. Start `2ship-coop-server.exe`. It listens on **UDP 7780** (allow it through the firewall; forward the port if
   friends connect over the Internet) and creates `server.json` with the defaults.
3. In the game open **F1 → Co-op**, set the server address, a nick and (optionally) the password, then connect.
   Enabling *auto-connect* and *auto-enter* drops you straight into the server's world.
4. Press **Enter** to chat.

`server.json` options: `port`, `maxPlayers`, `password`, `motd`, `language`, `sharedEnemies`, `sharedProps`,
`groups`, `inviteSeconds`, `bossCutscenes`, `effects`, `endingForAll`, `timeSpeed` (how fast the three days pass),
`voteSeconds`, `saveSeconds`, `backupMinutes`, `backupKeep`, `giftMax`, `commandPermissions` (who may use each
command), `gameSettings` (2 Ship
options forced on everyone in the server's world), `mods` (below), `o2r` (the game mods the players download) and the
total sync's parts (all `true` by default): `sounds`, `ambient` (music and quakes), `playerObjects`, `sceneFlags`,
`sceneObjects` (platforms, lifts, switches, blocks), `ocarina` and `drops` (everything that falls — rupees,
hearts, ammo, fairies — is one item for everyone: the first one to touch it gets it).

To try it alone: `2ship-coop-bot.exe --target YourNick --mode mirror` makes a fake player.

## Saving and backups

The server saves the shared world (`world.json`) and each player's own data (`players/<nick>.json`) every
`saveSeconds` while something changes, when the world is created or starts a new cycle, and when it stops. Your game
uploads your data every 2 seconds while it changes, and again when you leave or close the game; **Save** in the pause
menu (2 Ship's *Pause Save*) writes it to the server's disk at once.

- **Backups** go to `backups/<date>-<reason>/` next to `world.json` (the world and every player's file): when the
  server starts with a world that changed, every `backupMinutes` (30; `0` = never) while it changes, and before the
  moon, a new cycle, `/reiniciar`, an import or a restore. The newest `backupKeep` (20) are kept. `/backup` (admins)
  makes one now (manual copies are never removed by themselves); `/backup lista` lists them.
- **Restoring:** `/restaurar <name|ultima>` in the server console puts a backup back while the server runs: everyone
  in the world switches to it.
- **Damaged files are never overwritten:** they are moved aside (`world.json.bad-<date>`,
  `players/<nick>.json.bad-<date>`), and a damaged `world.json` comes back from the newest backup. A `world.json` saved
  by another version of the mod still loads (new parts start empty).
- `/unlockall <player>` (admins) gives every item, mask, song and heart — the shared ones for the whole world — and it
  is saved like any other progress. It and `/give` only reach players inside the server's world. In a world where its
  first version was used, it also repairs what that one broke (Deku stick and nut capacities, two unused songs, more
  than 20 hearts).

## Converting a base-game save

`2ship-coop-convert.exe` (next to the server) turns a 2 Ship save — `saves/file1.json` of your normal game — into the
server's world: drag the file onto the exe in the server's folder, type the nick of the player who owns the save and
start the server. It shows a summary (hearts, masks, songs...), keeps a backup of the world that was there and refuses
to write while that server is running (use `/importar` in its console instead). If the save lacks what a co-op world
needs to work (a save still in its first cycle: no Ocarina, no Song of Time...), that is added and listed. An owl save
keeps its day and time and starts at its owl statue; otherwise the world starts at the Dawn of the First Day.
Randomizer saves are not supported.

```
2ship-coop-convert saves/file1.json --nick Ana --out C:\coop\server
2ship-coop-convert saves/file2.json --nick Ben --player-only     # only Ben's own items, for the world already there
```

Options: `--source auto|owl|cycle`, `--info` (summary only), `--yes` (replace an existing world without asking),
`--force`, `--lang es|en|zh|ru`. In the server console: `/importar <file> [nick] [auto|buho|ciclo]`.

## Mods

The server loads **Lua 5.4 scripts** from `mods/` and **plugins** (`.dll` / `.so`) from `plugins/`. Both use one API:
named functions (`coop.chat.tell(player, text)`, `coop.game.spawn("*", "EN_DODONGO")`, `coop.world.setTime(3, 22)`...)
and events (`player_join`, `chat`, `enemy_killed`, `world_hour`...), with commands, timers, per-mod storage and
settings. Orders to the games (items, health, messages, actors, warps, forced options) only reach players inside the
server's world, never their own saves, and each player can refuse them (F1 → Co-op → *Permitir los mods del
servidor*).

```lua
-- mods/hello.lua
coop.on("player_join", function(e) coop.chat.broadcast(e.nick .. " is here!") end)
coop.commands.register("heal", { help = "heals you" }, function(ctx)
    coop.game.heal(ctx.player)
    return "Healed.", "ok"
end)
```

`/mods` lists them; `/mod reload [name]` reloads (admins), `/mod load` / `/mod unload` (console). Command line:
`--mods-dir`, `--plugins-dir`, `--script F`, `--plugin F`, `--no-mods`, `--mod-docs DIR` (writes the reference).
Documentation (in Spanish): [`coop/docs/mods/README.md`](coop/docs/mods/README.md) (guide),
[`API.md`](coop/docs/mods/API.md) (every function and event, generated from the code),
[`IDS.md`](coop/docs/mods/IDS.md) (item, actor and scene names), [`PLUGINS.md`](coop/docs/mods/PLUGINS.md) (C/C++ SDK);
example script [`coop/mods/ejemplo.lua`](coop/mods/ejemplo.lua).

## Game mods (.o2r)

Put 2 Ship mods (the `.o2r` files that normally go in the game's `mods` folder) in the server's **`o2r/`** folder.
The server announces them when a game connects (name, size and SHA-256); the game asks once whether to download what
it lacks (F1 → Co-op can download them without asking), saves them in **`coop_mods/`** next to `2ship.exe`, checks
their SHA-256, loads them while running and only then enters the server's world. While playing there, if the mods
have alternate assets (`alt/`, like most model mods), 2 Ship's **Enable Mods** is switched on and given back on
leaving. Downloaded mods stay loaded until the game is closed and are never added to your own `mods` folder.

`server.json` → `"o2r": {"dir": "o2r", "files": ["*"]}` (`"*"` = every `.o2r` of the folder in name order; a list of
names sets the order of loading, the last one wins; `"!name"` leaves one out). Command line: `--o2r-dir D`,
`--no-o2r`. `/mods` lists them too. The files are read when the server starts: restart it after changing them.
Music and sound changes of a mod do not apply (2 Ship loads the audio at start).

## Building

Same prerequisites as 2 Ship (see [`docs/BUILDING.md`](docs/BUILDING.md)), plus one extra step for the co-op host:

```
git clone --recursive <this repo>
./coop/patches/apply-libultraship-patch.sh      # Windows: .\coop\patches\apply-libultraship-patch.ps1
```

That applies a small windowless-backend patch to the `libultraship` submodule (see
[`coop/patches/README.md`](coop/patches/README.md)). Then build the game as usual; the server, bot and tests are
part of the same CMake project (`add_subdirectory(coop)`), or build only them:

```
cmake -S coop -B build/coop -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build/coop --config Release --parallel
build/coop/Release/coop-tests.exe
```

Game and server must be the same build (the handshake checks it).

## Developer docs

- [`coop/README.md`](coop/README.md) — code map, recipes, CVars, security notes (in Spanish).
- [`coop/docs/mods/`](coop/docs/mods) — the mod API: guide, generated reference, game ids, plugin SDK (in Spanish).
- [`docs/superpowers/specs/`](docs/superpowers/specs) and [`docs/superpowers/plans/`](docs/superpowers/plans) — design
  documents and implementation plans for each part (in Spanish).

Code touched outside `coop/` and `mm/2s2h/Coop/` is marked `[COOP]`.

## License and credits

Same license as 2 Ship 2 Harkinian (see [LICENSE](LICENSE)). All credit for the port goes to the 2 Ship 2 Harkinian
and libultraship teams; this mod is an unofficial fork and is not affiliated with them or with Nintendo.
