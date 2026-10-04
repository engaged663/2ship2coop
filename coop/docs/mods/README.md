# Co-op server mods

A **mod** changes the shared game however you like: it gives items, heals or hurts, spawns enemies, moves players
from one place to another, adds commands, changes the clock or the world, forces 2 Ship difficulty options...
Mods live **on the server**: players install nothing (they use the same `2ship.exe` as always).

There are two kinds:

- **Lua scripts** (`mods/*.lua`): the usual choice. A text file, no compiling, hot-reloadable.
- **DLL plugins** (`plugins/*.dll`): the same in C++ (or C), for those who need threads, sockets, another library or
  lots of speed. See [PLUGINS.md](PLUGINS.md).

Both use **the same API**: the same functions and the same events, described one by one in
[API.md](API.md) (generated from the code, always up to date). The names of items, actors and scenes are in
[IDS.md](IDS.md). A complete, commented example: `mods/ejemplo.lua`.

**2 Ship mods** (the `.o2r` files with models, textures... that normally go in the game's `mods` folder) are a
different thing: put them in the server's **`o2r/`** folder (not in `mods/`) and every game downloads and loads them
before entering the server's world. See "Game mods (.o2r)" in `COOP.md`.

## Index

1. [Installing and loading mods](#1-installing-and-loading-mods)
2. [Your first script, step by step](#2-your-first-script-step-by-step)
3. [How a script works](#3-how-a-script-works)
4. [Commands to the players' games](#4-commands-to-the-players-games)
5. [Game options that can be forced](#5-game-options-that-can-be-forced)
6. [The shared world: fields and flags](#6-the-shared-world-fields-and-flags)
7. [Other server parameters](#7-other-server-parameters)
8. [Debugging](#8-debugging)
9. [Security and limits](#9-security-and-limits)
10. [Recipes](#10-recipes)
11. [Known limits](#11-known-limits)

## 1. Installing and loading mods

Next to the server:

```
2ship-coop-server.exe
server.json
mods/              the scripts (.lua); subfolders for what you load with require ("lib/util.lua")
mods/data/         what each mod saves (<mod>.json): the server creates it
plugins/           the plugins (.dll on Windows, .so on Linux)
docs/              this documentation
sdk/               what you need to build plugins
```

Copy a `.lua` into `mods/` (or a `.dll` into `plugins/`) and start the server: it loads everything in those folders,
in alphabetical order. With the server running:

| Command | Who | What it does |
|---|---|---|
| `/mods` | everyone | lists the loaded mods: name, kind, version, title and description |
| `/mod reload [name]` | admins and console | reloads a mod (or all of them) from its file; what it saved is still there |
| `/mod load <file>` | console | loads a file (from `mods/` or `plugins/`, or a path) |
| `/mod unload <name>` | console | unloads a mod (removes its events, timers and commands) |

A mod's **name** is its file name in lowercase and without extension (`mods/My Mod.lua` is `my_mod`).

### `server.json`

The server adds these keys the first time it starts (an old `server.json` gets them on its own):

```json
"mods": {
    "enabled": true,
    "scriptsDir": "mods",
    "scripts": ["*"],
    "pluginsDir": "plugins",
    "plugins": ["*"],
    "dataDir": "mods/data",
    "unsafeLua": false,
    "scriptTimeoutMs": 2000,
    "scriptMemoryMb": 64,
    "settings": {}
}
```

| Key | Default | What it is |
|---|---|---|
| `enabled` | `true` | `false`: no mod is loaded |
| `scriptsDir` / `pluginsDir` | `"mods"` / `"plugins"` | script and plugin folders |
| `scripts` / `plugins` | `["*"]` | what to load, in order: `"*"` = everything in the folder (by name), `"hello"` or `"hello.lua"` = that file, a path, `"!hello"` = not that one |
| `dataDir` | `"mods/data"` | where each mod saves its data (`<mod>.json`) |
| `unsafeLua` | `false` | `true`: full Lua (`io`, `os`, `package`): a script could read or delete files on the PC |
| `scriptTimeoutMs` | `2000` | how long a handler may take before it is cut off (100 to 60000 ms) |
| `scriptMemoryMb` | `64` | memory per script (8 to 1024 MB) |
| `settings` | `{}` | each mod's settings: `{"ejemplo": {"consejosCadaMinutos": 5}}` (read by `coop.mod.setting`) |

List examples: `"scripts": ["*", "!tests"]` (all but `tests.lua`), `"scripts": ["base", "events"]`
(only those two, in that order).

### Command line

| Parameter | What it does |
|---|---|
| `--mods-dir <folder>` | script folder (instead of `scriptsDir`) |
| `--plugins-dir <folder>` | plugin folder |
| `--script <file>` | adds a script to the list (can be repeated) |
| `--plugin <file>` | adds a plugin to the list (can be repeated) |
| `--no-mods` | starts without any mod |
| `--mod-docs <folder>` | writes `API.md` and `IDS.md` into that folder and exits (does not start the server) |

`2ship-coop-server.exe --script tests.lua` starts with the usual ones plus `tests.lua`.

## 2. Your first script, step by step

1. Create `mods/hello.lua` with a text editor (Notepad is fine; save it as UTF-8):

   ```lua
   -- When someone enters the server, we tell everyone.
   coop.on("player_join", function(e)
       coop.chat.broadcast(e.nick .. " has arrived!")
   end)

   -- A new command: /hello. What it returns is the reply (and its color: "ok" is green).
   coop.commands.register("hello", { help = "greets you" }, function(ctx)
       return "Hello, " .. ctx.nick .. "!", "ok"
   end)
   ```

2. With the server stopped, start it; with the server running, type `/mod load hello.lua` in its console.
   The console prints `Mod loaded: hello (lua)` (or `Mod cargado: hello (lua)` if the server language is Spanish).
3. Enter with the game and type `/hello` in the chat. `/help` already shows it with its help.
4. Change the text, save and type `/mod reload hello`: the change applies immediately.

If something fails, the log (`logs/server.log` and the console) says what and on which line: `[hello] error in /hello:
hello.lua:8: ...`.

## 3. How a script works

Each script has its own Lua: its variables do not mix with another's, and an error in one does not stop the others.
Everything runs on the server's thread, between one turn of its loop and the next: no locks are needed.

### Events

```lua
local id = coop.on("chat", function(e)
    if e.text:find("cheat") then
        return false            -- in a cancelable event: it does not happen (here, the message is not sent)
    end
    e.text = e.text:upper()     -- a writable field: the server uses the new value
end)
coop.off(id)                    -- stop listening
```

[API.md](API.md) says which fields each event carries, which can be changed and which events can be cancelled
(`player_connect`, `chat`, `command`, `vote_start`). A misspelled name is an error on load (so it does not go
unnoticed). Handlers are called in the order they subscribed; if one cancels, the following ones do not receive it.

**Events between mods**: a name with `:` (`"economy:payment"`). One mod fires it with
`local data, cancelled = coop.emit("economy:payment", { nick = "Ana", rupees = 50 })` and others listen with
`coop.on("economy:payment", ...)` (they can change any field and cancel it).

### Functions

`coop.<namespace>.<function>(...)`: `coop.chat.tell(1, "hello")`, `coop.players.list()`, `coop.world.time()`...
If an argument is wrong, the function raises an error that says which one; to carry on even if it fails:

```lua
local ok, err = pcall(coop.players.kick, "Ana", "spam")
if not ok then print("could not: " .. err) end
```

Arguments that repeat:

- **player**: a connected player, by id (`e.player`) or nick (`"Ana"`).
- **target**: a player, a list (`{ "Ana", 2 }`) or `"*"` (everyone).
- **item**, **actor**, **scene**: the name from [IDS.md](IDS.md) (`"MASK_BUNNY"`, `"EN_DODONGO"`,
  `"SOUTH_CLOCK_TOWN"`) or the number.

A player's id is valid while they stay connected: a timer that keeps `e.player` must expect them to have left (the
function will raise an error, it will never do anything odd).

### Timers

```lua
coop.timer.after(5000, function() coop.chat.broadcast("5 seconds have passed") end)
local t = coop.timer.every(60 * 1000, function() print("another minute") end)
coop.timer.cancel(t)
```

At least 10 ms. They are removed on their own when the mod is unloaded or reloaded. For repeating logic this is what
there is: there is no per-frame event.

### Commands

```lua
coop.commands.register("prize", {
    usage = "/prize <player> <rupees>",
    help = "gives rupees to a player",
    perm = "op",          -- "player" (everyone, the default), "op" (admins) or "console" (console only)
    minArgs = 2,          -- with fewer arguments the server replies with the usage
    aliases = { "award" },
}, function(ctx, args)
    -- ctx.player (missing if it is the console), ctx.nick, ctx.isConsole, ctx.isOp; args = { "Ana", "50" }
    local n = coop.game.giveRupees(args[1], tonumber(args[2]) or 0)
    return "Sent to " .. n .. " game(s).", "ok"
end)
```

The name: 1 to 24 lowercase letters, digits or `_`, and not already taken. `/help` shows it. What the function
returns is the reply (a text and, optionally, its level: `"ok"`, `"warn"`, `"error"`).

### Saved data

```lua
local visits = coop.storage.get("visits", 0) + 1
coop.storage.set("visits", visits)            -- number, text, boolean or table
```

Each mod has its own (`mods/data/<mod>.json`), which is still there when the server restarts or the mod reloads. It
is written at most every 2 seconds and on stop.

### Settings

The server owner writes them in `server.json` (`mods.settings.<mod>`) and the script reads them with a default
value:

```lua
local PRIZE = coop.mod.setting("prize", 20)
```

### What `/mods` shows

```lua
coop.mod.describe({ title = "My mod", version = "1.0", author = "me", description = "what it does" })
```

### Splitting a script into several files

`require("lib.util")` loads `mods/lib/util.lua` (once; it returns whatever that file returns). Only files inside the
script folder.

### The Lua of scripts

Lua 5.4 without what touches the PC: `string`, `table`, `math`, `utf8`, `coroutine`, `os.time`, `os.date`,
`os.clock`, `os.difftime`, `print` (writes to the server log) and `require` (the one above) are available. `io`,
`os.execute`, `load`, `loadfile` and `dofile` are not (except with `"unsafeLua": true`).

Between Lua and the API, values are converted like this: `nil` is "nothing", integers stay integers, a table with
keys `1..n` is a list, an empty table counts as a list or as an object. `NaN`, infinities, tables that contain
themselves and more than 16 levels are not accepted; a text that is not UTF-8 arrives with `?`.

## 4. Commands to the players' games

The `coop.game.*` functions send commands to each player's game: `notify` (pop-up notice), `message` (game text
box), `sound`, `giveItem`, `takeItem`, `giveRupees`, `heal`, `damage`, `kill`, `magic`, `spawn` (create an
actor), `warp` (take to another scene) and `unlockAll`. Rules:

- They only reach whoever is **playing in the server's game** (they never touch their save files) and return how
  many games they reached (`0`: nobody was playing).
- Each player can turn them off in the game: F1 → Co-op → "Permitir los mods del servidor" (allow the server's mods,
  `gCoop.Mods`).
- If the game cannot carry it out at that moment (changing scene, in a dialogue or a cutscene), the command waits up
  to 10 seconds; a `message` that did not find its moment arrives as a chat line.
- The game checks every command again (ranges, items that exist, valid entrances) and discards what does not fit.
- The font of the game's text boxes has no accents: `message` removes them (`á` → `a`). `notify` and the chat do
  show them.
- The actors `spawn` creates belong to each game: every player sees and fights their own.
- **What belongs to the shared world belongs to everyone**: the items on the items screen (except bottles), masks,
  songs, upgrades, the sword and shield and heart containers. Giving or taking one from a player changes it for
  everyone playing in the game. Rupees, health, magic, ammo and what is in the bottles are per player.

What happens in the game arrives as events: `player_item`, `player_death`, `enemy_killed`, `boss_defeated` and
`player_stats` (health, magic and rupees, at most 4 times per second). `coop.players.get(p)` also gives the last
known health, magic and rupees.

## 5. Game options that can be forced

2 Ship's options (those in the F1 menu: cheats, difficulty, modes) can be imposed **while playing in the server's
game**. On leaving it, disconnecting or shutting the game down, each player gets their own back.

- For everyone, always: in `server.json`, `"gameSettings": { "gCheats.InfiniteMagic": 1 }`.
- From a script: `coop.game.setSetting("*", "gEnhancements.DifficultyOptions.DamageMultiplier", 2)` (for everyone,
  also whoever enters later) or with a player or a list (only them, until they disconnect).
  `coop.game.clearSetting(target, name)` removes it; `coop.game.settings()` says which ones are set.

The ones starting with `gEnhancements.`, `gCheats.`, `gModes.` and `gFixes.` can be forced, except the per-player ones
(`gEnhancements.A11y.`, `.Camera.`, `.Graphics.`, `.Saving.`, `.Mods.`, `.Playback.`, `.Dpad.`), "delete the file on
death" and the ones co-op already sets in the server's game (skip cutscenes, "time moves if you move"...).
At most 64 per player. The value is a number: `1`/`0` for checkboxes, the position for lists, decimals for decimal
sliders.

Some useful ones (the exact name of any other is in the game's settings file,
`2ship2harkinian.json` → `CVars`):

| Option | Values | What it does |
|---|---|---|
| `gCheats.InfiniteHealth` | 1 | infinite health |
| `gCheats.InfiniteMagic` | 1 | infinite magic |
| `gCheats.InfiniteRupees` | 1 | infinite rupees |
| `gCheats.InfiniteConsumables` | 1 | infinite ammo and items |
| `gCheats.MoonJumpOnL` | 1 | moon jump with L |
| `gCheats.UnbreakableRazorSword` | 1 | the Razor Sword does not wear out |
| `gEnhancements.DifficultyOptions.DamageMultiplier` | 0 (1x), 1 (2x), 2 (4x), 3 (8x), 4 (16x), 10 (one hit) | damage Link takes |
| `gEnhancements.DifficultyOptions.BossHealthMultiplier` | 0 (1x) to 4 (2x) | boss health (when the scene reloads) |
| `gEnhancements.DifficultyOptions.HyperEnemies` | 1 | enemies move twice as fast |
| `gEnhancements.DifficultyOptions.NoHeartDrops` | 1 | no hearts drop |
| `gEnhancements.DifficultyOptions.PermanentHeartLoss` | 1 | losing 4 heart quarters removes the container |
| `gEnhancements.DifficultyOptions.DisableTakkuriSteal` | 1 | the Takkuri does not steal |
| `gEnhancements.Player.ClimbSpeed` | 1 to 5 | climbing speed |
| `gEnhancements.Masks.FastTransformation` | 1 | transformations without animation |
| `gEnhancements.Masks.FierceDeitysAnywhere` | 1 | Fierce Deity outside bosses |
| `gEnhancements.Timesavers.FastChests` | 1 | fast chests |
| `gModes.MirroredWorld.Mode` | 0 (no), 1 (always), 2 (random)... | mirrored world |
| `gModes.PlayAsKafei` | 1 | play as Kafei (when the scene reloads) |

## 6. The shared world: fields and flags

The server's game keeps the common progress in **fields** (`coop.world.fields()` lists them):

| Field | Type | What it stores |
|---|---|---|
| `weekEventReg` | bits | quests and events of the cycle |
| `sceneFlags` | bits | per scene: chests, switches, cleared rooms, collected items |
| `sceneRooms`, `sceneExtra` | bits | visited rooms; fairy fountains, dungeon floors |
| `owls` | bits | activated owl statues |
| `mapsVisible`, `regions`, `clouds` | bits | Tingle maps, visited regions, map clouds |
| `upgrades`, `quest` | bits | upgrades (quiver, bag, wallet...) and quest items (songs, remains) |
| `dungeonItems` | bits | map, compass and boss key of each dungeon |
| `items`, `masks` | bytes | items and masks (the item id in each slot) |
| `equipment`, `magicFlags`, `defense`, `progress`, `resets`, `codes` | bytes | sword and shield, magic, double defense, progress, codes (lottery, Bombers...) |
| `heartQuarters`, `bottles`, `keys`, `fairies`, `skulls` | counters | maximum heart quarters, bottles, keys, stray fairies, Skulltulas |

```lua
if coop.world.getBit("owls", 0, 0x01) then print("the Clock Town owl is activated") end
coop.world.setBits("owls", 0, 0x01)        -- flags: set (and, optionally, clear) bits of a byte
coop.world.set("masks", 0, 0x32)           -- bytes: write a value
coop.world.add("heartQuarters", 0, 4)      -- counters: add (one heart is 4 quarters)
```

What a mod changes reaches everyone playing in the game right away (and the `world_change` event reports it with
`player = 0`). The clock: `coop.world.time()`, `setTime(day, hour, minute)`, `setStopped(true)`, `setSpeed(0.5)`,
`restart()` (new cycle without a vote) and `crashMoon()`.

## 7. Other server parameters

Besides the usual ones (`port`, `maxPlayers`, `password`, `motd`, `language`, `sharedEnemies`...), `server.json`
has:

| Key | Default | What it is |
|---|---|---|
| `timeSpeed` | `1.0` | speed of the three days (0.1 to 10): `0.5` = days twice as long |
| `voteSeconds` | `30` | how long the Song of Time vote lasts (10 to 300) |
| `saveSeconds` | `10` | how often the world is saved to disk (2 to 600) |
| `giftMax` | `999` | maximum rupees of a `/gift` (1 to 999) |
| `commandPermissions` | `{}` | who can use each command: `{"tp": "op", "gift": "console", "dado": "player"}` (`player`, `op` or `console`; mods' commands too) |
| `gameSettings` | `{}` | 2 Ship options forced on everyone (section 5) |

A script reads them with `coop.server.config("timeSpeed")` and changes some while running with
`coop.server.setConfig` (without saving them to the file).

## 8. Debugging

- `print(...)` writes to the server console and to `logs/server.log`, with the mod's name in front.
  `coop.server.log(text, "warn")` does the same with a level.
- An error inside a handler is logged with the file, the line and the trace: `[hello] error in chat:
  hello.lua:12: attempt to index a nil value`. The handler stays subscribed. After 20 errors from the same handler
  it stops being logged (not stops running).
- An error on load (syntax, a nonexistent event) leaves the mod unloaded and nothing half-done; the other mods
  start anyway.
- `/mod reload name` reloads without stopping the server; `/mods` says what is loaded.
- `coop.server.exec("list")` runs a command and returns its reply: useful for testing from a script.

## 9. Security and limits

- A script cannot read the server password or the hosts' token, nor touch files (except with
  `"unsafeLua": true`, only for trusted scripts).
- A handler that takes longer than `scriptTimeoutMs` (2 s) is cut off with an error; a script that exceeds
  `scriptMemoryMb` (64 MB) gets a memory error. The server carries on.
- A plugin is native code: a fault inside it takes the server down. Only install DLLs from people you trust.
- Each player's game checks everything it receives (ranges, ids, entrances, allowed options) and runs nothing
  outside the server's game.
- The notices the game sends (`gev`, `stat`) are validated by the server and rate-limited.
- Chained events (a handler that fires another event, and so on): at most 8 deep.

## 10. Recipes

**Give a prize for killing enemies**:

```lua
coop.on("enemy_killed", function(e)
    if e.actorKey == "EN_DODONGO" then
        coop.game.giveRupees(e.player, 20)
        coop.game.notify(e.player, "+20 rupees for the Dodongo")
    end
end)
```

**A welcome message in the game, with the game's text box**:

```lua
coop.on("world_enter", function(e)
    coop.game.message(e.player, "%gWelcome%w to Termina.\nThe clock does not stop: hurry up.")
end)
```

**Harder at night**:

```lua
coop.on("world_hour", function(e)
    coop.game.setSetting("*", "gEnhancements.DifficultyOptions.DamageMultiplier", e.night and 1 or 0)
end)
```

**Longer days with few players**:

```lua
local function Adjust()
    coop.world.setSpeed(#coop.players.inWorld() <= 1 and 0.5 or 1)
end
coop.on("world_enter", Adjust)
coop.on("world_leave", Adjust)
```

**A teleport by place name**:

```lua
coop.commands.register("go", { usage = "/go <scene>", minArgs = 1 }, function(ctx, args)
    if not ctx.player then return "Players only." end
    local ok, err = pcall(coop.game.warp, ctx.player, args[1]:upper())
    return ok and "Travelling..." or err, ok and "ok" or "warn"
end)
```

**Block a command at certain hours**:

```lua
coop.on("command", function(e)
    local t = coop.world.time()
    if e.name == "tp" and t and t.night then
        coop.chat.tell(e.player, "No /tp at night.", "warn")
        return false
    end
end)
```

## 11. Known limits

- Actors created with `game.spawn` belong to each game (they are not replicated and do not share health), an enemy
  does not appear in a room that is already cleared, and some actors only work in their own scene (bosses,
  mechanisms).
- There are no in-game scripts nor new UI, models or textures: those are 2 Ship's mods (`mods/*.o2r`).
- There is no per-frame or per-pose event: for periodic logic, use `coop.timer.every`.
- `world_change` cannot be cancelled (the game that sent it already applied it).
- `enemy_killed` comes from enemies that end with the game's "final blow" (almost all), and not from those simulated
  by a windowless server host.
- Sounds have no names (there are thousands): their numbers are used (`NA_SE_*` in `mm/include/sfx.h` of the game's
  code).
- Forced options are applied when something changes; some 2 Ship enhancements only read their option when the scene
  loads (their help in the F1 menu says so).
