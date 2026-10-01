# 2 Ship 2 Harkinian — Online Co-op Mod

An online co-op mod for [2 Ship 2 Harkinian](README.md): up to 4 players share one Termina — the same
three-day clock, the same unique items and quest flags, the same enemies, props and NPCs — through a small
dedicated server.

> **Status:** work in progress. The server and protocol (v12) are covered by an automated test suite
> (238 tests); the in-game side is verified by compiling and by manual play-testing, so expect rough edges.
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

## Quick start

1. Download a release (or build, see below) and extract the game and the server.
2. Start `2ship-coop-server.exe`. It listens on **UDP 7780** (allow it through the firewall; forward the port if
   friends connect over the Internet) and creates `server.json` with the defaults.
3. In the game open **F1 → Co-op**, set the server address, a nick and (optionally) the password, then connect.
   Enabling *auto-connect* and *auto-enter* drops you straight into the server's world.
4. Press **Enter** to chat.

`server.json` options: `port`, `maxPlayers`, `password`, `motd`, `language`, `sharedEnemies`, `sharedProps`,
`groups`, `inviteSeconds`, `bossCutscenes`, `effects`, `endingForAll`.

To try it alone: `2ship-coop-bot.exe --target YourNick --mode mirror` makes a fake player.

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
- [`docs/superpowers/specs/`](docs/superpowers/specs) and [`docs/superpowers/plans/`](docs/superpowers/plans) — design
  documents and implementation plans for each part (in Spanish).

Code touched outside `coop/` and `mm/2s2h/Coop/` is marked `[COOP]`.

## License and credits

Same license as 2 Ship 2 Harkinian (see [LICENSE](LICENSE)). All credit for the port goes to the 2 Ship 2 Harkinian
and libultraship teams; this mod is an unofficial fork and is not affiliated with them or with Nintendo.
