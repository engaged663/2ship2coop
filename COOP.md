# 2 Ship 2 Harkinian — Online Co-op Mod

An online co-op mod for [2 Ship 2 Harkinian](README.md): up to 4 players share one Termina — the same
three-day clock, the same unique items and quest flags, the same enemies, props and NPCs — through a small
dedicated server.

> **Status:** work in progress. The server and protocol (v15) are covered by an automated test suite
> (385 tests); the in-game side is verified by compiling and by manual play-testing, so expect rough edges.
> Bug reports are welcome.

You need your own legally obtained copy of the game, exactly as for 2 Ship itself. This repository contains no
copyrighted assets.

## What it does

- **Shared world:** one server-owned save. Chests, heart pieces, masks, songs, quest flags and owl statues are shared;
  each player keeps their own inventory. Progress lives on the server, never in your local saves.
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
  and push blocks (run by whoever stands on them) and the notes of their ocarina. Each part can be switched off in
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
`voteSeconds`, `saveSeconds`, `giftMax`, `commandPermissions` (who may use each command), `gameSettings` (2 Ship
options forced on everyone in the server's world), `mods` (below), `o2r` (the game mods the players download) and the
total sync's parts (all `true` by default): `sounds`, `ambient` (music and quakes), `playerObjects`, `sceneFlags`,
`sceneObjects` (platforms, lifts, switches, blocks) and `ocarina`.

To try it alone: `2ship-coop-bot.exe --target YourNick --mode mirror` makes a fake player.

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
