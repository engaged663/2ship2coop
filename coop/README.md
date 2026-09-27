# Co-op mod — mapa del código

Mod cooperativo online para 2 Ship 2 Harkinian (hasta 4 jugadores por IP, servidor dedicado).
Diseño completo: `docs/superpowers/specs/2026-09-26-coop-a-nucleo-online-design.md` (A) y
`docs/superpowers/specs/2026-09-26-coop-b-mundo-compartido-design.md` (B).
**Lee este archivo y luego solo el archivo que vayas a tocar.**

Estado: Sub-proyectos **A (núcleo online)** y **B (mundo compartido)** terminados. Pendiente: **C** (NPCs y enemigos
en tiempo real, cambios en el motor). Diseño de B: docs/superpowers/specs/2026-09-26-coop-b-mundo-compartido-design.md.

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
| `ByteStream.h` | `Writer`/`Reader` little-endian |
| `Text.h/.cpp` | reglas de nick, saneado de chat, troceado de comandos con comillas |
| `Transport.h/.cpp` | ENet: canal 0 fiable (JSON), canal 1 no fiable (poses) |
| `Hex.*` | bytes ↔ hex |
| `WorldFields.*` | esquema del mundo: nombre, tamaño y tipo de cada campo |
| `WorldOps.*` | cambios del mundo: diferencias, aplicar validado, JSON; servidor y juego |
| `Clock.*` | reloj de 3 días: abs ↔ día/hora, formato, regla de seguimiento del juego |
| `WorldRules.*` | reglas del juego original que el mundo aplica fuera de él (espadas que robó Takkuri) |
| `ActorState.*` | stream binario de los enemigos compartidos de una sala (C): registros, partes, límites |

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
| `Features/Location.cpp` | envía `loc` al cambiar de escena/sala o de estado ocupado (pausa, texto, cinemática) |
| `Actors/CoopEngine.h` | C: la API en C que llaman los cambios `[COOP]` del motor |
| `Actors/SharedActors.*` | C: **la lista de enemigos compartidos** (añadir uno = una línea) y su estado de dibujo |
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

## Recetas (lo que más se modifica)

**Comando nuevo** (sin recompilar el juego): crea `coop/server/Commands/MiComando.cpp`:
```cpp
#include "server/CommandRegistry.h"
#include "server/Server.h"
namespace coop::server {
static void Run(CommandContext& ctx, const std::vector<std::string>& args) {
    ctx.Reply("Hola " + ctx.SenderName(), level::kOk);
}
COOP_COMMAND(hola, "hola", "/hola", "saluda", Perm::Player, 0, Run);
}
```
Recompila solo el servidor. `/help` lo mostrará solo.

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

## Compilar

```powershell
# Juego + servidor + bot + tests (Windows, desde la raíz del repo)
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -T v143 -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build/x64 --config Release --parallel
# Solo servidor/bot/tests (rápido, también en Linux)
cmake -S coop -B build/coop -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build/coop --config Release --parallel
build/coop/Release/coop-tests.exe          # 121 tests
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

## CVars del cliente (`2ship2harkinian.json` → `CVars.gCoop`)
`Nick`, `Host`, `Port`, `Password`, `AutoConnect`, `AutoEnter` (entrar en el mundo del servidor al conectar),
`ShowOwnNameTag`, `Chat.Scale`, `Chat.Opacity`, `Debug.BootToClockTown`, `Debug.BootEntrance` (entrada; 55296 =
Ciudad Reloj Sur, 54784 = Norte), `Debug.GrantHeartOnEnter` (prueba: +1 contenedor al entrar, una vez por ejecución),
`Debug.FieldSelfTest` (prueba: leer y reescribir todos los campos no debe cambiar nada).

## Seguridad del servidor
- Los paquetes malformados o desconocidos se registran (limpios, solo los 3 primeros) y a los 50 se expulsa al cliente.
- Poses: ≤30/s por jugador (ráfaga 40); `loc`: ≤5/s (ráfaga 10; el último cambio siempre llega). ENet no reensambla
  paquetes de más de 16 KB. Límites en `common/Protocol.h`.
- Las poses con valores que el juego no puede dibujar se descartan en el servidor y otra vez en el cliente
  (`SanitizePlayerState`, límites comprobados contra el motor con `static_assert` en `PoseCapture.cpp`).

## Diagnóstico
- Juego: líneas `[Coop]` en `logs/2 Ship 2 Harkinian.log` (envío de pose, marionetas creadas/eliminadas y por qué).
- Servidor: `logs/server.log` y el comando `stats` (poses recibidas/reenviadas por jugador).
