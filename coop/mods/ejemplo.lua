-- ejemplo.lua: script de ejemplo para el servidor co-op de 2 Ship 2 Harkinian.
--
-- Cópialo a la carpeta "mods" del servidor (al lado de 2ship-coop-server.exe) y arranca el servidor: se carga solo.
-- Después de editarlo, "/mod reload ejemplo" (en la consola del servidor o como admin) lo vuelve a cargar sin parar
-- nada. Lo que imprime con print() va al registro del servidor (logs/server.log).
--
-- Qué hace:
--   * saluda a quien entra en el servidor y le muestra un aviso al entrar en la partida compartida;
--   * tapa las palabras feas del chat;
--   * comandos: /curar [jugador], /dado [caras], /kit (una vez por ciclo), /muertes y /horda (solo admins);
--   * avisa al caer la noche y en las últimas horas antes de que caiga la luna;
--   * cada pocos minutos, un consejo.
--
-- Todas las funciones (coop.*) y los eventos están en docs/API.md; los nombres de objetos y actores, en docs/IDS.md.
-- Sus ajustes van en server.json, por ejemplo:
--   "mods": { "settings": { "ejemplo": { "saludo": "¡Bienvenido, {nick}!", "consejosCadaMinutos": 5 } } }

-- Lo que enseña /mods de este script.
coop.mod.describe({
    title = "Ejemplo",
    version = "1.0",
    author = "2ship2coop",
    description = "saludos, filtro del chat, /curar, /dado, /kit, /muertes, /horda y avisos de la hora",
})

-- Ajustes: el segundo valor es el que vale cuando el dueño del servidor no ha puesto nada.
local SALUDO = coop.mod.setting("saludo", "¡Hola, {nick}! Este servidor tiene mods: /help muestra sus comandos.")
local CONSEJOS_CADA_MINUTOS = coop.mod.setting("consejosCadaMinutos", 10) -- 0: ningún consejo
local PALABRAS_PROHIBIDAS = coop.mod.setting("palabrasProhibidas", { "tonto", "idiota" })

-- Responde a quien escribió un comando: el texto y su nivel ("ok" en verde, "warn" en amarillo).
local function Aviso(texto)
    return texto, "warn"
end

----------------------------------------------------------------------------------------------------------------------
-- Eventos: coop.on("nombre", función). La función recibe una tabla con los campos del evento (docs/API.md).
----------------------------------------------------------------------------------------------------------------------

-- Alguien entra en el servidor: chat.tell le manda una línea solo a él.
coop.on("player_join", function(e)
    coop.chat.tell(e.player, (SALUDO:gsub("{nick}", e.nick)), "ok")
end)

-- Alguien entra en la partida compartida: un aviso emergente en su pantalla. Las órdenes al juego (coop.game.*) solo
-- llegan a quien juega en la partida del servidor.
coop.on("world_enter", function(e)
    coop.game.notify(e.player, "Bienvenido a la partida compartida, " .. e.nick .. ".", 6)
end)

-- Filtro del chat: el evento "chat" deja cambiar el texto (e.text) antes de que lo lean los demás.
local function SinMayusculas(palabra)
    -- "tonto" -> "[tT][oO][nN][tT][oO]": el patrón la encuentra escrita de cualquier forma
    return (palabra:gsub("%a", function(letra)
        return "[" .. letra:lower() .. letra:upper() .. "]"
    end))
end

coop.on("chat", function(e)
    local texto = e.text
    for _, palabra in ipairs(PALABRAS_PROHIBIDAS) do
        texto = texto:gsub(SinMayusculas(palabra), string.rep("*", #palabra))
    end
    if texto ~= e.text then
        e.text = texto
    end
    -- "return false" aquí cancelaría el mensaje entero.
end)

-- Cada muerte, apuntada con coop.storage: los datos del mod siguen ahí aunque el servidor se reinicie.
-- (player_death llega aunque un hada embotellada lo reviva después.)
coop.on("player_death", function(e)
    local muertes = coop.storage.get("muertes", {})
    muertes[e.nick] = (muertes[e.nick] or 0) + 1
    coop.storage.set("muertes", muertes)
end)

-- La hora del mundo: aviso al caer la noche y en las últimas horas del tercer día.
coop.on("world_hour", function(e)
    if e.jump then
        return -- el reloj saltó (la Canción del Tiempo, un comando): no ha pasado la hora sin más
    end
    if e.day == 3 and e.hour < 6 then
        coop.chat.broadcast("¡Quedan " .. (6 - e.hour) .. " horas para que caiga la luna!", "warn")
    elseif e.hour == 18 then
        coop.chat.broadcast("Cae la noche del día " .. e.day .. ".")
    end
end)

----------------------------------------------------------------------------------------------------------------------
-- Comandos: coop.commands.register(nombre, opciones, función). La función recibe quién lo escribe (ctx) y sus
-- argumentos (args, textos); lo que devuelve es la respuesta.
----------------------------------------------------------------------------------------------------------------------

-- /curar [jugador]: te cura del todo. Curar a otro es cosa de admins.
coop.commands.register("curar", { usage = "/curar [jugador]", help = "te cura del todo (a otro: solo admins)" },
    function(ctx, args)
        local quien = ctx.player -- nil si lo escribe la consola del servidor
        if args[1] then
            if not ctx.isOp then
                return Aviso("Solo un admin puede curar a otro jugador.")
            end
            quien = coop.players.find(args[1])
            if not quien then
                return Aviso("No hay nadie conectado con el nick " .. args[1] .. ".")
            end
        end
        if not quien then
            return Aviso("Desde la consola, di a quién: /curar <jugador>")
        end
        -- Las órdenes al juego devuelven a cuántos juegos han llegado.
        if coop.game.heal(quien) == 0 then
            return Aviso("Hay que estar jugando en la partida del servidor.")
        end
        return "Curado.", "ok"
    end)

-- /dado [caras]: tira un dado y se lo cuenta a todos.
coop.commands.register("dado", { usage = "/dado [caras]", help = "tira un dado (de 6 caras si no dices otra cosa)" },
    function(ctx, args)
        local caras = tonumber(args[1] or "6")
        caras = caras and math.tointeger(caras)
        if not caras or caras < 2 or caras > 1000 then
            return Aviso("Di un número de caras entre 2 y 1000.")
        end
        coop.chat.broadcast(ctx.nick .. " tira un dado de " .. caras .. " caras: sale un " .. math.random(caras) .. ".")
    end)

-- /kit: 30 flechas y 10 bombas, una vez por ciclo de tres días (quién lo recogió en qué ciclo, en coop.storage).
coop.commands.register("kit", { usage = "/kit", help = "flechas y bombas, una vez por ciclo" }, function(ctx)
    if not ctx.player then
        return Aviso("Solo para jugadores.")
    end
    local ciclo = coop.world.cycle() -- 0 mientras no haya partida
    local recogidos = coop.storage.get("kit", {})
    if recogidos[ctx.nick] == ciclo then
        return Aviso("Ya recogiste tu kit en este ciclo: vuelve después de la Canción del Tiempo.")
    end
    -- Los objetos van por su nombre de docs/IDS.md (o por su número).
    if coop.game.giveItem(ctx.player, "ARROWS_30") == 0 then
        return Aviso("Hay que estar jugando en la partida del servidor.")
    end
    coop.game.giveItem(ctx.player, "BOMBS_10")
    recogidos[ctx.nick] = ciclo
    coop.storage.set("kit", recogidos)
    return "Kit entregado: 30 flechas y 10 bombas (caben si tienes carcaj y bolsa de bombas).", "ok"
end)

-- /muertes: quién ha muerto más veces.
coop.commands.register("muertes", { usage = "/muertes", help = "quién ha muerto más veces" }, function()
    local lista = {}
    for nick, veces in pairs(coop.storage.get("muertes", {})) do
        lista[#lista + 1] = { nick = nick, veces = veces }
    end
    if #lista == 0 then
        return "Nadie ha muerto todavía."
    end
    table.sort(lista, function(a, b)
        return a.veces > b.veces
    end)
    local partes = {}
    for _, fila in ipairs(lista) do
        partes[#partes + 1] = fila.nick .. ": " .. fila.veces
    end
    return "Muertes: " .. table.concat(partes, ", ")
end)

-- /horda [jugador]: solo admins (perm = "op"). Tres Keese delante de cada jugador de la partida, o de uno. Cada juego
-- crea los suyos: no se comparten entre jugadores.
coop.commands.register("horda", { usage = "/horda [jugador]", help = "tres Keese delante de cada jugador", perm = "op" },
    function(ctx, args)
        local objetivo = "*" -- todos los que juegan en la partida del servidor
        if args[1] then
            objetivo = coop.players.find(args[1])
            if not objetivo then
                return Aviso("No hay nadie conectado con el nick " .. args[1] .. ".")
            end
        end
        local juegos = 0
        for i = 1, 3 do
            -- EN_FIREFLY es el Keese; params = 2, uno normal que vuela. distance: unidades delante de Link.
            juegos = coop.game.spawn(objetivo, "EN_FIREFLY", { params = 2, distance = 120 + 60 * i })
        end
        if juegos == 0 then
            return Aviso("Nadie está jugando en la partida del servidor.")
        end
        coop.chat.broadcast("¡Una horda de Keese, cortesía de " .. ctx.nick .. "!", "warn")
    end)

----------------------------------------------------------------------------------------------------------------------
-- Temporizadores: coop.timer.after(ms, función) una vez, coop.timer.every(ms, función) siempre.
----------------------------------------------------------------------------------------------------------------------

local CONSEJOS = {
    "Escribe /tiempo para ver el reloj de la partida.",
    "Con /kit tienes flechas y bombas una vez por ciclo.",
    "Si te quedas atrás, /tp <jugador> te lleva a su lado.",
    "La Canción del Tiempo se vota: hace falta más de la mitad de los que juegan.",
}

if CONSEJOS_CADA_MINUTOS > 0 then
    local siguiente = 1
    coop.timer.every(CONSEJOS_CADA_MINUTOS * 60 * 1000, function()
        if coop.players.count() > 0 then
            coop.chat.broadcast("Consejo: " .. CONSEJOS[siguiente])
            siguiente = siguiente % #CONSEJOS + 1
        end
    end)
end

print("listo: " .. coop.mod.name() .. " (servidor en " .. coop.server.info().language .. ")")
