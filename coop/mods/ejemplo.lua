-- ejemplo.lua: example script for the 2 Ship 2 Harkinian co-op server ("ejemplo" means "example").
--
-- Copy it to the server's "mods" folder (next to 2ship-coop-server.exe) and start the server: it loads on its own.
-- After editing it, "/mod reload ejemplo" (in the server console or as an admin) loads it again without stopping
-- anything. What it prints with print() goes to the server log (logs/server.log).
--
-- What it does:
--   * greets whoever enters the server and shows a notice when they enter the shared game;
--   * masks bad words in the chat;
--   * commands: /curar [player] (heal), /dado [sides] (die), /kit (once per cycle), /muertes (deaths) and /horda
--     (horde, admins only);
--   * warns when night falls and in the last hours before the moon falls;
--   * a tip every few minutes.
--
-- Every function (coop.*) and event is in docs/API.md; the names of items and actors, in docs/IDS.md.
-- Its settings go in server.json, for example:
--   "mods": { "settings": { "ejemplo": { "saludo": "Welcome, {nick}!", "consejosCadaMinutos": 5 } } }

-- What /mods shows for this script.
coop.mod.describe({
    title = "Example",
    version = "1.0",
    author = "2ship2coop",
    description = "greetings, chat filter, /curar, /dado, /kit, /muertes, /horda and time notices",
})

-- Settings: the second value is what applies when the server owner has not set anything.
local GREETING = coop.mod.setting("saludo", "Hello, {nick}! This server has mods: /help shows their commands.")
local TIP_EVERY_MINUTES = coop.mod.setting("consejosCadaMinutos", 10) -- 0: no tips
local BAD_WORDS = coop.mod.setting("palabrasProhibidas", { "silly", "idiot" })

-- Replies to whoever typed a command: the text and its level ("ok" in green, "warn" in yellow).
local function Warning(text)
    return text, "warn"
end

----------------------------------------------------------------------------------------------------------------------
-- Events: coop.on("name", function). The function receives a table with the event's fields (docs/API.md).
----------------------------------------------------------------------------------------------------------------------

-- Someone enters the server: chat.tell sends a line only to them.
coop.on("player_join", function(e)
    coop.chat.tell(e.player, (GREETING:gsub("{nick}", e.nick)), "ok")
end)

-- Someone enters the shared game: a pop-up notice on their screen. Commands to the game (coop.game.*) only reach
-- whoever is playing in the server's game.
coop.on("world_enter", function(e)
    coop.game.notify(e.player, "Welcome to the shared game, " .. e.nick .. ".", 6)
end)

-- Chat filter: the "chat" event lets you change the text (e.text) before the others read it.
local function CaseInsensitive(word)
    -- "silly" -> "[sS][iI][lL][lL][yY]": the pattern finds it written any way
    return (word:gsub("%a", function(letter)
        return "[" .. letter:lower() .. letter:upper() .. "]"
    end))
end

coop.on("chat", function(e)
    local text = e.text
    for _, word in ipairs(BAD_WORDS) do
        text = text:gsub(CaseInsensitive(word), string.rep("*", #word))
    end
    if text ~= e.text then
        e.text = text
    end
    -- "return false" here would cancel the whole message.
end)

-- Every death, noted with coop.storage: the mod's data is still there even if the server restarts.
-- (player_death arrives even if a bottled fairy revives them afterwards.)
coop.on("player_death", function(e)
    local deaths = coop.storage.get("muertes", {})
    deaths[e.nick] = (deaths[e.nick] or 0) + 1
    coop.storage.set("muertes", deaths)
end)

-- The world's hour: a notice when night falls and in the last hours of the third day.
coop.on("world_hour", function(e)
    if e.jump then
        return -- the clock jumped (the Song of Time, a command): the hour did not simply go by
    end
    if e.day == 3 and e.hour < 6 then
        coop.chat.broadcast((6 - e.hour) .. " hours left until the moon falls!", "warn")
    elseif e.hour == 18 then
        coop.chat.broadcast("Night falls on day " .. e.day .. ".")
    end
end)

----------------------------------------------------------------------------------------------------------------------
-- Commands: coop.commands.register(name, options, function). The function receives who typed it (ctx) and its
-- arguments (args, strings); what it returns is the reply.
----------------------------------------------------------------------------------------------------------------------

-- /curar [player]: heals you fully. Healing someone else is for admins.
coop.commands.register("curar", { usage = "/curar [player]", help = "heals you fully (someone else: admins only)" },
    function(ctx, args)
        local who = ctx.player -- nil if typed in the server console
        if args[1] then
            if not ctx.isOp then
                return Warning("Only an admin can heal another player.")
            end
            who = coop.players.find(args[1])
            if not who then
                return Warning("Nobody is connected with the nick " .. args[1] .. ".")
            end
        end
        if not who then
            return Warning("From the console, say who: /curar <player>")
        end
        -- Commands to the game return how many games they reached.
        if coop.game.heal(who) == 0 then
            return Warning("You have to be playing in the server's game.")
        end
        return "Healed.", "ok"
    end)

-- /dado [sides]: rolls a die and tells everyone.
coop.commands.register("dado", { usage = "/dado [sides]", help = "rolls a die (6 sides unless you say otherwise)" },
    function(ctx, args)
        local sides = tonumber(args[1] or "6")
        sides = sides and math.tointeger(sides)
        if not sides or sides < 2 or sides > 1000 then
            return Warning("Give a number of sides between 2 and 1000.")
        end
        coop.chat.broadcast(ctx.nick .. " rolls a " .. sides .. "-sided die: it lands on " .. math.random(sides) .. ".")
    end)

-- /kit: 30 arrows and 10 bombs, once per three-day cycle (who collected it in which cycle, in coop.storage).
coop.commands.register("kit", { usage = "/kit", help = "arrows and bombs, once per cycle" }, function(ctx)
    if not ctx.player then
        return Warning("Players only.")
    end
    local cycle = coop.world.cycle() -- 0 while there is no game
    local collected = coop.storage.get("kit", {})
    if collected[ctx.nick] == cycle then
        return Warning("You already collected your kit this cycle: come back after the Song of Time.")
    end
    -- Items go by their name from docs/IDS.md (or by their number).
    if coop.game.giveItem(ctx.player, "ARROWS_30") == 0 then
        return Warning("You have to be playing in the server's game.")
    end
    coop.game.giveItem(ctx.player, "BOMBS_10")
    collected[ctx.nick] = cycle
    coop.storage.set("kit", collected)
    return "Kit delivered: 30 arrows and 10 bombs (they fit if you have a quiver and a bomb bag).", "ok"
end)

-- /muertes: who has died the most times.
coop.commands.register("muertes", { usage = "/muertes", help = "who has died the most times" }, function()
    local list = {}
    for nick, times in pairs(coop.storage.get("muertes", {})) do
        list[#list + 1] = { nick = nick, times = times }
    end
    if #list == 0 then
        return "Nobody has died yet."
    end
    table.sort(list, function(a, b)
        return a.times > b.times
    end)
    local parts = {}
    for _, row in ipairs(list) do
        parts[#parts + 1] = row.nick .. ": " .. row.times
    end
    return "Deaths: " .. table.concat(parts, ", ")
end)

-- /horda [player]: admins only (perm = "op"). Three Keese in front of each player in the game, or of one. Each game
-- creates its own: they are not shared between players.
coop.commands.register("horda", { usage = "/horda [player]", help = "three Keese in front of each player", perm = "op" },
    function(ctx, args)
        local target = "*" -- everyone playing in the server's game
        if args[1] then
            target = coop.players.find(args[1])
            if not target then
                return Warning("Nobody is connected with the nick " .. args[1] .. ".")
            end
        end
        local games = 0
        for i = 1, 3 do
            -- EN_FIREFLY is the Keese; params = 2, a normal one that flies. distance: units in front of Link.
            games = coop.game.spawn(target, "EN_FIREFLY", { params = 2, distance = 120 + 60 * i })
        end
        if games == 0 then
            return Warning("Nobody is playing in the server's game.")
        end
        coop.chat.broadcast("A horde of Keese, courtesy of " .. ctx.nick .. "!", "warn")
    end)

----------------------------------------------------------------------------------------------------------------------
-- Timers: coop.timer.after(ms, function) once, coop.timer.every(ms, function) forever.
----------------------------------------------------------------------------------------------------------------------

local TIPS = {
    "Type /tiempo to see the game's clock.",
    "With /kit you get arrows and bombs once per cycle.",
    "If you fall behind, /tp <player> takes you to their side.",
    "The Song of Time is voted on: more than half of those playing are needed.",
}

if TIP_EVERY_MINUTES > 0 then
    local nextTip = 1
    coop.timer.every(TIP_EVERY_MINUTES * 60 * 1000, function()
        if coop.players.count() > 0 then
            coop.chat.broadcast("Tip: " .. TIPS[nextTip])
            nextTip = nextTip % #TIPS + 1
        end
    end)
end

print("ready: " .. coop.mod.name() .. " (server in " .. coop.server.info().language .. ")")
