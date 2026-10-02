// The events a mod can listen to: X(Id, "name", cancelable, "what happened", "field:type[*] what it is|...").
// Types: int, number, string, bool, list, table. A handler may change the fields marked with * (and only those).
// The texts are the Spanish reference: coop/docs/mods/API.md is generated from this table (ModDocs.cpp), so no
// description may contain the character |.
X(ServerStart, "server_start", false, "El servidor ha arrancado y los mods configurados están cargados.", "")
X(ServerStop, "server_stop", false, "El servidor va a detenerse.", "reason:string Motivo de la parada")
X(PlayerConnect, "player_connect", true,
  "Un jugador ha pasado las comprobaciones del servidor (versión, nick, baneos, contraseña) y va a entrar. "
  "Cancelarlo lo rechaza con `reason`.",
  "nick:string Nick con el que entra|ip:string Su dirección IP|reason:string* Motivo que verá si se cancela")
X(PlayerJoin, "player_join", false, "Un jugador ha entrado en el servidor.",
  "player:int Id del jugador|nick:string Su nick|ip:string Su dirección IP")
X(PlayerLeave, "player_leave", false, "Un jugador se ha desconectado. Su id todavía vale dentro del manejador.",
  "player:int Id del jugador|nick:string Su nick|reason:string Motivo (desconectado, expulsado...)")
X(PlayerScene, "player_scene", false, "Un jugador ha cambiado de escena o de sala.",
  "player:int Id del jugador|nick:string Su nick|scene:int Id de la escena|sceneKey:string Clave de la escena "
  "(`SOUTH_CLOCK_TOWN`...; vacía si no se conoce)|sceneName:string Nombre que muestra el juego|room:int Sala|"
  "entrance:int Entrada por la que llegó|prevScene:int Escena anterior (-1: ninguna)|prevRoom:int Sala anterior")
X(PlayerStats, "player_stats", false,
  "La vida, la magia o las rupias de un jugador han cambiado (como mucho 4 avisos por segundo y jugador).",
  "player:int Id del jugador|nick:string Su nick|health:int Vida (16 = un corazón)|maxHealth:int Vida máxima|"
  "magic:int Magia (48 = barra normal, 96 = doble)|rupees:int Rupias|prevHealth:int Vida anterior|"
  "prevMagic:int Magia anterior|prevRupees:int Rupias anteriores")
X(PlayerDeath, "player_death", false, "Un jugador se ha quedado sin vida (aunque un hada lo reviva después).",
  "player:int Id del jugador|nick:string Su nick|scene:int Escena en la que estaba")
X(PlayerItem, "player_item", false,
  "El juego ha dado un objeto a un jugador (también munición, corazones y rupias recogidas).",
  "player:int Id del jugador|nick:string Su nick|item:int Id del objeto|itemKey:string Su nombre "
  "(`MASK_BUNNY`...; vacío si no se conoce)")
X(EnemyKilled, "enemy_killed", false,
  "Un enemigo ha recibido el golpe final. `player` es quien lo dio (o el juego que lo simulaba si no se sabe).",
  "player:int Id del jugador|nick:string Su nick|actor:int Id del actor|actorKey:string Su nombre "
  "(`EN_DODONGO`...)|params:int Parámetros del actor|scene:int Escena|room:int Sala|x:number Posición|"
  "y:number Posición|z:number Posición")
X(BossDefeated, "boss_defeated", false, "Un jefe ha sido derrotado en el juego de ese jugador.",
  "player:int Id del jugador|nick:string Su nick|actor:int Id del actor del jefe|actorKey:string Su nombre|"
  "scene:int Escena")
X(Chat, "chat", true, "Un jugador escribe en el chat. Cancelarlo impide que el mensaje se envíe.",
  "player:int Id del jugador|nick:string Su nick|text:string* El mensaje")
X(Command, "command", true,
  "Alguien va a ejecutar un comando (ya con permiso y argumentos suficientes). Cancelarlo impide que se ejecute.",
  "player:int Id del jugador (0: la consola)|nick:string Su nick|name:string Comando, sin la barra|"
  "args:list Argumentos|line:string La línea completa")
X(WorldEnter, "world_enter", false, "Un jugador ha entrado en la partida del servidor.",
  "player:int Id del jugador|nick:string Su nick")
X(WorldLeave, "world_leave", false, "Un jugador ha salido de la partida del servidor.",
  "player:int Id del jugador|nick:string Su nick")
X(WorldCreated, "world_created", false, "Se ha creado el mundo del servidor (lo creó el juego de ese jugador).",
  "player:int Id del jugador|nick:string Su nick")
X(WorldChange, "world_change", false,
  "El mundo compartido ha cambiado (ya aplicado). Las listas son las de `wops`: bits `[campo, byte, pone, quita]`, "
  "bytes `[campo, byte, valor]`, adds `[campo, byte, cuánto]`.",
  "player:int Id de quien lo cambió (0: el servidor o un mod)|bits:list Cambios de bits|bytes:list Cambios de bytes|"
  "adds:list Cambios de contadores")
X(WorldHour, "world_hour", false, "El reloj del mundo ha pasado a otra hora del juego.",
  "day:int Día (1-3)|hour:int Hora (0-23)|night:bool Es de noche (18:00-5:59)|abs:int Reloj absoluto (0 = día 1, "
  "6:00)|jump:bool El reloj saltó (canción, comando, ciclo nuevo) en vez de avanzar")
X(VoteStart, "vote_start", true,
  "Un jugador propone volver al Amanecer del Primer Día (Canción del Tiempo). Cancelarlo impide la votación.",
  "player:int Id del jugador|nick:string Su nick")
X(VoteEnd, "vote_end", false, "La votación de la Canción del Tiempo ha terminado.", "passed:bool Ha salido adelante")
X(CycleReset, "cycle_reset", false, "Ha empezado un ciclo nuevo de tres días.",
  "cycle:int Número del ciclo nuevo|reason:string `sot` (votación), `moon` (cayó la luna), `restart` (comando) o "
  "`ending` (final del juego)")
X(MoonCrash, "moon_crash", false, "El reloj ha llegado al final del tercer día: la luna cae.",
  "cycle:int Ciclo que termina")
X(GroupChange, "group_change", false, "Un grupo ha cambiado de miembros o se ha deshecho (sin miembros).",
  "group:int Id del grupo|leader:int Id del líder (0: deshecho)|members:list Ids de sus miembros")
X(Activity, "activity", false, "Un jugador empieza o termina un minijuego, o comunica su resultado.",
  "player:int Id del jugador|nick:string Su nick|state:string `start`, `end` o `result`|key:string Clave del "
  "minijuego|name:string Su nombre|score:int Puntos (al terminar)|cs:int Tiempo en centésimas|won:bool Ganó "
  "(solo en `result`)")
X(ModUnload, "mod_unload", false, "Este mod va a descargarse (solo lo recibe él).", "")
