# Mods del servidor co-op

Un **mod** cambia la partida compartida a tu gusto: da objetos, cura o hace daño, crea enemigos, lleva a los
jugadores de un sitio a otro, añade comandos, cambia el reloj o el mundo, fuerza opciones de dificultad de 2 Ship...
Los mods viven **en el servidor**: los jugadores no instalan nada (usan el mismo `2ship.exe` de siempre).

Hay dos clases:

- **Scripts Lua** (`mods/*.lua`): lo normal. Un archivo de texto, sin compilar, que se recarga en caliente.
- **Plugins DLL** (`plugins/*.dll`): lo mismo en C++ (o C), para quien necesite hilos, sockets, otra librería o mucha
  velocidad. Ver [PLUGINS.md](PLUGINS.md).

Los dos usan **la misma API**: las mismas funciones y los mismos eventos, descritos uno a uno en
[API.md](API.md) (generado a partir del código, siempre al día). Los nombres de objetos, actores y escenas están en
[IDS.md](IDS.md). Un ejemplo completo y comentado: `mods/ejemplo.lua`.

## Índice

1. [Instalar y cargar mods](#1-instalar-y-cargar-mods)
2. [Tu primer script, paso a paso](#2-tu-primer-script-paso-a-paso)
3. [Cómo funciona un script](#3-cómo-funciona-un-script)
4. [Órdenes al juego de los jugadores](#4-órdenes-al-juego-de-los-jugadores)
5. [Opciones del juego que se pueden forzar](#5-opciones-del-juego-que-se-pueden-forzar)
6. [El mundo compartido: campos y banderas](#6-el-mundo-compartido-campos-y-banderas)
7. [Otros parámetros del servidor](#7-otros-parámetros-del-servidor)
8. [Depurar](#8-depurar)
9. [Seguridad y límites](#9-seguridad-y-límites)
10. [Recetas](#10-recetas)
11. [Límites conocidos](#11-límites-conocidos)

## 1. Instalar y cargar mods

Junto al servidor:

```
2ship-coop-server.exe
server.json
mods/              los scripts (.lua); subcarpetas para lo que cargues con require ("lib/util.lua")
mods/data/         lo que guarda cada mod (<mod>.json): lo crea el servidor
plugins/           los plugins (.dll en Windows, .so en Linux)
docs/              esta documentación
sdk/               lo necesario para compilar plugins
```

Copia un `.lua` a `mods/` (o una `.dll` a `plugins/`) y arranca el servidor: carga todo lo de esas carpetas, en orden
alfabético. Con el servidor en marcha:

| Comando | Quién | Qué hace |
|---|---|---|
| `/mods` | todos | lista los mods cargados: nombre, tipo, versión, título y descripción |
| `/mod reload [nombre]` | admins y consola | recarga un mod (o todos) desde su archivo; lo que guardó sigue ahí |
| `/mod load <archivo>` | consola | carga un archivo (de `mods/` o `plugins/`, o una ruta) |
| `/mod unload <nombre>` | consola | descarga un mod (quita sus eventos, temporizadores y comandos) |

El **nombre** de un mod es el de su archivo en minúsculas y sin extensión (`mods/Mi Mod.lua` es `mi_mod`).

### `server.json`

El servidor añade estas claves la primera vez que arranca (un `server.json` antiguo las gana solo):

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

| Clave | Por defecto | Qué es |
|---|---|---|
| `enabled` | `true` | `false`: no se carga ningún mod |
| `scriptsDir` / `pluginsDir` | `"mods"` / `"plugins"` | carpetas de scripts y de plugins |
| `scripts` / `plugins` | `["*"]` | qué cargar, en orden: `"*"` = todos los de la carpeta (por nombre), `"hola"` o `"hola.lua"` = ese archivo, una ruta, `"!hola"` = ese no |
| `dataDir` | `"mods/data"` | dónde guarda cada mod sus datos (`<mod>.json`) |
| `unsafeLua` | `false` | `true`: Lua completo (`io`, `os`, `package`): un script podría leer o borrar archivos del PC |
| `scriptTimeoutMs` | `2000` | lo que puede tardar un manejador antes de cortarlo (100 a 60000 ms) |
| `scriptMemoryMb` | `64` | memoria de cada script (8 a 1024 MB) |
| `settings` | `{}` | ajustes de cada mod: `{"ejemplo": {"consejosCadaMinutos": 5}}` (los lee `coop.mod.setting`) |

Ejemplos de listas: `"scripts": ["*", "!pruebas"]` (todos menos `pruebas.lua`), `"scripts": ["base", "eventos"]`
(solo esos dos, en ese orden).

### Línea de comandos

| Parámetro | Qué hace |
|---|---|
| `--mods-dir <carpeta>` | carpeta de scripts (en vez de `scriptsDir`) |
| `--plugins-dir <carpeta>` | carpeta de plugins |
| `--script <archivo>` | añade un script a la lista (se puede repetir) |
| `--plugin <archivo>` | añade un plugin a la lista (se puede repetir) |
| `--no-mods` | arranca sin ningún mod |
| `--mod-docs <carpeta>` | escribe `API.md` e `IDS.md` en esa carpeta y sale (no arranca el servidor) |

`2ship-coop-server.exe --script pruebas.lua` arranca con lo de siempre más `pruebas.lua`.

## 2. Tu primer script, paso a paso

1. Crea `mods/hola.lua` con un editor de texto (Bloc de notas vale; guárdalo en UTF-8):

   ```lua
   -- Cuando alguien entra en el servidor, se lo contamos a todos.
   coop.on("player_join", function(e)
       coop.chat.broadcast("¡" .. e.nick .. " ha llegado!")
   end)

   -- Un comando nuevo: /hola. Lo que devuelve es la respuesta (y su color: "ok" es verde).
   coop.commands.register("hola", { help = "te saluda" }, function(ctx)
       return "¡Hola, " .. ctx.nick .. "!", "ok"
   end)
   ```

2. Con el servidor parado, arráncalo; con el servidor en marcha, escribe en su consola `/mod load hola.lua`.
   En la consola sale `Mod cargado: hola (lua)`.
3. Entra con el juego y escribe `/hola` en el chat. `/help` ya lo muestra con su ayuda.
4. Cambia el texto, guarda y escribe `/mod reload hola`: el cambio vale al momento.

Si algo falla, el registro (`logs/server.log` y la consola) dice qué y en qué línea: `[hola] error en /hola:
hola.lua:8: ...`.

## 3. Cómo funciona un script

Cada script tiene su propio Lua: sus variables no se mezclan con las de otro, y un error en uno no para los demás.
Todo corre en el hilo del servidor, entre una vuelta de su bucle y la siguiente: no hace falta ningún cerrojo.

### Eventos

```lua
local id = coop.on("chat", function(e)
    if e.text:find("trampa") then
        return false            -- en un evento que se puede cancelar: no ocurre (aquí, el mensaje no sale)
    end
    e.text = e.text:upper()     -- un campo que se puede cambiar: el servidor usa el valor nuevo
end)
coop.off(id)                    -- deja de escuchar
```

[API.md](API.md) dice qué campos trae cada evento, cuáles se pueden cambiar y qué eventos se pueden cancelar
(`player_connect`, `chat`, `command`, `vote_start`). Un nombre mal escrito es un error al cargar (así no pasa
desapercibido). Los manejadores se llaman en el orden en que se suscribieron; si uno cancela, los siguientes no lo
reciben.

**Eventos entre mods**: un nombre con `:` (`"economia:pago"`). Uno lo lanza con
`local datos, cancelado = coop.emit("economia:pago", { nick = "Ana", rupias = 50 })` y otros lo escuchan con
`coop.on("economia:pago", ...)` (pueden cambiar cualquier campo y cancelarlo).

### Funciones

`coop.<espacio>.<función>(...)`: `coop.chat.tell(1, "hola")`, `coop.players.list()`, `coop.world.time()`...
Si un argumento está mal, la función lanza un error que dice cuál; para seguir aunque falle:

```lua
local ok, err = pcall(coop.players.kick, "Ana", "molestar")
if not ok then print("no se pudo: " .. err) end
```

Argumentos que se repiten:

- **player**: un jugador conectado, por su id (`e.player`) o su nick (`"Ana"`).
- **target**: un jugador, una lista (`{ "Ana", 2 }`) o `"*"` (todos).
- **item**, **actor**, **scene**: el nombre de [IDS.md](IDS.md) (`"MASK_BUNNY"`, `"EN_DODONGO"`,
  `"SOUTH_CLOCK_TOWN"`) o el número.

El id de un jugador vale mientras siga conectado: un temporizador que guarda `e.player` debe contar con que se haya
ido (la función dará un error, nunca hará algo raro).

### Temporizadores

```lua
coop.timer.after(5000, function() coop.chat.broadcast("han pasado 5 segundos") end)
local t = coop.timer.every(60 * 1000, function() print("otro minuto") end)
coop.timer.cancel(t)
```

Como poco 10 ms. Se borran solos al descargar o recargar el mod. Para lógica que se repite, esto es lo que hay: no
existe un evento por fotograma.

### Comandos

```lua
coop.commands.register("premio", {
    usage = "/premio <jugador> <rupias>",
    help = "da rupias a un jugador",
    perm = "op",          -- "player" (todos, por defecto), "op" (admins) o "console" (solo la consola)
    minArgs = 2,          -- con menos argumentos el servidor responde con el uso
    aliases = { "prize" },
}, function(ctx, args)
    -- ctx.player (no está si es la consola), ctx.nick, ctx.isConsole, ctx.isOp; args = { "Ana", "50" }
    local n = coop.game.giveRupees(args[1], tonumber(args[2]) or 0)
    return "Enviado a " .. n .. " juego(s).", "ok"
end)
```

El nombre: de 1 a 24 letras minúsculas, números o `_`, y que no exista ya. `/help` lo muestra. Lo que devuelve la
función es la respuesta (un texto y, opcional, su nivel: `"ok"`, `"warn"`, `"error"`).

### Datos que se guardan

```lua
local visitas = coop.storage.get("visitas", 0) + 1
coop.storage.set("visitas", visitas)          -- número, texto, booleano o tabla
```

Cada mod tiene los suyos (`mods/data/<mod>.json`), que siguen ahí al reiniciar el servidor o recargar el mod. Se
escriben como mucho cada 2 segundos y al parar.

### Ajustes

El dueño del servidor los escribe en `server.json` (`mods.settings.<mod>`) y el script los lee con un valor por
defecto:

```lua
local PREMIO = coop.mod.setting("premio", 20)
```

### Lo que enseña `/mods`

```lua
coop.mod.describe({ title = "Mi mod", version = "1.0", author = "yo", description = "qué hace" })
```

### Partir un script en varios archivos

`require("lib.util")` carga `mods/lib/util.lua` (una vez; devuelve lo que ese archivo devuelva). Solo archivos de
dentro de la carpeta de scripts.

### El Lua de los scripts

Lua 5.4 sin lo que toca el PC: están `string`, `table`, `math`, `utf8`, `coroutine`, `os.time`, `os.date`,
`os.clock`, `os.difftime`, `print` (escribe en el registro del servidor) y `require` (el de arriba). No están `io`,
`os.execute`, `load`, `loadfile` ni `dofile` (salvo con `"unsafeLua": true`).

Entre Lua y la API los valores se convierten así: `nil` es "nada", los números enteros siguen siendo enteros, una
tabla con claves `1..n` es una lista, una tabla vacía vale como lista o como objeto. No se admiten `NaN`, infinitos,
tablas que se contienen a sí mismas ni más de 16 niveles; un texto que no es UTF-8 llega con `?`.

## 4. Órdenes al juego de los jugadores

Las funciones `coop.game.*` mandan órdenes al juego de cada jugador: `notify` (aviso emergente), `message` (cuadro de
texto del juego), `sound`, `giveItem`, `takeItem`, `giveRupees`, `heal`, `damage`, `kill`, `magic`, `spawn` (crear un
actor), `warp` (llevar a otra escena) y `unlockAll`. Reglas:

- Solo llegan a quien **juega en la partida del servidor** (nunca tocan sus archivos de guardado) y devuelven a
  cuántos juegos llegaron (`0`: nadie estaba jugando).
- Cada jugador puede desactivarlas en el juego: F1 → Co-op → «Permitir los mods del servidor» (`gCoop.Mods`).
- Si el juego no puede cumplirla en ese momento (cambiando de escena, en un diálogo o una cinemática), la orden espera
  hasta 10 segundos; un `message` que no encontró su momento llega como línea de chat.
- El juego vuelve a comprobar cada orden (rangos, objetos que existen, entradas válidas) y descarta lo que no cuadra.
- La letra de los cuadros de texto del juego no tiene tildes: `message` las quita (`á` → `a`). `notify` y el chat sí
  las muestran.
- Los actores que crea `spawn` son de cada juego: cada jugador ve y combate el suyo.
- **Lo que es del mundo compartido es de todos**: los objetos de la pantalla de objetos (salvo las botellas), las
  máscaras, las canciones, las mejoras, la espada y el escudo y los contenedores de corazón. Darle o quitarle uno a un
  jugador lo cambia para todos los que juegan en la partida. Son de cada jugador las rupias, la vida, la magia, la
  munición y lo que hay en sus botellas.

Lo que pasa en el juego llega como eventos: `player_item`, `player_death`, `enemy_killed`, `boss_defeated` y
`player_stats` (vida, magia y rupias, como mucho 4 veces por segundo). `coop.players.get(p)` también da la última
vida, magia y rupias conocidas.

## 5. Opciones del juego que se pueden forzar

Las opciones de 2 Ship (las del menú F1: trucos, dificultad, modos) se pueden imponer **mientras se juega en la
partida del servidor**. Al salir de ella, al desconectarse o al apagar el juego, cada jugador recupera las suyas.

- Para todos, siempre: en `server.json`, `"gameSettings": { "gCheats.InfiniteMagic": 1 }`.
- Desde un script: `coop.game.setSetting("*", "gEnhancements.DifficultyOptions.DamageMultiplier", 2)` (para todos,
  también para quien entre después) o con un jugador o una lista (solo ellos, hasta que se desconecten).
  `coop.game.clearSetting(target, name)` la quita; `coop.game.settings()` dice cuáles hay.

Se pueden forzar las que empiezan por `gEnhancements.`, `gCheats.`, `gModes.` y `gFixes.`, salvo las de cada jugador
(`gEnhancements.A11y.`, `.Camera.`, `.Graphics.`, `.Saving.`, `.Mods.`, `.Playback.`, `.Dpad.`), «borrar el archivo al
morir» y las que el co-op ya fija en la partida del servidor (saltar cinemáticas, «el tiempo se mueve si te mueves»...).
Como mucho 64 por jugador. El valor es un número: `1`/`0` en las casillas, la posición en las listas, decimales en los
deslizadores de decimales.

Algunas útiles (el nombre exacto de cualquier otra sale en el archivo de ajustes del juego,
`2ship2harkinian.json` → `CVars`):

| Opción | Valores | Qué hace |
|---|---|---|
| `gCheats.InfiniteHealth` | 1 | vida infinita |
| `gCheats.InfiniteMagic` | 1 | magia infinita |
| `gCheats.InfiniteRupees` | 1 | rupias infinitas |
| `gCheats.InfiniteConsumables` | 1 | munición y objetos infinitos |
| `gCheats.MoonJumpOnL` | 1 | salto lunar con L |
| `gCheats.UnbreakableRazorSword` | 1 | la Espada de Navaja no se desgasta |
| `gEnhancements.DifficultyOptions.DamageMultiplier` | 0 (1x), 1 (2x), 2 (4x), 3 (8x), 4 (16x), 10 (un golpe) | daño que recibe Link |
| `gEnhancements.DifficultyOptions.BossHealthMultiplier` | 0 (1x) a 4 (2x) | vida de los jefes (al recargar la escena) |
| `gEnhancements.DifficultyOptions.HyperEnemies` | 1 | los enemigos se mueven el doble de rápido |
| `gEnhancements.DifficultyOptions.NoHeartDrops` | 1 | no salen corazones |
| `gEnhancements.DifficultyOptions.PermanentHeartLoss` | 1 | perder 4 cuartos de corazón quita el contenedor |
| `gEnhancements.DifficultyOptions.DisableTakkuriSteal` | 1 | Takkuri no roba |
| `gEnhancements.Player.ClimbSpeed` | 1 a 5 | velocidad al trepar |
| `gEnhancements.Masks.FastTransformation` | 1 | transformaciones sin animación |
| `gEnhancements.Masks.FierceDeitysAnywhere` | 1 | Deidad Fiera fuera de los jefes |
| `gEnhancements.Timesavers.FastChests` | 1 | cofres rápidos |
| `gModes.MirroredWorld.Mode` | 0 (no), 1 (siempre), 2 (al azar)... | mundo en espejo |
| `gModes.PlayAsKafei` | 1 | jugar como Kafei (al recargar la escena) |

## 6. El mundo compartido: campos y banderas

La partida del servidor guarda el progreso común en **campos** (`coop.world.fields()` los lista):

| Campo | Tipo | Qué guarda |
|---|---|---|
| `weekEventReg` | bits | misiones y sucesos del ciclo |
| `sceneFlags` | bits | por escena: cofres, interruptores, salas limpias, objetos recogidos |
| `sceneRooms`, `sceneExtra` | bits | salas visitadas; fuentes de hadas, pisos de mazmorras |
| `owls` | bits | estatuas de búho activadas |
| `mapsVisible`, `regions`, `clouds` | bits | mapas de Tingle, regiones visitadas, nubes del mapa |
| `upgrades`, `quest` | bits | mejoras (carcaj, bolsa, cartera...) y objetos de misión (canciones, restos) |
| `dungeonItems` | bits | mapa, brújula y llave del jefe de cada mazmorra |
| `items`, `masks` | bytes | objetos y máscaras (el id del objeto en cada casilla) |
| `equipment`, `magicFlags`, `defense`, `progress`, `resets`, `codes` | bytes | espada y escudo, magia, doble defensa, progreso, códigos (lotería, Bombers...) |
| `heartQuarters`, `bottles`, `keys`, `fairies`, `skulls` | contadores | cuartos de corazón máximos, botellas, llaves, hadas perdidas, Skulltulas |

```lua
if coop.world.getBit("owls", 0, 0x01) then print("el búho de Ciudad Reloj está activado") end
coop.world.setBits("owls", 0, 0x01)        -- banderas: poner (y, opcional, quitar) bits de un byte
coop.world.set("masks", 0, 0x32)           -- bytes: escribir un valor
coop.world.add("heartQuarters", 0, 4)      -- contadores: sumar (un corazón son 4 cuartos)
```

Lo que cambia un mod llega al momento a todos los que juegan en la partida (y el evento `world_change` lo cuenta con
`player = 0`). El reloj: `coop.world.time()`, `setTime(día, hora, minuto)`, `setStopped(true)`, `setSpeed(0.5)`,
`restart()` (nuevo ciclo sin votación) y `crashMoon()`.

## 7. Otros parámetros del servidor

Además de los de siempre (`port`, `maxPlayers`, `password`, `motd`, `language`, `sharedEnemies`...), `server.json`
tiene:

| Clave | Por defecto | Qué es |
|---|---|---|
| `timeSpeed` | `1.0` | velocidad de los tres días (0.1 a 10): `0.5` = días el doble de largos |
| `voteSeconds` | `30` | lo que dura la votación de la Canción del Tiempo (10 a 300) |
| `saveSeconds` | `10` | cada cuánto se guarda el mundo en el disco (2 a 600) |
| `giftMax` | `999` | rupias máximas de un `/gift` (1 a 999) |
| `commandPermissions` | `{}` | quién puede usar cada comando: `{"tp": "op", "gift": "console", "dado": "player"}` (`player`, `op` o `console`; también los de los mods) |
| `gameSettings` | `{}` | opciones de 2 Ship forzadas a todos (apartado 5) |

Un script los lee con `coop.server.config("timeSpeed")` y cambia algunos en marcha con `coop.server.setConfig`
(sin guardarlos en el archivo).

## 8. Depurar

- `print(...)` escribe en la consola del servidor y en `logs/server.log`, con el nombre del mod delante.
  `coop.server.log(texto, "warn")` lo mismo con nivel.
- Un error dentro de un manejador se registra con el archivo, la línea y la traza: `[hola] error en chat:
  hola.lua:12: attempt to index a nil value`. El manejador sigue suscrito. Tras 20 errores del mismo manejador se deja
  de registrar (no de ejecutar).
- Un error al cargar (sintaxis, un evento que no existe) deja el mod sin cargar y sin nada a medias; los demás mods
  arrancan igual.
- `/mod reload nombre` recarga sin parar el servidor; `/mods` dice qué hay cargado.
- `coop.server.exec("list")` ejecuta un comando y devuelve su respuesta: útil para probar desde un script.

## 9. Seguridad y límites

- Un script no puede leer la contraseña del servidor ni el token de los anfitriones, ni tocar archivos (salvo con
  `"unsafeLua": true`, solo para scripts de confianza).
- Un manejador que tarda más de `scriptTimeoutMs` (2 s) se corta con un error; un script que pasa de `scriptMemoryMb`
  (64 MB) recibe un error de memoria. El servidor sigue.
- Un plugin es código nativo: un fallo dentro de él tumba el servidor. Instala solo DLL de quien te fíes.
- El juego de cada jugador comprueba todo lo que le llega (rangos, ids, entradas, opciones permitidas) y no ejecuta
  nada fuera de la partida del servidor.
- Los avisos que manda el juego (`gev`, `stat`) los valida el servidor y tienen un límite de ritmo.
- Eventos encadenados (un manejador que lanza otro evento, y así): como mucho 8 de profundidad.

## 10. Recetas

**Dar un premio al matar enemigos**:

```lua
coop.on("enemy_killed", function(e)
    if e.actorKey == "EN_DODONGO" then
        coop.game.giveRupees(e.player, 20)
        coop.game.notify(e.player, "+20 rupias por el Dodongo")
    end
end)
```

**Un mensaje de bienvenida en la partida, con el cuadro de texto del juego**:

```lua
coop.on("world_enter", function(e)
    coop.game.message(e.player, "%gBienvenido%w a Termina.\nEl reloj no se para: daos prisa.")
end)
```

**Más difícil de noche**:

```lua
coop.on("world_hour", function(e)
    coop.game.setSetting("*", "gEnhancements.DifficultyOptions.DamageMultiplier", e.night and 1 or 0)
end)
```

**Días más largos con pocos jugadores**:

```lua
local function Ajustar()
    coop.world.setSpeed(#coop.players.inWorld() <= 1 and 0.5 or 1)
end
coop.on("world_enter", Ajustar)
coop.on("world_leave", Ajustar)
```

**Un teletransporte por nombre de sitio**:

```lua
coop.commands.register("ir", { usage = "/ir <escena>", minArgs = 1 }, function(ctx, args)
    if not ctx.player then return "Solo para jugadores." end
    local ok, err = pcall(coop.game.warp, ctx.player, args[1]:upper())
    return ok and "Viajando..." or err, ok and "ok" or "warn"
end)
```

**Bloquear un comando a ciertas horas**:

```lua
coop.on("command", function(e)
    local t = coop.world.time()
    if e.name == "tp" and t and t.night then
        coop.chat.tell(e.player, "De noche no hay /tp.", "warn")
        return false
    end
end)
```

## 11. Límites conocidos

- Los actores creados con `game.spawn` son de cada juego (no se replican ni comparten vida), un enemigo no aparece en
  una sala ya despejada, y algunos actores solo funcionan en su propia escena (jefes, mecanismos).
- No hay scripts dentro del juego ni interfaz, modelos o texturas nuevas: eso son los mods de 2 Ship (`mods/*.o2r`).
- No hay un evento por fotograma ni por pose: para lógica periódica, `coop.timer.every`.
- `world_change` no se puede cancelar (el juego que lo envió ya lo aplicó).
- `enemy_killed` llega de los enemigos que acaban con el «golpe final» del juego (casi todos), y no de los que simula
  un anfitrión sin ventana del servidor.
- Los sonidos no tienen nombres (son miles): se usan sus números (`NA_SE_*` en `mm/include/sfx.h` del código del
  juego).
- Las opciones forzadas se aplican cuando cambia algo; algunas mejoras de 2 Ship solo leen su opción al cargar la
  escena (lo dice su ayuda en el menú F1).
