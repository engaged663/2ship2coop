# Co-op mod — mapa del código

Mod cooperativo online para 2 Ship 2 Harkinian (hasta 4 jugadores por IP, servidor dedicado).
Diseño completo: `docs/superpowers/specs/2026-09-26-coop-a-nucleo-online-design.md`.
**Lee este archivo y luego solo el archivo que vayas a tocar.**

Estado: Sub-proyecto **A (núcleo online)** terminado. Pendientes: **B** (mundo compartido: reloj, flags, objetos
únicos, inventario en el servidor) y **C** (NPCs y enemigos en tiempo real, cambios en el motor).

## Piezas

| Pieza | Carpeta | Qué es |
|---|---|---|
| Librería común | `coop/common/` | protocolo + red (ENet), sin nada del juego. La usan servidor, bot, tests y juego |
| Servidor | `coop/server/` → `2ship-coop-server.exe` | consola; autoridad de jugadores, chat, comandos |
| Bot de pruebas | `coop/tools/CoopBot.cpp` → `2ship-coop-bot.exe` | jugador falso para probar con un solo PC |
| Tests | `coop/tests/` → `coop-tests.exe` | unitarios + integración (servidor real en loopback) |
| Cliente (juego) | `mm/2s2h/Coop/` | conexión, chat, otros Links, /tp, /gift, menú |

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

### `mm/2s2h/Coop/` (juego)
| Archivo | Responsabilidad |
|---|---|
| `CoopInit.cpp` | procesa la red al inicio de cada frame; autoconexión |
| `Client/NetClient.*` | hilo de red + colas (solo ese hilo toca ENet) |
| `Client/Dispatcher.*` | evento → handlers; macros `COOP_ON_EVENT/ON_STREAM/ON_LOST` |
| `Client/Session.*` | quién soy, quién está conectado y dónde |
| `Chat/ChatModel.*` | historial del chat y envío (`/...` = comando del servidor) |
| `Chat/ChatWindow.*` | overlay ImGui abajo a la izquierda (Enter abre, Esc cierra, ↑/↓ historial) |
| `Puppet/PoseCapture.*` | envía la pose del Link local cada frame (whitelist de flags de estado) |
| `Puppet/PuppetManager.*` | un actor marioneta por jugador remoto en mi escena; búfer anti-jitter |
| `Puppet/PuppetActor.*` | actor `En_CoopPuppet`: estructura `Player` completa dibujada con `Player_Draw` |
| `Features/Location.cpp` | envía `loc` al cambiar de escena/sala |
| `Features/Teleport.cpp` | aplica `tp`: mover en la misma sala o viajar con el sistema de reaparición |
| `Features/Gift.cpp` | pagar / recibir / reembolsar rupias |
| `Features/SelfNameTag.cpp` | nick propio opcional |
| `Features/DebugBoot.cpp` | prueba rápida: arranca en Ciudad Reloj con la partida de depuración |
| `Menu/CoopMenu.*` | pestaña "Co-op" del menú (F1) |

### Cambios fuera de estas carpetas (buscar `[COOP]`)
- `mm/include/tables/actor_table.h`: actor `En_CoopPuppet` (0x2B2).
- `CMakeLists.txt`: `add_subdirectory(coop)`. `mm/CMakeLists.txt`: enlaza `coop_common`.

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
build/coop/Release/coop-tests.exe          # 76 tests
```

## Probar sin amigos

```powershell
2ship-coop-server.exe --port 7780
2ship-coop-bot.exe --target TuNick --mode mirror        # te imita a tu lado (también: circle, front)
2ship-coop-bot.exe --nick Regalador --cmd "/gift TuNick 20"
```
En Git Bash, los argumentos que empiezan por `/` se convierten en rutas: usa `MSYS_NO_PATHCONV=1`.

## CVars del cliente (`2ship2harkinian.json` → `CVars.gCoop`)
`Nick`, `Host`, `Port`, `Password`, `AutoConnect`, `ShowOwnNameTag`, `Chat.Scale`, `Chat.Opacity`,
`Debug.BootToClockTown`, `Debug.BootEntrance` (entrada; 55296 = Ciudad Reloj Sur, 54784 = Norte).

## Seguridad del servidor
- Los paquetes malformados o desconocidos se registran (limpios, solo los 3 primeros) y a los 50 se expulsa al cliente.
- Poses: ≤30/s por jugador (ráfaga 40); `loc`: ≤5/s (ráfaga 10; el último cambio siempre llega). ENet no reensambla
  paquetes de más de 16 KB. Límites en `common/Protocol.h`.
- Las poses con valores que el juego no puede dibujar se descartan en el servidor y otra vez en el cliente
  (`SanitizePlayerState`, límites comprobados contra el motor con `static_assert` en `PoseCapture.cpp`).

## Diagnóstico
- Juego: líneas `[Coop]` en `logs/2 Ship 2 Harkinian.log` (envío de pose, marionetas creadas/eliminadas y por qué).
- Servidor: `logs/server.log` y el comando `stats` (poses recibidas/reenviadas por jugador).
