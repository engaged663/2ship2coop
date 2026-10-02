# Referencia de la API de mods

> Generada por `2ship-coop-server --mod-docs <carpeta>` a partir del código. No la edites a mano.

Guía para empezar: [README.md](README.md). Nombres de objetos, actores y escenas: [IDS.md](IDS.md). Plugins DLL: [PLUGINS.md](PLUGINS.md).

## Funciones

En un script se llaman `coop.<espacio>.<función>(...)`; en un plugin, `api.Call("<espacio>.<función>", { ... })` con los mismos argumentos en una lista. Un argumento con `?` se puede omitir (o pasar `nil`). Si uno está mal, la función lanza un error que empieza por su nombre (en Lua, con el archivo y la línea; `pcall` lo atrapa).

Argumentos que se repiten:

- **player**: un jugador conectado, por su id (número) o su nick.
- **target**: un jugador, una lista de jugadores o `"*"` (todos). Las órdenes al juego (`game.*`) solo llegan a los que juegan en la partida del servidor y devuelven a cuántos llegaron.
- **item**, **actor**, **scene**: un id del juego o su nombre de [IDS.md](IDS.md) (`"MASK_BUNNY"`, `"EN_DODONGO"`, `"SOUTH_CLOCK_TOWN"`).
- **level**: `"info"` (si se omite), `"ok"`, `"warn"` o `"error"`: el color de la línea en el chat.

### Funciones que reciben funciones

Las da cada motor: en un plugin son métodos de `coop::Plugin` (`On`, `Off`, `After`, `Every`, `CancelTimer`, `Command`; mira [PLUGINS.md](PLUGINS.md)).

#### `coop.on(event, fn)`

Devuelve: número

Escucha un evento: uno de la lista de abajo o uno propio entre mods (con `:` en el nombre). `fn(e)` recibe la tabla del evento; en uno que se puede cancelar, `return false` (o `e.cancel = true`) lo cancela, y asignar un campo que se puede cambiar (`e.text = "..."`) lo cambia. Devuelve el id de la suscripción. Un nombre sin `:` que no está en la lista es un error (así no pasa desapercibido uno mal escrito).

#### `coop.off(id)`

Devuelve: booleano

Deja de escuchar: el id que devolvió `coop.on`.

#### `coop.emit(name, payload?)`

Devuelve: tabla, booleano

Lanza un evento propio para otros mods (el nombre lleva `:`, como `"economia:pago"`) con la tabla `payload`. Devuelve la tabla tal como la dejaron sus manejadores (pueden cambiar cualquier campo) y si alguno lo canceló. Los plugins pueden escuchar estos eventos, pero solo los scripts los lanzan.

#### `coop.timer.after(ms, fn)`

Devuelve: número

Llama a `fn()` una vez, dentro de `ms` milisegundos (como poco 10; como mucho 30 días). Devuelve el id del temporizador.

#### `coop.timer.every(ms, fn)`

Devuelve: número

Llama a `fn()` cada `ms` milisegundos (como poco 10) hasta que se cancele o se descargue el mod. Devuelve su id.

#### `coop.timer.cancel(id)`

Devuelve: booleano

Cancela un temporizador de este mod.

#### `coop.commands.register(name, opts?, fn)`

Devuelve: nada

Crea el comando `/name`. `opts` = `{ usage = "/name <x>", help = "...", perm = "player" | "op" | "console", minArgs = 0, aliases = { "otro" } }` (todo opcional; `perm` dice quién puede usarlo y `commandPermissions` de `server.json` puede cambiarlo). `fn(ctx, args)` recibe quién lo escribe (`ctx.player`, que falta si es la consola; `ctx.nick`, `ctx.isConsole`, `ctx.isOp`) y sus argumentos (textos); lo que devuelve es la respuesta: un texto y, si quieres, su nivel (`"ok"`, `"warn"`, `"error"`). `/help` lo muestra con su ayuda. Un nombre que ya existe es un error.

#### `coop.commands.unregister(name)`

Devuelve: booleano

Quita un comando de este mod.

### server

#### `coop.server.config(key)`

Devuelve: valor

Lee una opción de `server.json`: `port`, `language`, `maxPlayers`, `motd`, `sharedEnemies`, `sharedProps`, `endingForAll`, `groups`, `bossCutscenes`, `inviteSeconds`, `effects`, `timeSpeed`, `voteSeconds`, `saveSeconds`, `giftMax`, `commandPermissions`, `gameSettings`. La contraseña no se puede leer.

#### `coop.server.exec(line)`

Devuelve: texto

Ejecuta un comando como si se escribiera en la consola del servidor (con todos los permisos), por ejemplo `coop.server.exec("kick Ana molestar")`. Devuelve lo que el comando respondió.

#### `coop.server.info()`

Devuelve: tabla

Datos del servidor: `protocol` (versión del protocolo), `language` (`es`, `en`, `zh`, `ru`), `port`, `maxPlayers`, `players` (conectados ahora), `uptimeMs`, `timeSpeed` y `mods` (cuántos hay cargados).

#### `coop.server.log(text, level?)`

Devuelve: nada

Escribe una línea en el registro del servidor (consola y `logs/server.log`) con el nombre del mod delante. `level`: `info` (por defecto), `warn` o `error`.

#### `coop.server.now()`

Devuelve: número

Milisegundos desde que arrancó el servidor. Sirve para medir tiempos; no es la hora del día.

#### `coop.server.setConfig(key, value)`

Devuelve: nada

Cambia una opción con el servidor en marcha (no se guarda en `server.json`): `motd`, `password`, `maxPlayers` (1-4), `inviteSeconds`, `bossCutscenes`, `effects`, `voteSeconds`, `saveSeconds`, `giftMax` y `timeSpeed`.

#### `coop.server.stop(reason?)`

Devuelve: nada

Detiene el servidor de forma ordenada (avisa a los jugadores y guarda el mundo).

### players

#### `coop.players.ban(player, reason?)`

Devuelve: nada

Banea el nick y la IP de un jugador conectado y lo expulsa (como `/ban`).

#### `coop.players.count()`

Devuelve: número

Cuántos jugadores hay conectados.

#### `coop.players.find(nick)`

Devuelve: número o nil

El id del jugador con ese nick (sin distinguir mayúsculas), o `nil`.

#### `coop.players.get(player)`

Devuelve: tabla o nil

Los datos de un jugador (por id o por nick), o `nil` si no está conectado: `id`, `nick`, `ip`, `op`, `scene`, `sceneKey`, `sceneName`, `room`, `entrance`, `x`, `y`, `z`, `rot`, `inWorld` (juega en la partida del servidor), `busy` (en una cinemática), `group` (0: sin grupo), `activity` (clave del minijuego), `form` (0 Deidad Fiera, 1 Goron, 2 Zora, 3 Deku, 4 humano), `mask`, `connectedMs` y, cuando su juego los ha comunicado, `health`, `maxHealth`, `magic` y `rupees`.

#### `coop.players.inScene(scene)`

Devuelve: lista

Los ids de los jugadores que están en esa escena (su id o su nombre, como `"SOUTH_CLOCK_TOWN"`).

#### `coop.players.inWorld()`

Devuelve: lista

Los ids de los jugadores que están jugando en la partida del servidor.

#### `coop.players.isOp(player)`

Devuelve: booleano

Si ese jugador es administrador.

#### `coop.players.kick(player, reason?)`

Devuelve: nada

Expulsa a un jugador del servidor.

#### `coop.players.list()`

Devuelve: lista

Los ids de los jugadores conectados.

#### `coop.players.near(player, radius)`

Devuelve: lista

Los ids de los otros jugadores de su misma escena que están a `radius` unidades o menos (un Link mide unas 60 de alto).

#### `coop.players.setOp(player, op)`

Devuelve: nada

Da (`true`) o quita (`false`) los permisos de administrador a un jugador conectado. Quedan ligados a su nick y a la IP que tiene ahora.

#### `coop.players.teleport(player, dest)`

Devuelve: nada

Lleva a un jugador junto a otro (`dest` = id o nick, como `/tp`) o a un punto exacto: `dest` = `{ scene = "SOUTH_CLOCK_TOWN", x = 0, y = 0, z = 0, room = 0, rot = 0 }` (`scene` por nombre o id; también vale `entrance` en su lugar). Si Link está en un diálogo o una cinemática, su juego no lo mueve.

### chat

#### `coop.chat.broadcast(text, level?)`

Devuelve: nada

Escribe una línea de sistema en el chat de todos los jugadores conectados (y en el registro). `level` le da color: `info` (por defecto), `ok`, `warn` o `error`. El texto puede tener hasta 8 líneas separadas por `\n`.

#### `coop.chat.say(name, text)`

Devuelve: nada

Escribe en el chat de todos una línea normal, como si la dijera `name` (un personaje, un bot): `<Tatl> ¡Escucha!`. `name` no tiene que ser un jugador.

#### `coop.chat.tell(player, text, level?)`

Devuelve: nada

Escribe una línea de sistema solo en el chat de ese jugador.

### world

#### `coop.world.add(field, offset, delta)`

Devuelve: número

Suma `delta` (puede ser negativo) a un contador (`heartQuarters`, `keys`, `fairies`, `skulls`, `bottles`). Devuelve lo que se sumó de verdad (el contador no sale de su rango).

#### `coop.world.crashMoon()`

Devuelve: nada

La luna cae ahora: el mundo vuelve al principio del ciclo, como si nadie hubiera tocado la Canción del Tiempo.

#### `coop.world.cycle()`

Devuelve: número

El número del ciclo de tres días en curso (1 el primero; 0 si todavía no hay mundo).

#### `coop.world.exists()`

Devuelve: booleano

Si el mundo del servidor ya existe (lo crea el primer jugador que entra en la partida).

#### `coop.world.fields()`

Devuelve: lista

Los campos del mundo compartido: `{ name, size (bytes), kind }`, con `kind` = `bits` (banderas), `bytes` (valores) o `counter` (contadores).

#### `coop.world.get(field, offset)`

Devuelve: número

Lee un byte de un campo del mundo (por nombre o por índice); en un campo `counter`, el contador que empieza en ese byte.

#### `coop.world.getBit(field, offset, mask)`

Devuelve: booleano

Si todos los bits de `mask` están puestos en ese byte. Ejemplo: `coop.world.getBit("owls", 0, 0x01)`.

#### `coop.world.restart()`

Devuelve: nada

Vuelve al Amanecer del Primer Día sin votación (como `/reiniciar`): cada jugador conserva lo que conserva la Canción del Tiempo.

#### `coop.world.set(field, offset, value)`

Devuelve: booleano

Escribe un byte de un campo `bytes` (por ejemplo una máscara en `masks`) o fija un contador, para todos los jugadores. Devuelve si algo cambió.

#### `coop.world.setBits(field, offset, set, clear?)`

Devuelve: booleano

Pone los bits de `set` y quita los de `clear` en un byte de un campo `bits` (banderas de misiones, cofres, búhos...), para todos los jugadores. Devuelve si algo cambió.

#### `coop.world.setSpeed(speed)`

Devuelve: nada

Cambia la velocidad a la que pasan los tres días: 1 es la del juego original, 0.5 la mitad (días el doble de largos), 2 el doble. Entre 0.1 y 10.

#### `coop.world.setStopped(stopped)`

Devuelve: nada

Detiene (`true`) o reanuda (`false`) el reloj del mundo, como `/freezetime`. Se queda así aunque entren o salgan jugadores.

#### `coop.world.setTime(day, hour, minute?)`

Devuelve: nada

Salta a ese momento para todos (día 1-3, hora 0-23). Los juegos recargan su escena, como con `/settime`.

#### `coop.world.time()`

Devuelve: tabla o nil

El reloj del mundo: `day` (1-3), `hour`, `minute`, `night`, `stopped`, `inverted` (Canción del Tiempo Invertida), `speed` (velocidad del tiempo), `abs` (unidades desde el día 1 a las 6:00; un día son 65536) y `time` (la hora como la guarda el juego). `nil` si no hay mundo.

### game

#### `coop.game.clearSetting(target, name)`

Devuelve: nada

Deja de forzar esa opción: la de todos con `"*"`, o la propia de esos jugadores.

#### `coop.game.damage(target, amount)`

Devuelve: número

Quita `amount` de vida (16 = un corazón; la doble defensa lo reduce a la mitad), sin retroceso. Si se queda sin vida, Link muere como en el juego (un hada embotellada lo revive).

#### `coop.game.giveItem(target, item)`

Devuelve: número

Da un objeto (por nombre, como `"MASK_BUNNY"`, o por id) como lo da el juego pero sin la animación del cofre: objetos, máscaras, contenidos de botella (llenan una vacía), equipo, munición, mejoras, canciones, corazones y rupias. Los pocos ids que el juego no sabe dar (`SWORD_DEITY`, `WALLET_DEFAULT`, `FISHING_ROD`, `STRAY_FAIRIES`, `INVALID_*`) dan error. Los nombres están en `IDS.md`.

#### `coop.game.giveRupees(target, amount)`

Devuelve: número

Da rupias, o las quita con un número negativo (de -9999 a 9999). Lo que no cabe en la cartera se pierde.

#### `coop.game.heal(target, amount?)`

Devuelve: número

Cura `amount` de vida (16 = un corazón). Sin `amount`, o con 0, cura toda.

#### `coop.game.idOf(kind, name)`

Devuelve: número o nil

El id de un nombre, o `nil` si el juego no tiene nada con ese nombre. No distingue mayúsculas y admite el prefijo del juego (`ITEM_`, `ACTOR_`, `SCENE_`).

#### `coop.game.ids(kind)`

Devuelve: tabla

Todos los nombres de un tipo (`"item"`, `"actor"` o `"scene"`) con su id: `{ MASK_BUNNY = 57, ... }`.

#### `coop.game.kill(target)`

Devuelve: número

Deja sin vida a esos jugadores.

#### `coop.game.magic(target, amount?)`

Devuelve: número

Da magia, o la quita con un número negativo (48 es la barra normal, 96 la doble). Sin `amount`, o con 0, la llena.

#### `coop.game.message(target, text)`

Devuelve: número

Abre un cuadro de texto del juego con ese mensaje, como el de un cartel (admite saltos de línea; cada 4 líneas, un cuadro nuevo). La letra del juego no tiene tildes: se quitan (`á` → `a`, `ñ` → `n`, sin `¡ ¿`); `%r` `%g` `%b` `%y` `%p` `%w` cambian el color (rojo, verde, azul, amarillo, rosa, blanco). Si Link está ocupado (otro diálogo, una cinemática, el menú, a caballo), espera hasta 10 segundos a que quede libre; si no, el mensaje le llega como línea de chat.

#### `coop.game.nameOf(kind, id)`

Devuelve: texto o nil

El nombre de un id, o `nil` si no existe.

#### `coop.game.notify(target, text, seconds?)`

Devuelve: número

Muestra un aviso emergente (la notificación de 2 Ship, en una esquina de la pantalla) durante `seconds` segundos (de 1 a 30; 6 si no se indica). El texto se corta a 400 caracteres.

#### `coop.game.setSetting(target, name, value)`

Devuelve: nada

Fuerza una opción de 2 Ship (un CVar de juego: `gEnhancements.*`, `gCheats.*`, `gModes.*`, `gFixes.*`) mientras se juegue en la partida del servidor; al salir, cada juego recupera la suya. Con `target` = `"*"` vale para todos, también para quien entre después; con un jugador o una lista, solo para ellos y hasta que se desconecten. `value`: un número entero para las opciones enteras (casillas: 1 o 0; listas), uno con decimales para los deslizadores (`2.0`, no `2`); `true` y `false` valen 1 y 0. Como mucho 64 opciones por jugador.

#### `coop.game.settings(player?)`

Devuelve: tabla

Las opciones forzadas, `{ nombre = valor }`: sin argumentos, las de todos (incluidas las de `gameSettings` de `server.json`); con un jugador, todas las que tiene él.

#### `coop.game.sound(target, sfx)`

Devuelve: número

Reproduce un efecto de sonido del juego por su id (los `NA_SE_*` del juego, de 0 a 65535; por ejemplo `0x4802` es la melodía de acierto, `0x4803` una rupia y `0x4806` el de error).

#### `coop.game.spawn(target, actor, opts?)`

Devuelve: número

Crea un actor (por nombre, como `"EN_DODONGO"`, o por id) en el juego de esos jugadores. `opts` = `{ params = 0, distance = 100, rotY = 0, x = ..., y = ..., z = ... }`: sin `x, y, z` aparece `distance` unidades delante de Link (de 0 a 2000). Cada juego crea el suyo: no se comparte con los demás jugadores. Se crea aunque la escena no use ese actor, pero un enemigo no aparece en una sala ya despejada y algunos actores solo funcionan en su propia escena (jefes, mecanismos).

#### `coop.game.takeItem(target, item)`

Devuelve: número

Quita un objeto de la pantalla de objetos o de máscaras si lo tiene (un contenido de botella se lleva su botella). Con el resto (equipo, canciones, munición) no hace nada, y nunca quita la máscara que Link lleva puesta ni la de su forma.

#### `coop.game.unlockAll(target)`

Devuelve: número

Da a esos jugadores todos los objetos, máscaras, canciones y corazones (lo que hace `/unlockall`).

#### `coop.game.warp(target, scene, spawn?)`

Devuelve: número

Lleva a esos jugadores a una escena (por nombre, como `"SOUTH_CLOCK_TOWN"`, o por id) por su entrada número `spawn` (de 0 a 31; 0 si no se indica). Si Link está en un diálogo o una cinemática, espera hasta 10 segundos.

### storage

#### `coop.storage.get(key, default?)`

Devuelve: valor

Lo que este mod guardó con esa clave, o `default` si no hay nada. Los datos de cada mod son suyos: otro mod no los ve.

#### `coop.storage.keys()`

Devuelve: lista

Las claves que este mod tiene guardadas, en orden alfabético.

#### `coop.storage.remove(key)`

Devuelve: booleano

Borra una clave. Devuelve si existía.

#### `coop.storage.save()`

Devuelve: nada

Escribe ahora mismo los datos en el disco (normalmente no hace falta).

#### `coop.storage.set(key, value)`

Devuelve: nada

Guarda un valor (número, texto, booleano o tabla) que seguirá ahí cuando el servidor se reinicie. `nil` lo borra. Se escribe en `<dataDir>/<mod>.json` como mucho cada 2 segundos y al parar.

### mod

#### `coop.mod.describe(info)`

Devuelve: nada

Dice qué es este mod para el comando `/mods`: `{ title = "...", version = "1.0", author = "...", description = "..." }` (todos opcionales).

#### `coop.mod.list()`

Devuelve: lista

Los mods cargados, en orden de carga: `{ name, kind ("lua" o "plugin"), title, version, author, description }`.

#### `coop.mod.name()`

Devuelve: texto

El nombre de este mod: el de su archivo, en minúsculas y sin extensión (`mods/Mi Mod.lua` es `mi_mod`).

#### `coop.mod.setting(key, default?)`

Devuelve: valor

Un ajuste de este mod escrito por el dueño del servidor en `server.json`, dentro de `mods.settings.<nombre del mod>`. Si no está, devuelve `default`.

## Eventos

Un script escucha un evento con `coop.on("nombre", function(e) ... end)` y un plugin con `api.On("nombre", ...)`. El manejador recibe los campos del evento (`e.nick`, `e.player`...); `player` es el id del jugador y vale para las funciones mientras siga conectado. En los eventos que se pueden cancelar, `return false` (o `e.cancel = true`) impide lo que anuncian, y los campos que se pueden cambiar se cambian asignándolos. Los manejadores se llaman en el orden en que se suscribieron; si uno cancela, los siguientes no lo reciben.

### `server_start`

El servidor ha arrancado y los mods configurados están cargados.

Sin campos.

### `server_stop`

El servidor va a detenerse.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `reason` | string |  | Motivo de la parada |

### `player_connect` (se puede cancelar)

Un jugador ha pasado las comprobaciones del servidor (versión, nick, baneos, contraseña) y va a entrar. Cancelarlo lo rechaza con `reason`.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `nick` | string |  | Nick con el que entra |
| `ip` | string |  | Su dirección IP |
| `reason` | string | sí | Motivo que verá si se cancela |

### `player_join`

Un jugador ha entrado en el servidor.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id del jugador |
| `nick` | string |  | Su nick |
| `ip` | string |  | Su dirección IP |

### `player_leave`

Un jugador se ha desconectado. Su id todavía vale dentro del manejador.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id del jugador |
| `nick` | string |  | Su nick |
| `reason` | string |  | Motivo (desconectado, expulsado...) |

### `player_scene`

Un jugador ha cambiado de escena o de sala.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id del jugador |
| `nick` | string |  | Su nick |
| `scene` | int |  | Id de la escena |
| `sceneKey` | string |  | Clave de la escena (`SOUTH_CLOCK_TOWN`...; vacía si no se conoce) |
| `sceneName` | string |  | Nombre que muestra el juego |
| `room` | int |  | Sala |
| `entrance` | int |  | Entrada por la que llegó |
| `prevScene` | int |  | Escena anterior (-1: ninguna) |
| `prevRoom` | int |  | Sala anterior |

### `player_stats`

La vida, la magia o las rupias de un jugador han cambiado (como mucho 4 avisos por segundo y jugador).

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id del jugador |
| `nick` | string |  | Su nick |
| `health` | int |  | Vida (16 = un corazón) |
| `maxHealth` | int |  | Vida máxima |
| `magic` | int |  | Magia (48 = barra normal, 96 = doble) |
| `rupees` | int |  | Rupias |
| `prevHealth` | int |  | Vida anterior |
| `prevMagic` | int |  | Magia anterior |
| `prevRupees` | int |  | Rupias anteriores |

### `player_death`

Un jugador se ha quedado sin vida (aunque un hada lo reviva después).

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id del jugador |
| `nick` | string |  | Su nick |
| `scene` | int |  | Escena en la que estaba |

### `player_item`

El juego ha dado un objeto a un jugador (también munición, corazones y rupias recogidas).

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id del jugador |
| `nick` | string |  | Su nick |
| `item` | int |  | Id del objeto |
| `itemKey` | string |  | Su nombre (`MASK_BUNNY`...; vacío si no se conoce) |

### `enemy_killed`

Un enemigo ha recibido el golpe final. `player` es quien lo dio (o el juego que lo simulaba si no se sabe).

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id del jugador |
| `nick` | string |  | Su nick |
| `actor` | int |  | Id del actor |
| `actorKey` | string |  | Su nombre (`EN_DODONGO`...) |
| `params` | int |  | Parámetros del actor |
| `scene` | int |  | Escena |
| `room` | int |  | Sala |
| `x` | number |  | Posición |
| `y` | number |  | Posición |
| `z` | number |  | Posición |

### `boss_defeated`

Un jefe ha sido derrotado en el juego de ese jugador.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id del jugador |
| `nick` | string |  | Su nick |
| `actor` | int |  | Id del actor del jefe |
| `actorKey` | string |  | Su nombre |
| `scene` | int |  | Escena |

### `chat` (se puede cancelar)

Un jugador escribe en el chat. Cancelarlo impide que el mensaje se envíe.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id del jugador |
| `nick` | string |  | Su nick |
| `text` | string | sí | El mensaje |

### `command` (se puede cancelar)

Alguien va a ejecutar un comando (ya con permiso y argumentos suficientes). Cancelarlo impide que se ejecute.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id del jugador (0: la consola) |
| `nick` | string |  | Su nick |
| `name` | string |  | Comando, sin la barra |
| `args` | list |  | Argumentos |
| `line` | string |  | La línea completa |

### `world_enter`

Un jugador ha entrado en la partida del servidor.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id del jugador |
| `nick` | string |  | Su nick |

### `world_leave`

Un jugador ha salido de la partida del servidor.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id del jugador |
| `nick` | string |  | Su nick |

### `world_created`

Se ha creado el mundo del servidor (lo creó el juego de ese jugador).

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id del jugador |
| `nick` | string |  | Su nick |

### `world_change`

El mundo compartido ha cambiado (ya aplicado). Las listas son las de `wops`: bits `[campo, byte, pone, quita]`, bytes `[campo, byte, valor]`, adds `[campo, byte, cuánto]`.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id de quien lo cambió (0: el servidor o un mod) |
| `bits` | list |  | Cambios de bits |
| `bytes` | list |  | Cambios de bytes |
| `adds` | list |  | Cambios de contadores |

### `world_hour`

El reloj del mundo ha pasado a otra hora del juego.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `day` | int |  | Día (1-3) |
| `hour` | int |  | Hora (0-23) |
| `night` | bool |  | Es de noche (18:00-5:59) |
| `abs` | int |  | Reloj absoluto (0 = día 1, 6:00) |
| `jump` | bool |  | El reloj saltó (canción, comando, ciclo nuevo) en vez de avanzar |

### `vote_start` (se puede cancelar)

Un jugador propone volver al Amanecer del Primer Día (Canción del Tiempo). Cancelarlo impide la votación.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id del jugador |
| `nick` | string |  | Su nick |

### `vote_end`

La votación de la Canción del Tiempo ha terminado.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `passed` | bool |  | Ha salido adelante |

### `cycle_reset`

Ha empezado un ciclo nuevo de tres días.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `cycle` | int |  | Número del ciclo nuevo |
| `reason` | string |  | `sot` (votación), `moon` (cayó la luna), `restart` (comando) o `ending` (final del juego) |

### `moon_crash`

El reloj ha llegado al final del tercer día: la luna cae.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `cycle` | int |  | Ciclo que termina |

### `group_change`

Un grupo ha cambiado de miembros o se ha deshecho (sin miembros).

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `group` | int |  | Id del grupo |
| `leader` | int |  | Id del líder (0: deshecho) |
| `members` | list |  | Ids de sus miembros |

### `activity`

Un jugador empieza o termina un minijuego, o comunica su resultado.

| Campo | Tipo | Se puede cambiar | Qué es |
|---|---|---|---|
| `player` | int |  | Id del jugador |
| `nick` | string |  | Su nick |
| `state` | string |  | `start`, `end` o `result` |
| `key` | string |  | Clave del minijuego |
| `name` | string |  | Su nombre |
| `score` | int |  | Puntos (al terminar) |
| `cs` | int |  | Tiempo en centésimas |
| `won` | bool |  | Ganó (solo en `result`) |

### `mod_unload`

Este mod va a descargarse (solo lo recibe él).

Sin campos.

