# Co-op mod — mapa del código

Mod cooperativo online para 2 Ship 2 Harkinian (hasta 4 jugadores por IP, servidor dedicado).
Diseño completo: `docs/superpowers/specs/2026-09-26-coop-a-nucleo-online-design.md` (A) y
`docs/superpowers/specs/2026-09-26-coop-b-mundo-compartido-design.md` (B).
**Lee este archivo y luego solo el archivo que vayas a tocar.**

Estado: **A (núcleo online)**, **B (mundo compartido)**, **C1/D1** (enemigos compartidos, anfitrión del servidor) y
**D3** (réplica completa de la memoria de enemigos y NPCs) hechos; además objetos del escenario compartidos, final del
juego para todos, el tiempo nunca se detiene, pasajeros de Epona entre escenas y **grupos** (invitaciones, minijuegos,
misiones, diálogos y cinemáticas compartidos; jefes para toda la escena: ver «Grupos, minijuegos y misiones»), con sus
límites arreglados (actores de cinemática y efectos para quien mira, minijuegos Juntos / Cada uno / Por turnos,
contactos, resultados y textos con los valores de quien habla). Pendiente: D2 (spec D).

## Piezas

| Pieza | Carpeta | Qué es |
|---|---|---|
| Librería común | `coop/common/` | protocolo + red (ENet), sin nada del juego. La usan servidor, bot, tests y juego |
| Servidor | `coop/server/` → `2ship-coop-server.exe` | consola; autoridad de jugadores, chat, comandos, mundo y reloj |
| Bot de pruebas | `coop/tools/CoopBot.cpp` → `2ship-coop-bot.exe` | jugador falso para probar con un solo PC |
| Tests | `coop/tests/` → `coop-tests.exe` | unitarios + integración (servidor real en loopback) |
| Cliente (juego) | `mm/2s2h/Coop/` | conexión, chat, otros Links, /tp, /gift, menú, jugar en el mundo del servidor |

## Mapa de archivos

### `coop/common/`
| Archivo | Responsabilidad |
|---|---|
| `Protocol.h` | versión, puerto (7780/UDP), límites, nombres de eventos (`ev::`), niveles (`level::`) |
| `Events.h/.cpp` | eventos JSON `{"t": ...}`: crear, serializar, validar, getters seguros |
| `PlayerState.h/.cpp` | stream binario de la pose (una lista de campos para codificar y decodificar) |
| `EponaState.h/.cpp` | stream binario de Eponas independientes: propietario, secuencia, estado, posición y montaje |
| `ByteStream.h` | `Writer`/`Reader` little-endian |
| `Text.h/.cpp` | reglas de nick, saneado de chat, troceado de comandos con comillas |
| `I18n.h/.cpp` + `I18nMessages.inc` | idiomas (es/en/zh/ru): `Tr(Msg::X, {args})`; todos los textos del servidor, una línea por texto |
| `Transport.h/.cpp` | ENet: canal 0 fiable (JSON), canal 1 no fiable (poses) |
| `Hex.*` | bytes ↔ hex |
| `WorldFields.*` | esquema del mundo: nombre, tamaño y tipo de cada campo |
| `WorldOps.*` | cambios del mundo: diferencias, aplicar validado, JSON; servidor y juego |
| `Clock.*` | reloj de 3 días: abs ↔ día/hora, formato, regla de seguimiento del juego |
| `WorldRules.*` | reglas del juego original que el mundo aplica fuera de él (espadas que robó Takkuri) |
| `ActorImage.*` | D3: stream binario de la memoria de los actores replicados de una sala: registros, partes, límites |
| `SlotCodec.h` | una ranura de `ActorImage` en el cable (tipo + valor), para otros streams |
| `EffectImage.*` | eco de efectos: stream binario `kStreamEffects` (partículas con sus datos de inicio como ranuras) |

### `coop/server/`
| Archivo | Responsabilidad |
|---|---|
| `main.cpp` | arranque, bucle, consola (Ctrl+C / cerrar ventana = parada ordenada) |
| `Server.h/.cpp` | núcleo: recibe paquetes y los reparte a los handlers; enviar/broadcast/kick/stop |
| `Registry.h/.cpp` | tablas de handlers + macros `COOP_SERVER_EVENT/STREAM/ON_DISCONNECT/ON_TICK` |
| `CommandRegistry.h/.cpp` | comandos `/...` + macro `COOP_COMMAND` + permisos (`Perm::Player/Op/Console`) |
| `PlayerRegistry.*` | clientes conectados (`RemoteClient`: nick, ip, escena, posición...) |
| `AccessLists.*` | `bans.json` / `ops.json` (un admin = nick + la IP que tenía al darle `op`) |
| `ServerConfig.*` | `server.json` (se crea con valores por defecto) |
| `GiftManager.*` | regalos de rupias en curso (por conexión; un pago tardío se reembolsa) |
| `Logger.*` | log a consola + `logs/server.log` |
| `Handlers/SessionHandlers.cpp` | `hello` → `welcome`/`reject`, `leave` |
| `Handlers/ChatHandlers.cpp` | `chat`, `cmd` |
| `Handlers/StateHandlers.cpp` | reenvío de poses solo a la misma escena (con límite de ritmo y poses imposibles descartadas), `loc` |
| `Handlers/GiftHandlers.cpp` | `gift_paid`, `gift_recv`, reembolsos, caducidad |
| `Commands/*.cpp` | un archivo por comando o grupo: help, list, pm, tp, gift, kick/ban/unban/banlist, op/deop, say/stop, stats |
| `World/SharedWorld.h` + `SharedWorld.cpp` | entrar, crear, salir, reenvío de cambios, inventarios, guardado, cuándo corre el reloj |
| `World/SharedWorldCycle.cpp` | Doble Tiempo, Invertida, votación, reinicios, luna, /settime |
| `World/WorldStore.*` | los campos del mundo, la copia del inicio del ciclo y `world.json` |
| `World/WorldClock.*` | el reloj del servidor (velocidad, parado, saltos) |
| `World/PlayerStore.*` | inventario propio de cada jugador (`players/<nick>.json`) |
| `World/SotVote.*` | votación de la Canción del Tiempo |
| `World/JsonFile.*` | leer/escribir JSON en disco sin dejar archivos a medias |
| `Handlers/WorldHandlers.cpp` | `world_enter/world_leave/world_init/wops/inv/cycle_result` |
| `Handlers/ClockHandlers.cpp` | `clock_jump/clock_speed/sot_propose` |
| `Commands/WorldCommands.cpp` | `/tiempo /si /no /settime /mundo /reiniciar` |
| `World/RoomAuthority.*` | C: quién simula los enemigos de cada (escena, sala): el que lleva más tiempo y no está ocupado |
| `Handlers/ActorHandlers.cpp` | C: `auth` y reenvío del stream de enemigos, `hit`, `hurt`, `drop` (con sus reglas) |
| `Handlers/PropHandlers.cpp` | objetos del escenario (`prop`, `props`, `item`: lo roto se recuerda mientras alguien siga en la escena) |
| `Handlers/EndingHandlers.cpp` | el final: etapas (`ending`), cuenta atrás de la torre (`rooftop`), sincronía (`ending_sync`) y ciclo nuevo al acabar (`ending_done`) |
| `GroupBook.*` | reglas de grupos e invitaciones, sin red (probado en `TestGroups.cpp`) |
| `Groups.*` | quién oye qué (compañeros, en la escena) y los eventos/textos de los cambios de grupo |
| `Handlers/GroupHandlers.cpp` | caducidad de invitaciones; salir del mundo o desconectarse = salir del grupo al momento |
| `Commands/GroupCommands.cpp` | `/invitar /aceptar /rechazar /grupo /dejargrupo` (y sus nombres en inglés); `tp` al invitador al aceptar |
| `Handlers/ActivityHandlers.cpp` | `act` (también `result` y el resultado de una ronda), `act_hud`, `act_reward`, `follow`, `talk` (con `vars`), `cinema` (con `blur`), `title`: validados y al grupo en la escena o a toda la escena (`scope`) |
| `Handlers/EffectHandlers.cpp` | eco de efectos: valida el paquete, sella al remitente y lo reenvía a su escena (`effects` en `server.json`) |

### `mm/2s2h/Coop/` (juego)
| Archivo | Responsabilidad |
|---|---|
| `CoopInit.cpp` | orden del trabajo de cada frame: red → mundo → reloj; al final, cambios del mundo; autoconexión |
| `Client/NetClient.*` | hilo de red + colas (solo ese hilo toca ENet) |
| `Client/Dispatcher.*` | evento → handlers; macros `COOP_ON_EVENT/ON_STREAM/ON_LOST` |
| `Client/Session.*` | quién soy, quién está conectado y dónde |
| `Chat/ChatModel.*` | historial del chat y envío (`/...` = comando del servidor) |
| `Chat/ChatWindow.*` | overlay ImGui abajo a la izquierda (Enter abre, Esc cierra, ↑/↓ historial) |
| `Puppet/PoseCapture.*` | envía la pose del Link local cada frame (whitelist de flags de estado) |
| `Puppet/PuppetManager.*` | un actor marioneta por jugador remoto en mi escena; búfer anti-jitter |
| `Puppet/PuppetActor.*` | actor `En_CoopPuppet`: estructura `Player` completa dibujada con `Player_Draw` |
| `Features/Location.cpp` | envía `loc` al cambiar de escena/sala o al entrar/salir de una cinemática |
| `Features/Ending.h` | **mapa del final del juego** (spec: `docs/superpowers/specs/2026-09-30-coop-final-del-juego-design.md`) |
| `Features/Ending.cpp` | etapas: azotea (1), Canción del Juramento → guarida de Majora (2), Majora derrotada (3), final (4); si uno llega, los demás van |
| `Features/RooftopTimer.cpp` | la cuenta atrás de la azotea es la del servidor (igual para todos; al acabar cae la luna) |
| `Features/EndingMode.cpp` | el final se ve entero y sincronizado; el co-op se aparta; al acabar, ciclo nuevo |
| `Group/Group.h` | **mapa de los grupos** en el juego; `GroupState.cpp` (copia del grupo e invitaciones, comandos), `InviteWindow.cpp` (ventana Aceptar/Rechazar), `GroupMenu.cpp` (sección del menú y opciones) |
| `Activities/Activities.h` | **mapa de minijuegos y misiones**; `ActivityTable.cpp` (**una línea por minijuego/misión**), `Director.cpp` (este juego corre uno), `Guest.cpp` (un compañero corre uno aquí), `Follow.cpp` (`follow`), `Rewards.cpp` (premios) |
| `Features/Cinema.h/.cpp` | cinemáticas vistas con la cámara (y la música) de quien las dirige: grupo o escena (jefes); rótulos de jefe |
| `Features/BossArenas.cpp` | salas de jefe: quien simula al jefe dirige sus cinemáticas para todos y no cede sus salas |
| `Features/TalkSync.h/.cpp` | diálogos espejo: el texto de un compañero en tu propia caja, al ritmo de quien habla |
| `Features/MessageVars.*` | los valores de quien habla (nombre, puntos, tiempos, rupias, récords, códigos) en el texto espejo: viajan con `talk` y se ponen solo mientras se decodifica (**lista `Vars`**) |
| `Features/EffectEcho.cpp` | eco de efectos: las partículas (`EffectSs`) de nuestro Link, de lo que simulamos, de nuestros golpes y de nuestra cinemática se crean en los demás juegos (**tabla `kInits`**: una línea por tipo de efecto) |
| `Features/CoopEponaManager.*` | Eponas por jugador (dueño + copias); el pasajero sigue al conductor al cambiar de escena y vuelve a subir |
| `Actors/CoopEngine.h` | C: la API en C que llaman los cambios `[COOP]` del motor |
| `Actors/ReplicationRules.*` | D3: **qué actores se replican** (categorías ENEMY/BOSS/NPC + listas: enemigos archivados como prop, generadores, locales, objetos de minijuego) y los actores de cinemática (modo Cinema: nuestros, salvo mientras miramos la cinemática de otro) |
| `Actors/ActorMemory.*`, `ProcessMemory.*`, `Leases.*` | D3: copia de la memoria por ranuras (máscara local por actor: la colisión dinámica es de cada juego); NPCs/enemigos prestados al jugador cercano y objetos del minijuego prestados a su director |
| `Actors/PropSync.*` | jarrones, hierba (también la de campo y los grupos), cajas, rocas y rupias: roto/recogido para todos; lo que sueltan lo ven todos (**lista `kSharedProps`**) |
| `Actors/TimeNeverStops.cpp` | en el mundo del servidor nada detiene el tiempo (pausa, ocarina, textos, máscaras): ganchos en z_actor/z_play/z_kankyo |
| `Actors/ActorRegistry.*` | C: los enemigos compartidos de la escena, su clave, esqueleto y colisiones |
| `Actors/Authority.*` | C: la tabla `auth` en el juego |
| `Actors/ActorSync.*` | C: la autoridad transmite; los demás aplican réplicas; objetivo = el Link más cercano |
| `Actors/HitSync.*` | C: golpes a réplicas (`hit`) y de enemigos a otros jugadores (`hurt`), inyectados en las colisiones |
| `Actors/DropSync.cpp` | C: cada juego tira sus propios objetos cuando muere un enemigo compartido |
| `Actors/LiveFlags.*` | C: borra al momento los objetos únicos que recogió otro (fichas, hadas, piezas de corazón) |
| `Features/Teleport.cpp` | aplica `tp`: mover en la misma sala o viajar con el sistema de reaparición |
| `Features/Warp.*` | warp por reaparición: /tp y la entrada al mundo |
| `Features/Gift.cpp` | pagar / recibir / reembolsar rupias |
| `Features/SelfNameTag.cpp` | nick propio opcional |
| `Features/DebugBoot.cpp` | prueba rápida: arranca en Ciudad Reloj con la partida de depuración; prueba de campos (`Debug.FieldSelfTest`) |
| `World/FieldTable.*` | campo ↔ memoria del guardado (`static_assert`) |
| `World/WorldSync.*` | sombra + diferencias → `wops`; aplicar los de otros |
| `World/SaveBuilder.*` | construir el guardado en memoria, reglas de fin de ciclo en una copia |
| `World/WorldSession.*` | entrar/salir, reinicios, CVars forzadas, subida del inventario, cálculo del ciclo |
| `World/ClockSync.*` | reloj del servidor, canciones, luna |
| `Menu/CoopMenu.*` | pestaña "Co-op" del menú (F1) |

### Cambios fuera de estas carpetas (buscar `[COOP]`)
- `mm/include/tables/actor_table.h`: actor `En_CoopPuppet` (0x2B2).
- `CMakeLists.txt`: `add_subdirectory(coop)`. `mm/CMakeLists.txt`: enlaza `coop_common`.
- `mm/src/code/z_sram_NES.c`, `mm/src/overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope_NES.c`: sin archivo
  (`fileNum` 0xFF) no se lee ni escribe la memoria de guardado.
- `mm/src/code/z_parameter.c` + `GameInteractor_VanillaBehavior.h`: `VB_START_MOON_CRASH` (la luna solo cae cuando lo
  dice el servidor).
- `mm/src/code/z_message.c` + `GameInteractor_VanillaBehavior.h`: `VB_SONG_OF_DOUBLE_TIME_SET_TIME` (el "sí" de la
  Canción de Doble Tiempo no cambia la hora local: la mueve el servidor para todos).
- `mm/src/code/z_actor.c`, `z_collision_check.c`, `z_skelanime.c`, `z_en_item00.c`: ganchos de réplica (C/D3).
- `mm/src/code/z_actor.c`, `z_play.c`, `z_kankyo.c`: el tiempo no se detiene en el mundo (`TimeNeverStops.cpp`).
- `mm/src/code/z_demo.c`: el final espera a los jugadores rezagados (`Coop_CutsceneHold`) y avisa del último plano
  (`Coop_OnFinale`). `mm/2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipStoppingMoonCutscene.cpp`: en el mundo la
  Canción del Juramento la lleva `Features/Ending.cpp`.
- `mm/src/overlays/actors/ovl_En_Horse/z_en_horse.c`: copias de Epona de otros jugadores.
- `mm/src/overlays/actors/ovl_player_actor/z_player.c` (`func_808482E0`: `Coop_OnGetItem/End`, premios del grupo),
  `mm/src/code/z_parameter.c` (`Rupees_ChangeBy`: `Coop_OnRupeesChanged`), `mm/src/code/z_actor.c`
  (`TitleCard_InitBossName`: `Coop_OnBossTitleCard`; `Player_SetCsAction*`: `Coop_CsActionTarget`).
- Límites de los grupos: `mm/src/code/z_effect_soft_sprite.c` (`EffectSs_Spawn`: `Coop_OnEffectSpawn`),
  `mm/src/code/z_play.c` (`Coop_CollisionPass` alrededor de las colisiones del fotograma), `mm/src/code/z_message.c`
  (`Message_Decode`: `Coop_OnMessageDecode` al empezar y al acabar), y `Coop_AddActorRegion` en el `Init` de
  `ovl_Boss_01/02/03/07`, `ovl_En_Knight` (sus tablas de efectos) y `ovl_Obj_Takaraya_Wall` (el laberinto).

`server.json`: `language` (`es`, `en`, `zh`, `ru`; ver «Idiomas»), `sharedEnemies`, `sharedProps` (objetos del
escenario), `endingForAll` (Luna/Majora/final para todos), `groups` (grupos), `bossCutscenes` (cinemáticas de jefes
para toda la escena) y `effects` (eco de efectos) se pueden poner a `false`; `inviteSeconds` (10..600, 60 por defecto)
es lo que dura una invitación.

## Recetas (lo que más se modifica)

**Comando nuevo** (sin recompilar el juego): crea `coop/server/Commands/MiComando.cpp`:
```cpp
#include "server/CommandRegistry.h"
#include "server/Server.h"
namespace coop::server {
static void Run(CommandContext& ctx, const std::vector<std::string>& args) {
    ctx.Reply(Tr(Msg::HolaReply, { ctx.SenderName() }), level::kOk);
}
COOP_COMMAND(hola, "hola", Msg::HolaUsage, Msg::HolaHelp, Perm::Player, 0, Run);
}
```
y las tres líneas de texto (`HolaUsage`, `HolaHelp`, `HolaReply`) en `common/I18nMessages.inc`.
Recompila solo el servidor. `/help` lo mostrará solo. Otro nombre para el mismo comando: `COOP_ALIAS(id, "otro", "hola");`.

**Texto nuevo** (log, respuesta, motivo de rechazo...): una línea `X(MiTexto, "es", "en", "zh", "ru")` en
`common/I18nMessages.inc` (`{0}`, `{1}`... son los argumentos) y `Tr(Msg::MiTexto, { a, b })` donde se use.
El compilador rechaza una línea a la que le falte un idioma, y el test `EveryTextExistsInEveryLanguage...` comprueba
que los cuatro usan los mismos `{n}`.

**Evento nuevo**: nombre en `Protocol.h` (`ev::kMiEvento`), handler en el servidor con
`COOP_SERVER_EVENT(id, ev::kMiEvento, true, Fn)` y en el juego con `COOP_ON_EVENT(id, coop::ev::kMiEvento, Fn)`.
Añade un test en `coop/tests/`.

**Campo nuevo en la pose**: añádelo a `PlayerState` y **al final** de `VisitFields` en `PlayerState.cpp`, súbelo en
`PoseCapture.cpp` y aplícalo en `PuppetActor.cpp::ApplyState`. Sube `kProtocolVersion`.

**Campo nuevo del mundo**: añádelo **al final** de `kFields` (`coop/common/WorldFields.h`) y de `kAccess`
(`FieldTable.cpp`: memoria directa con `SAVE_BYTES` o funciones de lectura/escritura), sube `kProtocolVersion`; los
`static_assert` avisan si no coinciden.

**Dato nuevo del jugador**: una línea al final de `kPlayerFields` (`FieldTable.cpp`); el servidor lo guarda sin mirarlo.

**Funcionalidad nueva en el juego**: un archivo en `mm/2s2h/Coop/Features/` que se registra solo con
`static RegisterShipInitFunc init(MiFuncion);` (patrón de las Enhancements de 2Ship). Si añades archivos, vuelve a
ejecutar la configuración de CMake del juego (los fuentes se recogen con glob).

## Idiomas (es / en / zh / ru)

El servidor habla **un** idioma por proceso: logs, `/help`, respuestas de comandos, avisos del mundo, motivos de
rechazo y expulsión. Se elige de tres formas (la última gana):
1. `server.json` → `"language": "en"` (`es` por defecto; también vale `zh`, `ru`).
2. `2ship-coop-server.exe --lang zh` al arrancar.
3. En marcha: `/lang en` (admins y consola), sin argumento muestra el actual. Se guarda en `server.json`.

Los nombres de los comandos no cambian (se escriben en ASCII), salvo que los de mundo tienen también su nombre en inglés:
`/tiempo`=`/clock`, `/si`=`/yes`, `/mundo`=`/world`, `/reiniciar`=`/restart`. `motd` vacío = bienvenida por defecto en
el idioma del servidor; uno escrito a mano se envía tal cual. Los textos del juego (menú, ventana de chat) siguen en
español. Añadir un idioma: `Lang` + `LangCode/LangName` en `I18n.cpp` y una columna más en cada `X(...)` del `.inc`.

## Compilar

```powershell
# Juego + servidor + bot + tests (Windows, desde la raíz del repo)
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -T v143 -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build/x64 --config Release --parallel
# Solo servidor/bot/tests (rápido, también en Linux)
cmake -S coop -B build/coop -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build/coop --config Release --parallel
build/coop/Release/coop-tests.exe          # 238 tests
```

## Probar sin amigos

```powershell
2ship-coop-server.exe --port 7780
2ship-coop-bot.exe --target TuNick --mode mirror        # te imita a tu lado (también: circle, front)
2ship-coop-bot.exe --nick Regalador --cmd "/gift TuNick 20"
```
En Git Bash, los argumentos que empiezan por `/` se convierten en rutas: usa `MSYS_NO_PATHCONV=1`.

## Mundo compartido (B)

- Archivos del servidor: `world.json` (campos en hex, reloj, ciclo, copia del inicio del ciclo; si está dañado se
  aparta como `world.json.bad`) y `players/<nick>.json` (inventario propio, posición; un nick que es un dispositivo
  de Windows, como `con` o `nul`, usa `con-.json`; al crear un mundo nuevo se renombran a `.old`). Se guardan cada
  10 s y al parar.
- Comandos: `/tiempo`, `/si`, `/no` (todos); `/settime <día 1-3> <hh:mm>`, `/mundo`, `/reiniciar` (admins y consola).
- Eventos: `world_enter/world_leave/world_full/world_init/wops/inv/clock/clock_jump/clock_speed/sot_propose/`
  `cycle_compute/cycle_result` (campos en `Protocol.h`). Los `wops` que cambian el mundo vuelven a todos, también
  a quien los envió (bits y bytes; los contadores se corrigen aparte): así todos aplican los cambios de un byte en el
  orden del servidor y acaban con el mismo valor.
- En el juego: `fileNum = 0xFF` (jugar en el servidor **nunca** escribe en los archivos de guardado del jugador); CVars
  forzadas mientras se juega en el servidor (la lista de `kForcedCVars` en `WorldSession.cpp`). Sus valores
  anteriores se guardan en `gCoop.World.RestoreCVars` y vuelven al salir, o al arrancar si el juego se cerró dentro.
- Límites: `wops` ≤ 30/s, `inv` ≤ 2/s, ≤ 6 KB y ≤ 8 niveles de anidación, `world_enter`/`world_leave` ≤ 1 cada
  2 s (ráfaga 6); protocolo v2.

## Grupos, minijuegos y misiones

Specs: `docs/superpowers/specs/2026-09-30-coop-grupos-actividades-design.md` y
`docs/superpowers/specs/2026-09-30-coop-grupos-limites-design.md` (el arreglo de sus límites; lo que queda: su §13).
Estar en un grupo es aceptar compartir con los compañeros cercanos: minijuegos, premios, diálogos y cinemáticas. Sin
grupo nada cambia, salvo lo que es de toda la escena: las cinemáticas, rótulos y diálogos de los jefes y el eco de
efectos.

- **Comandos** (también en el menú Co-op → "Grupo e invitaciones" y en la ventana de invitación): `/invitar <nick>...`
  o `/invitar todos` (`/invite`, `all`), `/aceptar [nick]` (`/accept`: viajas junto a quien invitó), `/rechazar [nick]`
  (`/decline`; sin nick, todas), `/grupo` (`/group`), `/dejargrupo` (`/leavegroup`). Máximo 4 por grupo; la invitación
  dura `inviteSeconds`. `/list` muestra "(en *actividad*)".
- **Minijuegos**: el juego que lo corre (el *director*) se queda su NPC, manda su marcador (`act_hud`, 10 por segundo)
  y lleva al grupo a sus entradas especiales (`follow`). Modos:
  - **Juntos**: los invitados juegan la misma partida con el mismo marcador. Cuenta lo que hacen: sus golpes a los
    blancos replicados (`hit`) y sus **contactos** (tocar un blanco, meter una bomba en una cesta: `hit` con
    `oc: true`, que el director reproduce con su Link o con un explosivo igual; `HitSync.cpp`).
  - **Cada uno**: cada juego corre su propia carrera con los mismos rivales; nadie saca a nadie de su carrera y todos
    ven el **resultado** de cada uno (`act result`: "Ana gana la carrera goron (01:23.45)"). Los ganchos de cada
    carrera (cómo arranca el invitado, cómo acabó) son `ActivityHooks` en `ActivityTable.cpp`.
  - **Por turnos** (los de un jugador por diseño): uno juega y los demás ven su tiempo y sus puntos en una ventanita;
    al acabar (`act end` con `score` y `cs`) el chat dice su resultado y al siguiente del grupo que esté en la escena
    y no haya jugado en 10 minutos le sale "Te toca".
- **Objetos del minijuego prestados al director** (`props` en `ActivityTable.cpp`): actores de la sala que el minijuego
  usa (la plataforma de Honey y Darling, los aros de los castores, las plataformas y rupias del patio Deku, las
  antorchas del pescador). Se replican siempre y, mientras el director corre el minijuego (o acaba de hablar con su
  NPC), su juego los pide prestados todos (a cualquier distancia) y los simula: `ours: true` actúa con el Link del
  director (maquinaria), `false` con el Link más cercano (blancos). El servidor permite 48 préstamos por jugador.
  **No pongas como `prop`** un objeto cuyo estado es de cada jugador o cuyo código mueve al jugador local (la barca del
  pantano: su crucero empieza al cargar la escena y lleva la cámara y el viaje de vuelta), ni uno de un minijuego que
  el juego no sabe que corre (las puertas del mayordomo: sin reloj ni marcador nada los mantendría prestados), ni uno
  que solo existe en una capa de escena del que juega (los globos de Romani).
- **Premios** (`Rewards.cpp`): lo de cada jugador (rupias, munición, corazones, contenido de botellas) se copia a los
  compañeros que estaban allí; lo compartido por el mundo (máscaras, piezas de corazón, mejoras) ya lo da B; las
  compras nunca se copian.
- **Diálogos** (`TalkSync.cpp`) y **cinemáticas** (`Cinema.cpp`): se ven en el juego de los compañeros cercanos (o de
  toda la escena con los jefes y minijefes) al ritmo y con la cámara de quien los vive. El texto espejo muestra los
  valores de **quien habla** (`MessageVars.cpp`); la cámara lleva el temblor y el desenfoque de movimiento (`blur`).
- **Actores de cinemática** (`Dm_*`, `Demo_*`: modo Cinema en `ReplicationRules.cpp`): son de cada juego, pero mientras
  miras la cinemática de otro los mueve su juego (manda su memoria solo mientras su cámara se comparte; el servidor
  reenvía los actores de quien manda `cinema` aunque no sea dueño de la sala) y se crean los que hizo en marcha.
- **Eco de efectos** (`EffectEcho.cpp`, stream `kStreamEffects`): las partículas (`EffectSs`) que crean nuestro Link,
  los actores que simulamos, nuestros golpes o la cinemática que dirigimos se crean también en los demás juegos (los
  de cinemática, solo en quien la mira; el resto, a menos de 3000 unidades). Las tablas de efectos de los jefes y el
  laberinto de los cofres viajan con su actor: `Coop_AddActorRegion(actor, tabla, bytes)` desde su `Init`.
- **Protocolo v12**: `act` gana `result` (`won`, `cs`) y el resultado de `end` (`score`, `cs`); `talk` gana `vars`;
  `cinema` gana `blur`; `hit` gana `oc`; stream `kStreamEffects`. (v11: `group`, `invite`, `invite_end`, `act`,
  `act_hud`, `act_reward`, `follow`, `talk`, `title`; `cinema` con `scope` y `roll`.) Límites en `common/Protocol.h`.

**Receta: minijuego nuevo = una línea en `Activities/ActivityTable.cpp`** (clave ASCII, nombre, escena, NPC, modo,
entradas especiales, equipo, sitios, objetos prestados, ganchos). Misión nueva: una línea `K::Quest` con sus NPCs (da
nombre a la invitación). **Efecto nuevo que debe verse en los demás**: una línea en `kInits` (`EffectEcho.cpp`).
**Tabla de un actor fuera de su instancia**: `Coop_AddActorRegion` en su `Init`.

| Clave | Minijuego | Modo | Objetos prestados | Notas |
|---|---|---|---|---|
| `galeria_ciudad`, `galeria_pantano` | galerías de tiro | Juntos | — | arco en B; blancos replicados |
| `honey_darling` | Honey y Darling | Juntos | `BG_FU_KAITEN` (plataforma) | flechas, bombchus y bombas de todos cuentan; la plataforma gira para todos |
| `casa_espiritus` | casa de los espíritus | Juntos | — | entrada `GHOST_HUT 1` |
| `carrera_castores` | carrera de los castores | Juntos | `EN_TWIG` (aros) | los aros miran el Link más cercano |
| `espadachin` | escuela de espadachín | Juntos | — | los troncos son hijos del NPC |
| `patio_deku` | patio de los Deku | Juntos | `EN_GAMELUPY`, `OBJ_LUPYGAMELIFT` | las rupias se cogen por contacto |
| `carrera_goron`, `carrera_gorman` | carreras | Cada uno | — | entradas especiales; resultados compartidos; el NPC solo queda fijo al hablar |
| `cartero`, `cofres`, `carrera_perros`, `mayordomo_deku` | de un jugador | Por turnos | — | "Te toca" al siguiente |
| `saltos_pescador` | saltos del pescador | Por turnos | `OBJ_JGAME_LIGHT` (antorchas) | |
| `globos_romani` | tiro con Romani | Por turnos | — | se juega en una capa de escena propia (los demás no están en ella) |
| `barca_koume` | tiro en barca de Koume | Por turnos | — | la barca es de cada juego (ver arriba) |

## CVars del cliente (`2ship2harkinian.json` → `CVars.gCoop`)
`Nick`, `Host`, `Port`, `Password`, `AutoConnect`, `AutoEnter` (entrar en el mundo del servidor al conectar),
`ShowOwnNameTag`, `Chat.Scale`, `Chat.Opacity`, `Debug.BootToClockTown`, `Debug.BootEntrance` (entrada; 55296 =
Ciudad Reloj Sur, 54784 = Norte), `Debug.GrantHeartOnEnter` (prueba: +1 contenedor al entrar, una vez por ejecución),
`Debug.FieldSelfTest` (prueba: leer y reescribir todos los campos no debe cambiar nada). Grupo (1 = sí, por defecto):
`Group.Dialogues`, `Group.Cutscenes`, `Group.Minigames`, `Group.Rewards`, `Group.CinemaActors` (seguir los actores de
cinemática de quien miras), `Group.Turns` (aviso "Te toca") y `Effects` (eco de efectos: mandar y recibir).

## Seguridad del servidor
- Los paquetes malformados o desconocidos se registran (limpios, solo los 3 primeros) y a los 50 se expulsa al cliente.
- Poses: ≤30/s por jugador (ráfaga 40); `loc`: ≤5/s (ráfaga 10; el último cambio siempre llega). ENet no reensambla
  paquetes de más de 16 KB. Límites en `common/Protocol.h`.
- Las poses con valores que el juego no puede dibujar se descartan en el servidor y otra vez en el cliente
  (`SanitizePlayerState`, límites comprobados contra el motor con `static_assert` en `PoseCapture.cpp`).

## Diagnóstico
- Juego: líneas `[Coop]` en `logs/2 Ship 2 Harkinian.log` (envío de pose, marionetas creadas/eliminadas y por qué).
- Servidor: `logs/server.log` y el comando `stats` (poses recibidas/reenviadas por jugador).
