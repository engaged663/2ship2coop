// The events a mod can listen to: X(Id, "name", cancelable, "what happened", "field:type[*] what it is|...").
// Types: int, number, string, bool, list, table. A handler may change the fields marked with * (and only those).
// The texts are the English reference: coop/docs/mods/API.md is generated from this table (ModDocs.cpp), so no
// description may contain the character |.
X(ServerStart, "server_start", false, "The server has started and the configured mods are loaded.", "")
X(ServerStop, "server_stop", false, "The server is about to stop.", "reason:string Why it is stopping")
X(PlayerConnect, "player_connect", true,
  "A player has passed the server's checks (version, nick, bans, password) and is about to enter. "
  "Cancelling it rejects them with `reason`.",
  "nick:string Nick they enter with|ip:string Their IP address|reason:string* Reason they will see if it is cancelled")
X(PlayerJoin, "player_join", false, "A player has entered the server.",
  "player:int Player id|nick:string Their nick|ip:string Their IP address")
X(PlayerLeave, "player_leave", false, "A player has disconnected. Their id is still valid inside the handler.",
  "player:int Player id|nick:string Their nick|reason:string Reason (disconnected, kicked...)")
X(PlayerScene, "player_scene", false, "A player has changed scene or room.",
  "player:int Player id|nick:string Their nick|scene:int Scene id|sceneKey:string Scene key "
  "(`SOUTH_CLOCK_TOWN`...; empty if unknown)|sceneName:string Name the game shows|room:int Room|"
  "entrance:int Entrance they arrived through|prevScene:int Previous scene (-1: none)|prevRoom:int Previous room")
X(PlayerStats, "player_stats", false,
  "A player's health, magic or rupees have changed (at most 4 notices per second and player).",
  "player:int Player id|nick:string Their nick|health:int Health (16 = one heart)|maxHealth:int Maximum health|"
  "magic:int Magic (48 = normal bar, 96 = double)|rupees:int Rupees|prevHealth:int Previous health|"
  "prevMagic:int Previous magic|prevRupees:int Previous rupees")
X(PlayerDeath, "player_death", false, "A player has run out of health (even if a fairy revives them afterwards).",
  "player:int Player id|nick:string Their nick|scene:int Scene they were in")
X(PlayerItem, "player_item", false,
  "The game has given an item to a player (also ammo, hearts and picked-up rupees).",
  "player:int Player id|nick:string Their nick|item:int Item id|itemKey:string Its name "
  "(`MASK_BUNNY`...; empty if unknown)")
X(EnemyKilled, "enemy_killed", false,
  "An enemy has taken the final blow. `player` is who dealt it (or the game that simulated it if unknown).",
  "player:int Player id|nick:string Their nick|actor:int Actor id|actorKey:string Its name "
  "(`EN_DODONGO`...)|params:int Actor parameters|scene:int Scene|room:int Room|x:number Position|"
  "y:number Position|z:number Position")
X(BossDefeated, "boss_defeated", false, "A boss has been defeated in that player's game.",
  "player:int Player id|nick:string Their nick|actor:int Boss actor id|actorKey:string Its name|"
  "scene:int Scene")
X(Chat, "chat", true, "A player writes in the chat. Cancelling it stops the message from being sent.",
  "player:int Player id|nick:string Their nick|text:string* The message")
X(Command, "command", true,
  "Someone is about to run a command (already with permission and enough arguments). Cancelling it stops it from running.",
  "player:int Player id (0: the console)|nick:string Their nick|name:string Command, without the slash|"
  "args:list Arguments|line:string The full line")
X(WorldEnter, "world_enter", false, "A player has entered the server's game.",
  "player:int Player id|nick:string Their nick")
X(WorldLeave, "world_leave", false, "A player has left the server's game.",
  "player:int Player id|nick:string Their nick")
X(WorldCreated, "world_created", false, "The server's world has been created (by that player's game).",
  "player:int Player id|nick:string Their nick")
X(WorldChange, "world_change", false,
  "The shared world has changed (already applied). The lists are those of `wops`: bits `[field, byte, set, clear]`, "
  "bytes `[field, byte, value]`, adds `[field, byte, amount]`.",
  "player:int Id of who changed it (0: the server or a mod)|bits:list Bit changes|bytes:list Byte changes|"
  "adds:list Counter changes")
X(WorldHour, "world_hour", false, "The world clock has moved to another game hour.",
  "day:int Day (1-3)|hour:int Hour (0-23)|night:bool It is night (18:00-5:59)|abs:int Absolute clock (0 = day 1, "
  "6:00)|jump:bool The clock jumped (song, command, new cycle) instead of advancing")
X(VoteStart, "vote_start", true,
  "A player proposes going back to Dawn of the First Day (Song of Time). Cancelling it stops the vote.",
  "player:int Player id|nick:string Their nick")
X(VoteEnd, "vote_end", false, "The Song of Time vote has ended.", "passed:bool It passed")
X(CycleReset, "cycle_reset", false, "A new three-day cycle has started.",
  "cycle:int Number of the new cycle|reason:string `sot` (vote), `moon` (the moon fell), `restart` (command), "
  "`ending` (end of the game), `import` (a converted base-game save, /importar) or `restore` (a backup, /restaurar)")
X(MoonCrash, "moon_crash", false, "The clock has reached the end of the third day: the moon falls.",
  "cycle:int Cycle that ends")
X(GroupChange, "group_change", false, "A group has changed members or has been disbanded (no members).",
  "group:int Group id|leader:int Leader id (0: disbanded)|members:list Ids of its members")
X(Activity, "activity", false, "A player starts or ends a minigame, or reports its result.",
  "player:int Player id|nick:string Their nick|state:string `start`, `end` or `result`|key:string Minigame "
  "key|name:string Its name|score:int Points (at the end)|cs:int Time in hundredths of a second|won:bool Won "
  "(only in `result`)")
X(ModUnload, "mod_unload", false, "This mod is about to be unloaded (only it receives this).", "")
