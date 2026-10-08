// Every text the server says, in the four languages: X(Id, es, en, zh, ru).
// Placeholders: {0} {1} {2}... are replaced by the arguments of Tr(Msg::Id, { ... }).
// To add a text: add one line here and call Tr(Msg::Id, ...). To add a language: see I18n.h.
// Spanish is the default language (the tests and the old server.json files rely on it).

// ---- Generic / infrastructure ----
X(ConsoleName, "Servidor", "Server", "服务器", "Сервер")
X(PlayerNotFound, "No hay ningún jugador conectado llamado '{0}'.", "No player named '{0}' is connected.",
  "没有名为“{0}”的在线玩家。", "Игрока с ником «{0}» нет на сервере.")
X(UnknownCommand, "Comando desconocido: /{0}. Escribe /help para ver la lista.",
  "Unknown command: /{0}. Type /help to see the list.", "未知命令:/{0}。输入 /help 查看命令列表。",
  "Неизвестная команда: /{0}. Введите /help, чтобы увидеть список.")
X(NoPermission, "No tienes permiso para usar /{0}.", "You do not have permission to use /{0}.",
  "你没有权限使用 /{0}。", "У вас нет прав на использование /{0}.")
X(UsageLine, "Uso: {0}", "Usage: {0}", "用法:{0}", "Использование: {0}")
X(LogCommandUsed, "{0} usó: {1}", "{0} used: {1}", "{0} 使用了:{1}", "{0} использовал: {1}")
X(CommandTooLong, "Comando demasiado largo.", "Command too long.", "命令太长。", "Команда слишком длинная.")
X(RateLimited, "Estás enviando mensajes demasiado rápido. Espera un momento.",
  "You are sending messages too fast. Wait a moment.", "你发送消息太快了,请稍等。",
  "Вы отправляете сообщения слишком быстро. Подождите немного.")
X(ConsoleHint, "Escribe 'help' para ver los comandos de la consola.", "Type 'help' to see the console commands.",
  "输入 'help' 查看控制台命令。", "Введите 'help', чтобы увидеть команды консоли.")
X(BadPortArg, "--port inválido: {0}", "Invalid --port: {0}", "无效的 --port:{0}", "Неверное значение --port: {0}")
X(BadLangArg, "--lang inválido: {0} (usa es, en, zh o ru)", "Invalid --lang: {0} (use es, en, zh or ru)",
  "无效的 --lang:{0}(请使用 es、en、zh 或 ru)", "Неверное значение --lang: {0} (используйте es, en, zh или ru)")
X(BadLangConfig, "{0}: 'language' debe ser es, en, zh o ru; se usa el valor por defecto",
  "{0}: 'language' must be es, en, zh or ru; the default is used",
  "{0}:'language' 必须是 es、en、zh 或 ru;将使用默认值",
  "{0}: 'language' должен быть es, en, zh или ru; используется значение по умолчанию")
X(LangCurrent, "Idioma del servidor: {0}. Disponibles: es, en, zh, ru.",
  "Server language: {0}. Available: es, en, zh, ru.", "服务器语言:{0}。可用:es、en、zh、ru。",
  "Язык сервера: {0}. Доступны: es, en, zh, ru.")
X(LangChanged, "Idioma del servidor cambiado a {0}.", "Server language changed to {0}.", "服务器语言已更改为 {0}。",
  "Язык сервера изменён на {0}.")
X(LangUnknown, "Idioma desconocido: '{0}'. Usa es, en, zh o ru.", "Unknown language: '{0}'. Use es, en, zh or ru.",
  "未知语言:“{0}”。请使用 es、en、zh 或 ru。", "Неизвестный язык: «{0}». Используйте es, en, zh или ru.")
X(DefaultMotd, "Bienvenido al servidor co-op de Majora's Mask. Escribe /help para ver los comandos.",
  "Welcome to the Majora's Mask co-op server. Type /help to see the commands.",
  "欢迎来到 Majora's Mask 合作服务器。输入 /help 查看命令。",
  "Добро пожаловать на кооперативный сервер Majora's Mask. Введите /help, чтобы увидеть команды.")

// ---- Server core ----
X(ServerListening, "Servidor co-op escuchando en el puerto UDP {0} (máx. {1} jugadores, protocolo v{2})",
  "Co-op server listening on UDP port {0} (max {1} players, protocol v{2})",
  "合作服务器正在监听 UDP 端口 {0}(最多 {1} 名玩家,协议 v{2})",
  "Кооп-сервер слушает UDP-порт {0} (макс. игроков: {1}, протокол v{2})")
X(IncomingConnection, "Conexión entrante desde {0}", "Incoming connection from {0}", "来自 {0} 的连接", "Входящее подключение от {0}")
X(InvStreamBeforeHello, "stream antes del saludo", "stream before the handshake", "握手前收到数据流",
  "поток данных до рукопожатия")
X(InvStreamUnknown, "stream desconocido", "unknown stream", "未知的数据流", "неизвестный поток данных")
X(InvChannel, "paquete en un canal que no le toca", "packet on a channel it may not use", "数据包使用了不允许的通道",
  "пакет в недопустимом канале")
X(InvEvent, "evento inválido ({0})", "invalid event ({0})", "无效的事件({0})", "недопустимое событие ({0})")
X(InvEventUnknown, "evento desconocido '{0}'", "unknown event '{0}'", "未知事件“{0}”", "неизвестное событие «{0}»")
X(HandshakeTimeout, "{0} no completó el saludo a tiempo; desconectado", "{0} did not finish the handshake in time; disconnected",
  "{0} 未能及时完成握手;已断开", "{0} не завершил рукопожатие вовремя; отключён")
X(Rejected, "Rechazado {0}: {1}", "Rejected {0}: {1}", "已拒绝 {0}:{1}", "Отклонён {0}: {1}")
X(LeaveKicked, "expulsado", "kicked", "被踢出", "исключён")
X(LeaveKickedReason, "expulsado: {0}", "kicked: {0}", "被踢出:{0}", "исключён: {0}")
X(LeaveDisconnected, "desconectado", "disconnected", "已断开连接", "отключился")
X(InvalidFrom, "{0} de {1}", "{0} from {1}", "{1} 发送了{0}", "{0} от {1}")
X(TooManyInvalid, "demasiados paquetes inválidos", "too many invalid packets", "无效数据包过多", "слишком много недопустимых пакетов")
X(Stopping, "Deteniendo el servidor: {0}", "Stopping the server: {0}", "正在关闭服务器:{0}", "Остановка сервера: {0}")
X(StoppedNotice, "Servidor detenido: {0}", "Server stopped: {0}", "服务器已关闭:{0}", "Сервер остановлен: {0}")
X(Stopped, "Servidor detenido.", "Server stopped.", "服务器已关闭。", "Сервер остановлен.")
X(StopByAdmin, "servidor cerrado por el administrador", "server closed by the administrator", "服务器已被管理员关闭",
  "сервер закрыт администратором")
X(StopFromConsole, "cerrado desde la consola", "closed from the console", "已从控制台关闭", "закрыт из консоли")

// ---- Handshake ----
X(HostBuildLog, "Versión del juego del servidor: {0}", "Server game build: {0}", "服务器游戏版本:{0}", "Версия игры сервера: {0}")
X(RejectBuild,
  "Tu juego no es el mismo ejecutable que el del servidor. Usad todos el mismo 2ship.exe (la misma versión del mod).",
  "Your game is not the same executable as the server's. Everyone must use the same 2ship.exe (the same mod version).",
  "你的游戏与服务器使用的可执行文件不一致。所有人必须使用同一个 2ship.exe(相同版本的模组)。",
  "Ваша игра — не тот же исполняемый файл, что у сервера. Все должны использовать один и тот же 2ship.exe (одну версию мода).")
X(RejectHostToken, "Este servidor no acepta anfitriones con esa clave.", "This server does not accept hosts with that key.",
  "此服务器不接受使用该密钥的主机。", "Этот сервер не принимает хостов с таким ключом.")
X(RejectTooManyHosts, "Ya hay demasiados anfitriones conectados.", "There are too many hosts connected already.",
  "已连接的主机过多。", "Подключено слишком много хостов.")
X(HostConnected, "Anfitrión conectado (id {0})", "Host connected (id {0})", "主机已连接(id {0})", "Хост подключён (id {0})")
X(HostDisconnected, "Anfitrión desconectado (id {0})", "Host disconnected (id {0})", "主机已断开(id {0})", "Хост отключён (id {0})")
X(RejectProto, "Versión incompatible: el servidor usa el protocolo v{0} y tu juego el v{1}. Usad la misma versión del mod.",
  "Incompatible version: the server uses protocol v{0} and your game v{1}. Use the same mod version.",
  "版本不兼容:服务器使用协议 v{0},而你的游戏使用 v{1}。请使用相同版本的模组。",
  "Несовместимая версия: сервер использует протокол v{0}, а ваша игра — v{1}. Используйте одну и ту же версию мода.")
X(RejectFull, "El servidor está lleno ({0} jugadores).", "The server is full ({0} players).", "服务器已满({0} 名玩家)。",
  "Сервер переполнен (игроков: {0}).")
X(RejectBadNick, "Nick inválido: usa de 3 a 16 letras, números o _.", "Invalid nick: use 3 to 16 letters, numbers or _.",
  "昵称无效:请使用 3 到 16 个字母、数字或下划线。", "Недопустимый ник: используйте от 3 до 16 букв, цифр или _.")
X(RejectNickTaken, "Ya hay alguien conectado con el nick '{0}'.", "Someone is already connected with the nick '{0}'.",
  "已经有人使用昵称“{0}”连接。", "Игрок с ником «{0}» уже подключён.")
X(RejectBanned, "Estás baneado de este servidor.", "You are banned from this server.", "你已被此服务器封禁。",
  "Вы заблокированы на этом сервере.")
X(RejectBannedReason, "Estás baneado de este servidor: {0}", "You are banned from this server: {0}",
  "你已被此服务器封禁:{0}", "Вы заблокированы на этом сервере: {0}")
X(RejectPassword, "Contraseña incorrecta.", "Wrong password.", "密码错误。", "Неверный пароль.")
X(PlayerJoined, "{0} se ha unido ({1}, id {2})", "{0} joined ({1}, id {2})", "{0} 加入了({1},id {2})",
  "{0} присоединился ({1}, id {2})")
X(PlayerLeft, "{0} ha salido ({1})", "{0} left ({1})", "{0} 离开了({1})", "{0} вышел ({1})")

// ---- Command help ----
X(HelpTitle, "Comandos disponibles:", "Available commands:", "可用命令:", "Доступные команды:")
X(HelpHelp, "muestra esta lista", "shows this list", "显示此列表", "показывает этот список")
X(HelpUsage, "/help", "/help", "/help", "/help")
X(ListUsage, "/list", "/list", "/list", "/list")
X(ListHelp, "jugadores conectados y dónde están", "connected players and where they are", "在线玩家及其所在位置",
  "подключённые игроки и где они находятся")
X(PmUsage, "/pm <jugador> <mensaje>", "/pm <player> <message>", "/pm <玩家> <消息>", "/pm <игрок> <сообщение>")
X(PmHelp, "mensaje privado", "private message", "私聊消息", "личное сообщение")
X(GiftUsage, "/gift <jugador> <cantidad>", "/gift <player> <amount>", "/gift <玩家> <数量>", "/gift <игрок> <количество>")
X(GiftHelp, "regalar rupias a un jugador", "give rupees to a player", "赠送卢比给玩家", "подарить рупии игроку")
X(TpUsage, "/tp <jugador>", "/tp <player>", "/tp <玩家>", "/tp <игрок>")
X(TpHelp, "teletransportarte a un jugador (admins: /tp <jugador> <destino>)",
  "teleport to a player (admins: /tp <player> <destination>)", "传送到某位玩家身边(管理员:/tp <玩家> <目标>)",
  "телепортироваться к игроку (админы: /tp <игрок> <цель>)")
X(SayUsage, "/say <mensaje>", "/say <message>", "/say <消息>", "/say <сообщение>")
X(SayHelp, "anuncio del servidor para todos", "server announcement for everyone", "向所有人发送服务器公告",
  "объявление сервера для всех")
X(StopUsage, "/stop [motivo]", "/stop [reason]", "/stop [原因]", "/stop [причина]")
X(StopHelp, "cerrar el servidor", "close the server", "关闭服务器", "закрыть сервер")
X(KickUsage, "/kick <jugador> [motivo]", "/kick <player> [reason]", "/kick <玩家> [原因]", "/kick <игрок> [причина]")
X(KickHelp, "expulsar a un jugador", "kick a player", "踢出玩家", "исключить игрока")
X(BanUsage, "/ban <jugador> [motivo]", "/ban <player> [reason]", "/ban <玩家> [原因]", "/ban <игрок> [причина]")
X(BanHelp, "banear nick e IP y expulsar", "ban nick and IP and kick", "封禁昵称和 IP 并踢出",
  "заблокировать ник и IP и исключить")
X(UnbanUsage, "/unban <nick|ip>", "/unban <nick|ip>", "/unban <昵称|ip>", "/unban <ник|ip>")
X(UnbanHelp, "quitar un ban", "remove a ban", "解除封禁", "снять блокировку")
X(BanlistUsage, "/banlist", "/banlist", "/banlist", "/banlist")
X(BanlistHelp, "lista de baneados", "list of banned players", "封禁列表", "список заблокированных")
X(OpUsage, "/op <jugador>", "/op <player>", "/op <玩家>", "/op <игрок>")
X(OpHelp, "dar permisos de administrador", "give administrator permissions", "授予管理员权限", "выдать права администратора")
X(DeopUsage, "/deop <jugador>", "/deop <player>", "/deop <玩家>", "/deop <игрок>")
X(DeopHelp, "quitar permisos de administrador", "remove administrator permissions", "撤销管理员权限",
  "забрать права администратора")
X(StatsUsage, "/stats", "/stats", "/stats", "/stats")
X(StatsHelp, "estadísticas de red por jugador", "network statistics per player", "每位玩家的网络统计",
  "сетевая статистика по игрокам")
X(UnlockAllUsage, "/unlockall <jugador>", "/unlockall <player>", "/unlockall <玩家>", "/unlockall <игрок>")
X(UnlockAllHelp, "dar todos los objetos, máscaras, canciones y corazones (lo compartido es para toda la partida)",
  "give every item, mask, song and heart (the shared ones are for the whole world)",
  "给予所有道具、面具、歌曲和心之容器(共享的部分对整个世界生效)",
  "выдать все предметы, маски, песни и сердца (общее — для всего мира)")
X(GiveUsage, "/give <jugador> <cantidad>", "/give <player> <amount>", "/give <玩家> <数量>", "/give <игрок> <количество>")
X(GiveHelp, "entregar rupias (lo que no quepa va al banco)", "hand out rupees (what does not fit goes to the bank)",
  "发放卢比(装不下的部分存入银行)", "выдать рупии (что не поместится — уйдёт в банк)")
X(FreezeTimeUsage, "/freezetime [on|off]", "/freezetime [on|off]", "/freezetime [on|off]", "/freezetime [on|off]")
X(FreezeTimeHelp, "detener o reanudar el tiempo del mundo", "stop or resume the world's time", "暂停或恢复世界时间",
  "остановить или возобновить время мира")
X(TimeUsage, "/time set <day|night|dawn>", "/time set <day|night|dawn>", "/time set <day|night|dawn>",
  "/time set <day|night|dawn>")
X(TimeHelp, "cambiar el momento del día actual", "change the current time of day", "更改当前的时间段", "изменить текущее время суток")
X(ClockUsage, "/tiempo  (/clock)", "/clock  (/tiempo)", "/clock  (/tiempo)", "/clock  (/tiempo)")
X(ClockHelp, "día y hora de la partida del servidor", "day and time of the server's game", "服务器游戏的日期和时间",
  "день и время игры сервера")
X(YesUsage, "/si  (/yes)", "/yes  (/si)", "/yes  (/si)", "/yes  (/si)")
X(YesHelp, "votar sí a volver al Amanecer del Primer Día", "vote yes to return to the Dawn of the First Day",
  "投票同意回到第一天的黎明", "проголосовать «да» за возврат к Рассвету первого дня")
X(NoUsage, "/no", "/no", "/no", "/no")
X(NoHelp, "votar no a volver al Amanecer del Primer Día", "vote no to return to the Dawn of the First Day",
  "投票反对回到第一天的黎明", "проголосовать «нет» за возврат к Рассвету первого дня")
X(SetTimeUsage, "/settime <día 1-3> <hh:mm>", "/settime <day 1-3> <hh:mm>", "/settime <第 1-3 天> <hh:mm>",
  "/settime <день 1-3> <чч:мм>")
X(SetTimeHelp, "cambiar el día y la hora para todos", "change the day and time for everyone", "为所有人更改日期和时间",
  "изменить день и время для всех")
X(WorldUsage, "/mundo  (/world)", "/world  (/mundo)", "/world  (/mundo)", "/world  (/mundo)")
X(WorldHelp, "estado de la partida del servidor", "state of the server's game", "服务器游戏的状态", "состояние игры сервера")
X(RestartUsage, "/reiniciar  (/restart)", "/restart  (/reiniciar)", "/restart  (/reiniciar)", "/restart  (/reiniciar)")
X(RestartHelp, "volver al Amanecer del Primer Día sin votación", "return to the Dawn of the First Day without a vote",
  "不经投票直接回到第一天的黎明", "вернуться к Рассвету первого дня без голосования")
X(LangUsage, "/lang [es|en|zh|ru]", "/lang [es|en|zh|ru]", "/lang [es|en|zh|ru]", "/lang [es|en|zh|ru]")
X(LangHelp, "ver o cambiar el idioma del servidor", "show or change the server language", "查看或更改服务器语言",
  "показать или изменить язык сервера")

// ---- Command replies ----
X(PmEmpty, "El mensaje está vacío.", "The message is empty.", "消息为空。", "Сообщение пустое.")
X(PmToConsole, "[privado a {0}] {1}", "[private to {0}] {1}", "[私聊 {0}] {1}", "[лично для {0}] {1}")
X(SayPrefix, "[Servidor] {0}", "[Server] {0}", "[服务器] {0}", "[Сервер] {0}")
X(StatsTitle, "Estadísticas de red:", "Network statistics:", "网络统计:", "Сетевая статистика:")
X(StatsLine, "{0}: escena {1}, sala {2}, poses recibidas {3}, reenviadas {4}",
  "{0}: scene {1}, room {2}, poses received {3}, relayed {4}", "{0}:场景 {1},房间 {2},收到姿态 {3},转发 {4}",
  "{0}: сцена {1}, комната {2}, получено поз {3}, переслано {4}")
X(ListTitle, "Jugadores ({0}/{1}):", "Players ({0}/{1}):", "玩家({0}/{1}):", "Игроки ({0}/{1}):")
X(ListAdminTag, " [admin]", " [admin]", " [管理员]", " [админ]")
X(TpOnlyAdmins, "Solo los administradores pueden mover a otros jugadores.", "Only administrators can move other players.",
  "只有管理员可以移动其他玩家。", "Только администраторы могут перемещать других игроков.")
X(TpConsoleUsage, "Desde la consola usa: /tp <jugador> <destino>", "From the console use: /tp <player> <destination>",
  "在控制台中请使用:/tp <玩家> <目标>", "В консоли используйте: /tp <игрок> <цель>")
X(TpSelf, "No se puede teletransportar a alguien hasta sí mismo.", "You cannot teleport someone to themselves.",
  "无法把玩家传送到他自己身边。", "Нельзя телепортировать игрока к нему самому.")
X(TpNotInGame, "{0} todavía no está dentro del juego.", "{0} is not in the game yet.", "{0} 还没有进入游戏。",
  "{0} ещё не в игре.")
X(TpTeleporting, "Teletransportando a {0} hasta {1}...", "Teleporting {0} to {1}...", "正在将 {0} 传送到 {1}……",
  "Телепортация {0} к {1}...")
X(TpNotifyTarget, "{0} se está teletransportando a ti.", "{0} is teleporting to you.", "{0} 正在传送到你身边。",
  "{0} телепортируется к вам.")
X(GiftOnlyPlayers, "Solo los jugadores pueden usar /gift.", "Only players can use /gift.", "只有玩家可以使用 /gift。",
  "/gift могут использовать только игроки.")
X(GiftSelf, "No puedes regalarte rupias a ti mismo.", "You cannot gift rupees to yourself.", "你不能给自己赠送卢比。",
  "Нельзя подарить рупии самому себе.")
X(GiftAmountInt, "La cantidad debe ser un número entero.", "The amount must be a whole number.", "数量必须是整数。",
  "Количество должно быть целым числом.")
X(GiftAmountRange, "La cantidad debe estar entre 1 y {0}.", "The amount must be between 1 and {0}.",
  "数量必须在 1 到 {0} 之间。", "Количество должно быть от 1 до {0}.")
X(GiftRefundLost, "Reembolso perdido de {0} rupias para {1} (ya no está conectado)",
  "Lost refund of {0} rupees for {1} (no longer connected)", "{1} 的 {0} 卢比退款丢失(已不在线)",
  "Потерян возврат {0} рупий для {1} (игрок больше не подключён)")
X(GiftCancelled, "el regalo se canceló", "the gift was cancelled", "赠送已取消", "подарок отменён")
X(GiftNotEnough, "No tienes suficientes rupias para regalar {0}.", "You do not have enough rupees to give {0}.",
  "你的卢比不足以赠送 {0}。", "У вас недостаточно рупий, чтобы подарить {0}.")
X(GiftDisconnected, "{0} se ha desconectado", "{0} disconnected", "{0} 已断开连接", "{0} отключился")
X(GiftReceived, "{0} te ha regalado {1} rupias.", "{0} gave you {1} rupees.", "{0} 赠送了你 {1} 卢比。", "{0} подарил вам {1} рупий.")
X(GiftSent, "Has regalado {0} rupias a {1}.", "You gave {0} rupees to {1}.", "你赠送了 {0} 卢比给 {1}。",
  "Вы подарили {0} рупий игроку {1}.")
X(GiftLog, "{0} regaló {1} rupias a {2}", "{0} gave {1} rupees to {2}", "{0} 赠送了 {1} 卢比给 {2}",
  "{0} подарил {1} рупий игроку {2}")
X(GiftNoRoom, "{0} no tiene espacio en la cartera.", "{0} has no room in their wallet.", "{0} 的钱包装不下了。",
  "У {0} нет места в кошельке.")
X(GiftWalletFull, "la cartera de {0} está llena", "{0}'s wallet is full", "{0} 的钱包已满", "кошелёк {0} полон")
X(GiftNoReply, "no hubo respuesta", "there was no answer", "没有回应", "ответа не было")
X(GiftExpired, "El regalo ha caducado.", "The gift has expired.", "赠送已过期。", "Срок действия подарка истёк.")
X(AlreadyIs, "Ya es {0}.", "It is already {0}.", "现在已经是 {0}。", "Уже {0}.")
X(UnlockAllReply, "Desbloqueando todo para {0}...", "Unlocking everything for {0}...", "正在为 {0} 解锁全部内容……",
  "Разблокировка всего для {0}...")
X(UnlockAllNotice, "{0} ha desbloqueado todos los objetos, máscaras, canciones y corazones de la partida.",
  "{0} unlocked every item, mask, song and heart of the world.", "{0} 解锁了世界中所有的道具、面具、歌曲和心之容器。",
  "{0} разблокировал все предметы, маски, песни и сердца мира.")
X(GiveRange, "La cantidad debe ser un número entre 1 y 999999.", "The amount must be a number between 1 and 999999.",
  "数量必须是 1 到 999999 之间的数字。", "Количество должно быть числом от 1 до 999999.")
X(GiveReply, "Entregando {0} rupias a {1}...", "Giving {0} rupees to {1}...", "正在给 {1} 发放 {0} 卢比……",
  "Выдача {0} рупий игроку {1}...")
X(NoWorldYet, "Todavía no hay mundo: nadie ha entrado en la partida del servidor.",
  "There is no world yet: nobody has entered the server's game.", "还没有世界:还没有人进入服务器的游戏。",
  "Мира ещё нет: никто не вошёл в игру сервера.")
X(TimeSetUsage, "Uso: /time set <day|night|dawn> (o /tiempo para ver la hora).",
  "Usage: /time set <day|night|dawn> (or /clock to see the time).", "用法:/time set <day|night|dawn>(或用 /clock 查看时间)。",
  "Использование: /time set <day|night|dawn> (или /clock, чтобы узнать время).")
X(TimeUnknown, "Momento desconocido: usa day (día), night (noche) o dawn (amanecer).",
  "Unknown time of day: use day, night or dawn.", "未知的时间段:请使用 day(白天)、night(夜晚)或 dawn(黎明)。",
  "Неизвестное время суток: используйте day (день), night (ночь) или dawn (рассвет).")
X(SetTimeUsageLine, "Uso: /settime <día 1-3> <hh:mm>, por ejemplo /settime 2 18:00",
  "Usage: /settime <day 1-3> <hh:mm>, for example /settime 2 18:00", "用法:/settime <第 1-3 天> <hh:mm>,例如 /settime 2 18:00",
  "Использование: /settime <день 1-3> <чч:мм>, например /settime 2 18:00")
X(KickDone, "{0} ha sido expulsado.", "{0} was kicked.", "{0} 已被踢出。", "{0} исключён.")
X(BanKickReason, "baneado", "banned", "已被封禁", "заблокирован")
X(BanKickReasonText, "baneado: {0}", "banned: {0}", "已被封禁:{0}", "заблокирован: {0}")
X(BanDone, "{0} ha sido baneado (nick e IP).", "{0} was banned (nick and IP).", "{0} 已被封禁(昵称和 IP)。",
  "{0} заблокирован (ник и IP).")
X(BadNick, "Nick inválido: '{0}'.", "Invalid nick: '{0}'.", "无效的昵称:“{0}”。", "Недопустимый ник: «{0}».")
X(BanNickOnly, "{0} no está conectado; se ha baneado su nick.", "{0} is not connected; their nick was banned.",
  "{0} 不在线;已封禁其昵称。", "{0} не в сети; его ник заблокирован.")
X(UnbanDone, "Se ha quitado el ban de '{0}'.", "The ban on '{0}' was removed.", "已解除对“{0}”的封禁。",
  "Блокировка «{0}» снята.")
X(UnbanNone, "No había ningún ban para '{0}'.", "There was no ban for '{0}'.", "没有针对“{0}”的封禁。",
  "Для «{0}» блокировки не было.")
X(BanListEmpty, "No hay nadie baneado.", "Nobody is banned.", "没有被封禁的玩家。", "Заблокированных нет.")
X(BanListTitle, "Baneados ({0}):", "Banned ({0}):", "已封禁({0}):", "Заблокированные ({0}):")
X(OpNotConnected, "'{0}' no está conectado. El permiso se vincula a su IP: repite 'op' cuando entre.",
  "'{0}' is not connected. The permission is bound to their IP: repeat 'op' when they join.",
  "“{0}”不在线。权限与其 IP 绑定:请在其进入后重新执行 'op'。",
  "«{0}» не в сети. Права привязываются к IP: повторите 'op', когда игрок зайдёт.")
X(OpAlready, "{0} ya era administrador.", "{0} was already an administrator.", "{0} 已经是管理员。", "{0} уже администратор.")
X(OpDone, "{0} ahora es administrador (IP {1}).", "{0} is now an administrator (IP {1}).", "{0} 现在是管理员(IP {1})。",
  "{0} теперь администратор (IP {1}).")
X(OpNotice, "Ahora eres administrador del servidor.", "You are now a server administrator.", "你现在是服务器管理员了。",
  "Теперь вы администратор сервера.")
X(DeopNone, "{0} no era administrador.", "{0} was not an administrator.", "{0} 不是管理员。", "{0} не был администратором.")
X(DeopDone, "{0} ya no es administrador.", "{0} is no longer an administrator.", "{0} 不再是管理员。",
  "{0} больше не администратор.")
X(DeopNotice, "Ya no eres administrador del servidor.", "You are no longer a server administrator.", "你不再是服务器管理员了。",
  "Вы больше не администратор сервера.")

// ---- Ending ----
X(RooftopStopped, "La cuenta atrás de la torre del reloj se ha detenido.", "The clock tower countdown has stopped.",
  "钟楼倒计时已停止。", "Обратный отсчёт на башне с часами остановлен.")
X(RooftopStarted, "{0} ha empezado la cuenta atrás de la torre del reloj ({1} s).",
  "{0} started the clock tower countdown ({1} s).", "{0} 开始了钟楼倒计时({1} 秒)。",
  "{0} запустил обратный отсчёт на башне с часами ({1} с).")
X(RooftopEnded, "Se ha acabado la cuenta atrás de la torre del reloj.", "The clock tower countdown has run out.",
  "钟楼倒计时已结束。", "Обратный отсчёт на башне с часами закончился.")
X(EndingServerGame, "La partida del servidor", "The server's game", "服务器的游戏", "Игра сервера")
X(EndingStage1, "{0} ha subido a la torre del reloj: todos van allí.", "{0} climbed the clock tower: everyone goes there.",
  "{0} 登上了钟楼:所有人都会被带到那里。", "{0} поднялся на башню с часами: все отправляются туда.")
X(EndingStage2, "{0} ha tocado la Canción del Juramento: todos van a luchar contra Majora.",
  "{0} played the Oath to Order: everyone goes to fight Majora.", "{0} 演奏了秩序誓约:所有人都将去对战 Majora。",
  "{0} сыграл Клятву порядка: все отправляются сражаться с Маджорой.")
X(EndingStage3, "{0} ha derrotado a Majora.", "{0} defeated Majora.", "{0} 击败了 Majora。", "{0} победил Маджору.")
X(EndingStage4, "{0} empieza el final del juego: todos lo veis juntos.", "{0} starts the ending: everyone watches it together.",
  "{0} 开始了结局:所有人一起观看。", "{0} запускает финал игры: все смотрят его вместе.")
X(InvEnding, "final inválido", "invalid ending", "无效的结局数据", "недопустимые данные финала")
X(InvEndingSync, "sincronización del final inválida", "invalid ending sync", "无效的结局同步数据", "недопустимая синхронизация финала")
X(InvCountdown, "cuenta atrás inválida", "invalid countdown", "无效的倒计时", "недопустимый обратный отсчёт")
X(InvCamera, "cámara inválida", "invalid camera", "无效的摄像机数据", "недопустимые данные камеры")

// ---- Reasons a packet was invalid (logs) ----
X(InvActorPacket, "paquete de actores inválido", "invalid actor packet", "无效的角色数据包", "недопустимый пакет акторов")
X(InvHit, "golpe inválido", "invalid hit", "无效的攻击数据", "недопустимый удар")
X(InvDamage, "daño inválido", "invalid damage", "无效的伤害数据", "недопустимый урон")
X(InvDrop, "objeto soltado inválido", "invalid dropped item", "无效的掉落物数据", "недопустимый выпавший предмет")
X(InvLoan, "préstamo inválido", "invalid loan", "无效的借用数据", "недопустимая передача актора")
X(InvEchoActor, "actor de eco inválido", "invalid echo actor", "无效的回声角色", "недопустимый актор-эхо")
X(InvEponaCall, "llamada de Epona inválida", "invalid Epona call", "无效的 Epona 呼唤", "недопустимый вызов Эпоны")
X(InvEponaPassenger, "pasajero de Epona inválido", "invalid Epona passenger", "无效的 Epona 乘客", "недопустимый пассажир Эпоны")
X(InvEponaState, "estado de Epona inválido", "invalid Epona state", "无效的 Epona 状态", "недопустимое состояние Эпоны")
X(InvPropObject, "objeto del escenario inválido", "invalid scenery object", "无效的场景物件", "недопустимый объект окружения")
X(InvPropRequest, "petición de objetos inválida", "invalid object request", "无效的物件请求", "недопустимый запрос объектов")
X(InvPose, "pose inválida", "invalid pose", "无效的姿态", "недопустимая поза")
X(InvSceneFlags, "banderas de la escena inválidas", "invalid scene flags", "无效的场景标志", "недопустимые флаги сцены")
X(InvAmbient, "ambiente inválido (música o temblor)", "invalid ambient (music or quake)", "无效的环境数据(音乐或震动)",
  "недопустимое окружение (музыка или тряска)")
X(InvSong, "canción inválida", "invalid song", "无效的乐曲", "недопустимая мелодия")
X(InvWorldFlood, "demasiadas entradas y salidas de la partida del servidor",
  "too many entries and exits of the server's game", "进出服务器游戏过于频繁", "слишком много входов и выходов из игры сервера")
X(InvWorldInitUnasked, "world_init sin haberlo pedido el servidor", "world_init that the server did not ask for",
  "服务器未请求的 world_init", "world_init, который сервер не запрашивал")
X(InvWorldInit, "world_init inválido: {0}", "invalid world_init: {0}", "无效的 world_init:{0}", "недопустимый world_init: {0}")
X(InvInventoryFlood, "demasiadas subidas de inventario", "too many inventory uploads", "上传物品栏过于频繁",
  "слишком много отправок инвентаря")
X(InvInventory, "inventario inválido o demasiado grande", "invalid or too large inventory", "物品栏无效或过大",
  "недопустимый или слишком большой инвентарь")
X(InvCycleUnasked, "cycle_result sin haberlo pedido el servidor", "cycle_result that the server did not ask for",
  "服务器未请求的 cycle_result", "cycle_result, который сервер не запрашивал")
X(InvCycle, "cycle_result inválido: {0}", "invalid cycle_result: {0}", "无效的 cycle_result:{0}", "недопустимый cycle_result: {0}")
X(MissingFields, "falta 'fields'", "'fields' is missing", "缺少 'fields'", "отсутствует 'fields'")

// ---- Shared world ----
X(NoWorldLog, "Todavía no hay mundo: lo creará el primer jugador que entre en la partida.",
  "There is no world yet: the first player to enter the game will create it.", "还没有世界:将由第一个进入游戏的玩家创建。",
  "Мира ещё нет: его создаст первый вошедший в игру игрок.")
X(NoWorldCreate, "Todavía no hay mundo: lo creará el primer jugador que entre en la partida del servidor.",
  "There is no world yet: the first player to enter the server's game will create it.",
  "还没有世界:将由第一个进入服务器游戏的玩家创建。", "Мира ещё нет: его создаст первый игрок, вошедший в игру сервера.")
X(WorldLoaded, "Mundo cargado: ciclo {0}, {1}.", "World loaded: cycle {0}, {1}.", "世界已加载:第 {0} 个周期,{1}。",
  "Мир загружен: цикл {0}, {1}.")
X(WorldCorrupt,
  "{0} está dañado ({1}); se ha apartado como {2}.",
  "{0} is damaged ({1}); it was moved aside as {2}.",
  "{0} 已损坏({1});已另存为 {2}。",
  "{0} повреждён ({1}); он перемещён как {2}.")
X(WorldSaveFail, "No se pudo guardar el mundo: {0}", "Could not save the world: {0}", "无法保存世界:{0}", "Не удалось сохранить мир: {0}")
X(PlayersSaveFail, "No se pudo guardar a los jugadores: {0}", "Could not save the players: {0}", "无法保存玩家数据:{0}",
  "Не удалось сохранить игроков: {0}")
X(CreatorLate, "Tu juego no creó el mundo a tiempo; lo creará otro jugador. Vuelve a entrar en la partida del servidor.",
  "Your game did not create the world in time; another player will create it. Enter the server's game again.",
  "你的游戏未能及时创建世界;将由其他玩家创建。请重新进入服务器的游戏。",
  "Ваша игра не создала мир вовремя; его создаст другой игрок. Войдите в игру сервера снова.")
X(CreatorLateLog, "El juego encargado de crear el mundo no respondió a tiempo.",
  "The game in charge of creating the world did not answer in time.", "负责创建世界的游戏未能及时响应。",
  "Игра, отвечающая за создание мира, не ответила вовремя.")
X(HostFollows, "El anfitrión {0} sigue la partida del servidor.", "Host {0} follows the server's game.",
  "主机 {0} 正在跟随服务器的游戏。", "Хост {0} следует за игрой сервера.")
X(WaitReset, "Se está volviendo al Amanecer del Primer Día; entrarás en cuanto termine.",
  "The world is going back to the Dawn of the First Day; you will enter as soon as it is done.",
  "世界正在回到第一天的黎明;完成后你将立即进入。", "Мир возвращается к Рассвету первого дня; вы войдёте, как только это закончится.")
X(WaitCreate, "Se está creando el mundo; entrarás en cuanto esté listo.",
  "The world is being created; you will enter as soon as it is ready.", "正在创建世界;准备好后你将立即进入。",
  "Мир создаётся; вы войдёте, как только он будет готов.")
X(PlayerEntered, "{0} entra en la partida del servidor.", "{0} enters the server's game.", "{0} 进入了服务器的游戏。",
  "{0} входит в игру сервера.")
X(PlayerLeftWorld, "{0} sale de la partida del servidor.", "{0} leaves the server's game.", "{0} 离开了服务器的游戏。",
  "{0} выходит из игры сервера.")
X(WorldCreating, "{0} crea el mundo del servidor.", "{0} creates the server's world.", "{0} 正在创建服务器的世界。",
  "{0} создаёт мир сервера.")
X(WorldCreated, "Mundo creado por {0}.", "World created by {0}.", "世界由 {0} 创建。", "Мир создан игроком {0}.")
X(StopReset, "se está volviendo al Amanecer del Primer Día", "going back to the Dawn of the First Day",
  "正在回到第一天的黎明", "возврат к Рассвету первого дня")
X(StopPlayerPlace, "{0} está en un lugar donde el tiempo no corre", "{0} is in a place where time does not run",
  "{0} 处于时间不会流逝的地方", "{0} находится в месте, где время не идёт")
X(StopNobody, "no hay nadie en la partida", "nobody is in the game", "游戏中没有人", "в игре никого нет")
X(TimeSlow, "tiempo ralentizado", "slowed time", "时间变慢", "замедленное время")
X(TimeNormal, "velocidad normal", "normal speed", "正常速度", "обычная скорость")
X(TimeLine, "{0} · {1} · ciclo {2}", "{0} · {1} · cycle {2}", "{0} · {1} · 第 {2} 个周期", "{0} · {1} · цикл {2}")
X(TimeStoppedSuffix, " · parado: {0}", " · stopped: {0}", " · 已暂停:{0}", " · остановлено: {0}")
X(DescribeCreating, "Se está creando el mundo.", "The world is being created.", "正在创建世界。", "Мир создаётся.")
X(DescribeNone, "Todavía no hay mundo.", "There is no world yet.", "还没有世界。", "Мира ещё нет.")
X(DescWorld, "Mundo: {0}", "World: {0}", "世界:{0}", "Мир: {0}")
X(DescInGame, "\nEn la partida: {0}", "\nIn the game: {0}", "\n游戏中:{0}", "\nВ игре: {0}")
X(Nobody, "nadie", "nobody", "无人", "никого")
X(DescSaved, "\nJugadores guardados: {0}", "\nSaved players: {0}", "\n已保存的玩家:{0}", "\nСохранённых игроков: {0}")
X(DescVote, "\nVotación de la Canción del Tiempo en marcha ({0} s).", "\nSong of Time vote in progress ({0} s).",
  "\n时之歌投票进行中({0} 秒)。", "\nГолосование за Песнь времени идёт ({0} с).")
X(DescFile, "\nArchivo: {0}", "\nFile: {0}", "\n文件:{0}", "\nФайл: {0}")

// ---- Cycle rules ----
X(DoubleTimeLimit, "La Canción del Doble Tiempo no puede adelantar el tiempo más allá de la última noche.",
  "The Song of Double Time cannot move time past the last night.", "二重时之歌无法把时间推进到最后一夜之后。",
  "Песнь двойного времени не может продвинуть время дальше последней ночи.")
X(DoubleTimePlayed, "{0} ha tocado la Canción del Doble Tiempo: {1}.", "{0} played the Song of Double Time: {1}.",
  "{0} 演奏了二重时之歌:{1}。", "{0} сыграл Песнь двойного времени: {1}.")
X(InvertedOn, "{0} ha ralentizado el tiempo (Canción del Tiempo Invertida).",
  "{0} slowed down time (Inverted Song of Time).", "{0} 减慢了时间(逆时之歌)。", "{0} замедлил время (Обратная Песнь времени).")
X(InvertedOff, "{0} ha devuelto el tiempo a su velocidad normal.", "{0} put time back to normal speed.",
  "{0} 将时间恢复到了正常速度。", "{0} вернул времени обычную скорость.")
X(ResetAlready, "Ya se está volviendo al Amanecer del Primer Día.", "The world is already going back to the Dawn of the First Day.",
  "世界已经在回到第一天的黎明了。", "Мир уже возвращается к Рассвету первого дня.")
X(VoteAlready, "Ya hay una votación en marcha: escribe /si o /no.", "A vote is already in progress: type /yes or /no.",
  "已经有一个投票在进行:请输入 /yes 或 /no。", "Голосование уже идёт: введите /yes или /no.")
X(VoteProposed, "{0} quiere volver al Amanecer del Primer Día. Escribe /si o /no ({1} s).",
  "{0} wants to go back to the Dawn of the First Day. Type /yes or /no ({1} s).",
  "{0} 想回到第一天的黎明。请输入 /yes 或 /no({1} 秒)。",
  "{0} хочет вернуться к Рассвету первого дня. Введите /yes или /no ({1} с).")
X(VoteNone, "No hay ninguna votación en marcha.", "There is no vote in progress.", "当前没有进行中的投票。", "Сейчас нет голосования.")
X(VoteOnlyPlayers, "Solo votan los jugadores que están en la partida del servidor.",
  "Only the players in the server's game can vote.", "只有在服务器游戏中的玩家才能投票。",
  "Голосовать могут только игроки, находящиеся в игре сервера.")
X(VoteYesWord, "sí", "yes", "赞成", "да")
X(VoteNoWord, "no", "no", "反对", "нет")
X(VoteCast, "{0} vota {1} ({2} sí, {3} no, de {4}).", "{0} votes {1} ({2} yes, {3} no, out of {4}).",
  "{0} 投了{1}({2} 赞成,{3} 反对,共 {4} 人)。", "{0} голосует: {1} ({2} за, {3} против, из {4}).")
X(VoteFailed, "La votación para volver al Amanecer del Primer Día no ha salido adelante.",
  "The vote to go back to the Dawn of the First Day did not pass.", "回到第一天黎明的投票未通过。",
  "Голосование за возврат к Рассвету первого дня не прошло.")
X(VoteSourceWord, "votación", "vote", "投票", "голосование")
X(ResetBy, "Volvéis al Amanecer del Primer Día ({0}).", "You all go back to the Dawn of the First Day ({0}).",
  "大家回到第一天的黎明({0})。", "Вы возвращаетесь к Рассвету первого дня ({0}).")
X(ResetFailed, "No se pudo volver al Amanecer del Primer Día: ningún juego respondió.",
  "Could not go back to the Dawn of the First Day: no game answered.", "无法回到第一天的黎明:没有游戏响应。",
  "Не удалось вернуться к Рассвету первого дня: ни одна игра не ответила.")
X(ComputeLate, "Un juego no calculó el nuevo ciclo a tiempo; se pide a otro.",
  "A game did not compute the new cycle in time; another one is asked.", "有一个游戏未能及时计算新周期;改为请求另一个游戏。",
  "Одна из игр не успела вычислить новый цикл; запрос отправлен другой.")
X(CycleStarted, "Empieza el ciclo {0}.", "Cycle {0} begins.", "第 {0} 个周期开始。", "Начинается цикл {0}.")
X(MoonFell, "La luna ha caído. Volvéis al Amanecer del Primer Día con lo que teníais al empezar el ciclo.",
  "The moon has fallen. You go back to the Dawn of the First Day with what you had when the cycle began.",
  "月亮坠落了。你们带着周期开始时拥有的东西回到第一天的黎明。",
  "Луна упала. Вы возвращаетесь к Рассвету первого дня с тем, что имели в начале цикла.")
X(FinishBy, "{0} ha visto el final del juego", "{0} saw the end of the game", "{0} 看完了游戏结局", "{0} досмотрел финал игры")
X(ResetWait, "Se está volviendo al Amanecer del Primer Día; espera un momento.",
  "The world is going back to the Dawn of the First Day; wait a moment.", "世界正在回到第一天的黎明;请稍等。",
  "Мир возвращается к Рассвету первого дня; подождите немного.")
X(TimeChanged, "La hora ha cambiado: {0} ({1}).", "The time has changed: {0} ({1}).", "时间已更改:{0}({1})。",
  "Время изменено: {0} ({1}).")
X(ClockAlreadyStopped, "El tiempo ya está detenido.", "Time is already stopped.", "时间已经暂停。", "Время уже остановлено.")
X(ClockAlreadyRunning, "El tiempo ya está en marcha.", "Time is already running.", "时间已经在流逝。", "Время уже идёт.")
X(ClockStopped, "El tiempo se ha detenido ({0}).", "Time has stopped ({0}).", "时间已暂停({0})。", "Время остановлено ({0}).")
X(ClockResumed, "El tiempo vuelve a correr ({0}).", "Time runs again ({0}).", "时间恢复流逝({0})。", "Время снова идёт ({0}).")
X(RestartNobody, "No hay nadie en la partida: el nuevo ciclo lo calcula el juego de un jugador.",
  "Nobody is in the game: the new cycle is computed by a player's game.", "游戏中没有人:新周期由玩家的游戏计算。",
  "В игре никого нет: новый цикл вычисляет игра одного из игроков.")

// ---- Files, configuration and network ----
X(ConfigInvalidJson, "{0} no es un JSON válido", "{0} is not valid JSON", "{0} 不是有效的 JSON", "{0} — недопустимый JSON")
X(ConfigWarn, "{0}: '{1}' debe ser {2}; se usa el valor por defecto", "{0}: '{1}' must be {2}; the default is used",
  "{0}:'{1}' 必须是{2};将使用默认值", "{0}: '{1}' должно быть {2}; используется значение по умолчанию")
X(ExpectInt, "un número entero", "a whole number", "一个整数", "целым числом")
X(ExpectString, "un texto entre comillas", "a text in quotes", "一个带引号的文本", "текстом в кавычках")
X(ExpectBool, "true o false", "true or false", "true 或 false", "true или false")
X(ExpectNumber, "un número dentro de su rango", "a number within its range", "一个范围内的数字", "числом в допустимом диапазоне")
X(ExpectObject, "un objeto { }", "an object { }", "一个对象 { }", "объектом { }")
X(ExpectList, "una lista de textos [ ]", "a list of texts [ ]", "一个文本列表 [ ]", "списком строк [ ]")
X(BadCommandPerm, "{0}: permiso '{2}' desconocido para /{1} (usa player, op o console); se ignora",
  "{0}: unknown permission '{2}' for /{1} (use player, op or console); ignored",
  "{0}:/{1} 的权限“{2}”未知(请使用 player、op 或 console);已忽略",
  "{0}: неизвестное право «{2}» для /{1} (используйте player, op или console); игнорируется")
X(BadGameSetting, "{0}: la opción de juego '{1}' no se puede forzar o su valor no es un número; se ignora",
  "{0}: the game setting '{1}' cannot be forced or its value is not a number; ignored",
  "{0}:游戏选项“{1}”无法强制设置,或其值不是数字;已忽略",
  "{0}: игровую настройку «{1}» нельзя навязать, либо её значение не число; игнорируется")
X(ArgNeedsValue, "{0} necesita un valor", "{0} needs a value", "{0} 需要一个值", "{0} требует значения")
X(BansNotList, "{0} no es una lista JSON; se ignora", "{0} is not a JSON list; ignored", "{0} 不是 JSON 列表;已忽略",
  "{0} — не JSON-список; игнорируется")
X(BansBadEntry, "{0}: se ignora una entrada inválida", "{0}: an invalid entry is ignored", "{0}:已忽略一个无效条目",
  "{0}: недопустимая запись игнорируется")
X(OpsBadEntry, "{0}: se ignora una entrada sin nick o sin IP (usa 'op <jugador>' en la consola con el jugador conectado)",
  "{0}: an entry without nick or IP is ignored (use 'op <player>' in the console while the player is connected)",
  "{0}:已忽略缺少昵称或 IP 的条目(请在玩家在线时于控制台使用 'op <玩家>')",
  "{0}: запись без ника или IP игнорируется (используйте 'op <игрок>' в консоли, пока игрок подключён)")
X(FileMissing, "{0} no existe", "{0} does not exist", "{0} 不存在", "{0} не существует")
X(FileOpenFail, "no se pudo abrir {0}", "could not open {0}", "无法打开 {0}", "не удалось открыть {0}")
X(FileNotJson, "{0} no es JSON válido", "{0} is not valid JSON", "{0} 不是有效的 JSON", "{0} — недопустимый JSON")
X(FileWriteFail, "no se pudo escribir {0}", "could not write {0}", "无法写入 {0}", "не удалось записать {0}")
X(FileReplaceFail, "no se pudo reemplazar {0}: {1}", "could not replace {0}: {1}", "无法替换 {0}:{1}", "не удалось заменить {0}: {1}")
X(PlayerFileInvalid, "players/{0} no es válido; se ignora", "players/{0} is not valid; ignored", "players/{0} 无效;已忽略",
  "players/{0} недопустим; игнорируется")
X(WsFieldsNotObject, "los campos del mundo no son un objeto", "the world fields are not an object", "世界字段不是对象",
  "поля мира не являются объектом")
X(WsMissingField, "falta el campo '{0}'", "field '{0}' is missing", "缺少字段“{0}”", "отсутствует поле «{0}»")
X(WsNotHex, "el campo '{0}' no es hexadecimal", "field '{0}' is not hexadecimal", "字段“{0}”不是十六进制",
  "поле «{0}» не является шестнадцатеричным")
X(WsBadSize, "el campo '{0}' debe tener {1} bytes", "field '{0}' must have {1} bytes", "字段“{0}”必须为 {1} 字节",
  "поле «{0}» должно содержать {1} байт")
X(WsFieldAdded, "world.json: el campo '{0}' no estaba (mundo de una versión anterior): empieza vacío",
  "world.json: field '{0}' was missing (a world from an older version): it starts empty",
  "world.json:缺少字段“{0}”(来自旧版本的世界):将从空值开始",
  "world.json: поля «{0}» не было (мир из старой версии): оно начинается пустым")
X(WsFieldResized, "world.json: el campo '{0}' tenía {1} bytes y ahora tiene {2}",
  "world.json: field '{0}' had {1} bytes and now has {2}", "world.json:字段“{0}”原有 {1} 字节,现在为 {2} 字节",
  "world.json: в поле «{0}» было {1} байт, теперь {2}")
X(WsFieldUnknown, "world.json: el campo '{0}' no existe en esta versión: se ignora",
  "world.json: field '{0}' does not exist in this version: ignored", "world.json:此版本中不存在字段“{0}”:已忽略",
  "world.json: поля «{0}» нет в этой версии: игнорируется")
X(NotAnObject, "no es un objeto", "is not an object", "不是对象", "не является объектом")
X(WsBadCycle, "ciclo inválido", "invalid cycle", "无效的周期", "недопустимый цикл")
X(OpsNotList, "'{0}' no es una lista", "'{0}' is not a list", "“{0}”不是列表", "«{0}» не является списком")
X(OpsBadBits, "cambio de bits mal formado", "malformed bit change", "格式错误的位更改", "неверно сформированное изменение битов")
X(OpsBadBytes, "cambio de byte mal formado", "malformed byte change", "格式错误的字节更改", "неверно сформированное изменение байта")
X(OpsBadAdds, "suma mal formada", "malformed addition", "格式错误的加法更改", "неверно сформированное сложение")
X(NetInitFail, "No se pudo inicializar la red (enet_initialize)", "Could not initialize the network (enet_initialize)",
  "无法初始化网络(enet_initialize)", "Не удалось инициализировать сеть (enet_initialize)")
X(NetPortFail, "No se pudo abrir el puerto UDP {0} (¿ya está en uso?)", "Could not open UDP port {0} (already in use?)",
  "无法打开 UDP 端口 {0}(是否已被占用?)", "Не удалось открыть UDP-порт {0} (возможно, он уже занят?)")
X(NetSocketFail, "No se pudo crear el socket UDP", "Could not create the UDP socket", "无法创建 UDP 套接字",
  "Не удалось создать UDP-сокет")
X(NetResolveFail, "No se pudo resolver la dirección '{0}'", "Could not resolve the address '{0}'", "无法解析地址“{0}”",
  "Не удалось разрешить адрес «{0}»")
X(NetConnectFail, "No se pudo iniciar la conexión", "Could not start the connection", "无法发起连接", "Не удалось начать подключение")

// ---- Groups and activities (GroupCommands.cpp, Groups.cpp, ActivityHandlers.cpp) ----
X(InviteUsage, "/invitar <jugador>... | todos  (/invite)", "/invite <player>... | all  (/invitar)",
  "/invite <玩家>... | all  (/invitar)", "/invite <игрок>... | all  (/invitar)")
X(InviteHelp, "invitar a tu grupo: minijuegos, misiones, diálogos y cinemáticas juntos",
  "invite to your group: minigames, quests, dialogues and cutscenes together",
  "邀请加入你的小队:一起玩小游戏、做任务、看对话和过场动画",
  "пригласить в свою группу: мини-игры, задания, диалоги и сцены вместе")
X(AcceptUsage, "/aceptar [jugador]  (/accept)", "/accept [player]  (/aceptar)", "/accept [玩家]  (/aceptar)",
  "/accept [игрок]  (/aceptar)")
X(AcceptHelp, "aceptar una invitación (viajas junto a quien te invitó)",
  "accept an invitation (you travel to whoever invited you)", "接受邀请(你会传送到邀请者身边)",
  "принять приглашение (вы переместитесь к пригласившему)")
X(DeclineUsage, "/rechazar [jugador]  (/decline)", "/decline [player]  (/rechazar)", "/decline [玩家]  (/rechazar)",
  "/decline [игрок]  (/rechazar)")
X(DeclineHelp, "rechazar una invitación (sin jugador, todas)", "decline an invitation (without a player, all of them)",
  "拒绝邀请(不指定玩家则拒绝全部)", "отклонить приглашение (без игрока — все)")
X(GroupUsage, "/grupo  (/group)", "/group  (/grupo)", "/group  (/grupo)", "/group  (/grupo)")
X(GroupHelp, "ver tu grupo y tus invitaciones", "show your group and your invitations", "查看你的小队和邀请",
  "показать вашу группу и приглашения")
X(LeaveGroupUsage, "/dejargrupo  (/leavegroup)", "/leavegroup  (/dejargrupo)", "/leavegroup  (/dejargrupo)",
  "/leavegroup  (/dejargrupo)")
X(LeaveGroupHelp, "salir de tu grupo", "leave your group", "离开你的小队", "выйти из группы")
X(GroupOnlyPlayers, "Solo los jugadores pueden formar grupos.", "Only players can form groups.", "只有玩家可以组队。",
  "Создавать группы могут только игроки.")
X(GroupsOff, "Los grupos están desactivados en este servidor.", "Groups are turned off on this server.",
  "此服务器已关闭小队功能。", "Группы на этом сервере отключены.")
X(GroupNotInWorld, "Tienes que estar en la partida del servidor para formar grupos.",
  "You must be in the server's game to form groups.", "你必须在服务器的游戏中才能组队。",
  "Чтобы создавать группы, нужно быть в игре сервера.")
X(InviteSelf, "No puedes invitarte a ti mismo.", "You cannot invite yourself.", "你不能邀请自己。",
  "Нельзя пригласить самого себя.")
X(InviteNotInWorld, "{0} no está en la partida del servidor.", "{0} is not in the server's game.",
  "{0} 不在服务器的游戏中。", "{0} не в игре сервера.")
X(InviteAlreadyMate, "{0} ya está en tu grupo.", "{0} is already in your group.", "{0} 已经在你的小队里。",
  "{0} уже в вашей группе.")
X(InviteFull, "Tu grupo está lleno (máximo {0}).", "Your group is full (at most {0}).", "你的小队已满(最多 {0} 人)。",
  "Ваша группа заполнена (максимум {0}).")
X(InviteSent, "Invitación enviada a {0}.", "Invitation sent to {0}.", "已向 {0} 发送邀请。",
  "Приглашение отправлено: {0}.")
X(InviteNobody, "No hay nadie más en la partida para invitar.", "There is nobody else in the game to invite.",
  "游戏中没有其他可邀请的人。", "В игре больше некого пригласить.")
X(InviteToGroup, "{0} te invita a su grupo. Escribe /aceptar {0} o /rechazar {0}.",
  "{0} invites you to their group. Type /accept {0} or /decline {0}.",
  "{0} 邀请你加入小队。输入 /accept {0} 或 /decline {0}。",
  "{0} приглашает вас в свою группу. Введите /accept {0} или /decline {0}.")
X(InviteToActivity, "{0} te invita a {1}. Escribe /aceptar {0} o /rechazar {0}.",
  "{0} invites you to {1}. Type /accept {0} or /decline {0}.", "{0} 邀请你参加:{1}。输入 /accept {0} 或 /decline {0}。",
  "{0} приглашает вас: {1}. Введите /accept {0} или /decline {0}.")
X(InviteNone, "No tienes invitaciones pendientes.", "You have no pending invitations.", "你没有待处理的邀请。",
  "У вас нет ожидающих приглашений.")
X(InviteNoneFrom, "No tienes ninguna invitación de {0}.", "You have no invitation from {0}.", "你没有来自 {0} 的邀请。",
  "У вас нет приглашения от {0}.")
X(InviteGone, "El grupo de {0} ya no existe.", "{0}'s group no longer exists.", "{0} 的小队已不存在。",
  "Группы {0} больше нет.")
X(InviteFullOther, "El grupo de {0} está lleno.", "{0}'s group is full.", "{0} 的小队已满。", "Группа {0} заполнена.")
X(InviteAcceptedYou, "Te has unido al grupo de {0}. Viajando hasta su lado...",
  "You joined {0}'s group. Travelling to them...", "你加入了 {0} 的小队。正在前往其身边……",
  "Вы вступили в группу {0}. Перемещение к нему...")
X(InviteAcceptedOther, "{0} se ha unido al grupo.", "{0} joined the group.", "{0} 加入了小队。", "{0} вступил в группу.")
X(InviteDeclinedYou, "Has rechazado la invitación de {0}.", "You declined {0}'s invitation.", "你拒绝了 {0} 的邀请。",
  "Вы отклонили приглашение {0}.")
X(InviteDeclinedAll, "Has rechazado {0} invitaciones.", "You declined {0} invitations.", "你拒绝了 {0} 个邀请。",
  "Отклонено приглашений: {0}.")
X(InviteDeclinedOther, "{0} ha rechazado tu invitación.", "{0} declined your invitation.", "{0} 拒绝了你的邀请。",
  "{0} отклонил ваше приглашение.")
X(InviteExpiredYou, "La invitación de {0} ha caducado.", "{0}'s invitation expired.", "{0} 的邀请已过期。",
  "Приглашение {0} истекло.")
X(InviteExpiredOther, "{0} no respondió a tu invitación.", "{0} did not answer your invitation.",
  "{0} 没有回应你的邀请。", "{0} не ответил на ваше приглашение.")
X(GroupLeftYou, "Has salido del grupo.", "You left the group.", "你离开了小队。", "Вы вышли из группы.")
X(GroupLeftOther, "{0} ha salido del grupo.", "{0} left the group.", "{0} 离开了小队。", "{0} вышел из группы.")
X(GroupDissolved, "El grupo se ha deshecho.", "The group broke up.", "小队已解散。", "Группа распалась.")
X(GroupNone, "No estás en ningún grupo.", "You are not in a group.", "你不在任何小队中。", "Вы не состоите в группе.")
X(GroupTitle, "Grupo de {0} ({1}/{2}):", "{0}'s group ({1}/{2}):", "{0} 的小队({1}/{2}):", "Группа {0} ({1}/{2}):")
X(GroupLeaderTag, " (líder)", " (leader)", "(队长)", " (лидер)")
X(GroupInvitesTitle, "Invitaciones pendientes:", "Pending invitations:", "待处理的邀请:", "Ожидающие приглашения:")
X(GroupInviteLine, "{0} te invita ({1} s)", "{0} invites you ({1} s)", "{0} 邀请你({1} 秒)",
  "{0} приглашает вас ({1} с)")
X(ListActivityTag, " (en {0})", " (in {0})", "(正在:{0})", " (занят: {0})")
X(LogInvite, "{0} invita a {1} a su grupo", "{0} invites {1} to their group", "{0} 邀请 {1} 加入小队",
  "{0} приглашает {1} в группу")
X(LogGroupJoin, "{0} entra en el grupo de {1}", "{0} joins {1}'s group", "{0} 加入了 {1} 的小队",
  "{0} вступает в группу {1}")
X(LogGroupLeave, "{0} sale de su grupo", "{0} leaves their group", "{0} 离开了小队", "{0} выходит из группы")
X(LogActivityStart, "{0} empieza {1}", "{0} starts {1}", "{0} 开始了{1}", "{0} начинает: {1}")
X(LogActivityEnd, "{0} termina {1}", "{0} finishes {1}", "{0} 结束了{1}", "{0} заканчивает: {1}")
X(LogReward, "{0} comparte un premio con su grupo ({1})", "{0} shares a prize with their group ({1})",
  "{0} 与小队分享了奖励({1})", "{0} делится наградой с группой ({1})")
X(InvActivity, "actividad inválida", "invalid activity", "无效的活动", "недопустимое занятие")
X(InvActHud, "marcador de minijuego inválido", "invalid minigame HUD", "无效的小游戏界面数据",
  "недопустимые данные интерфейса мини-игры")
X(InvReward, "premio inválido", "invalid prize", "无效的奖励", "недопустимая награда")
X(InvFollow, "traslado inválido", "invalid trip", "无效的传送", "недопустимое перемещение")
X(InvTalk, "diálogo inválido", "invalid dialogue", "无效的对话", "недопустимый диалог")
X(InvTitle, "rótulo de jefe inválido", "invalid boss title card", "无效的首领标题", "недопустимая заставка босса")
X(LogActivityWon, "{0} gana {1}", "{0} wins {1}", "{0} 赢得了{1}", "{0} побеждает: {1}")
X(LogActivityLost, "{0} no gana {1}", "{0} does not win {1}", "{0} 没有赢得{1}", "{0} не побеждает: {1}")
X(InvEffects, "paquete de efectos inválido", "invalid effects packet", "无效的特效数据包", "недопустимый пакет эффектов")

// ---- Activity rooms (Rooms.cpp, Handlers/RoomHandlers.cpp, Commands/RoomCommands.cpp) ----
X(RoomUsage, "/sala [invitar|aceptar|rechazar|listo|salir|quitar|cerrar|unirse|abierta|viaje|premios] ...  (/room)",
  "/room [invite|accept|decline|ready|leave|kick|close|join|open|travel|rewards] ...  (/sala)",
  "/room [invite|accept|decline|ready|leave|kick|close|join|open|travel|rewards] ...  (/sala)",
  "/room [invite|accept|decline|ready|leave|kick|close|join|open|travel|rewards] ...  (/sala)")
X(RoomHelp, "tu sala de actividad: invitar, confirmar, salir y su configuración",
  "your activity room: invite, confirm, leave and its settings", "你的活动房间:邀请、确认、离开及其设置",
  "ваша комната занятия: приглашения, готовность, выход и настройки")
X(ReadyUsage, "/listo  (/ready)", "/ready  (/listo)", "/ready  (/listo)", "/ready  (/listo)")
X(ReadyHelp, "confirmar (o dejar de confirmar) que estás listo para la actividad de tu sala",
  "confirm (or take back) that you are ready for your room's activity", "确认(或取消确认)你已准备好进行房间的活动",
  "подтвердить (или отменить) готовность к занятию вашей комнаты")
X(RoomSayUsage, "/s <texto>", "/s <text>", "/s <文字>", "/s <текст>")
X(RoomSayHelp, "hablar en el chat de tu sala", "talk in your room's chat", "在房间聊天中发言",
  "написать в чат вашей комнаты")
X(RoomsOff, "Las salas están desactivadas en este servidor.", "Rooms are turned off on this server.",
  "此服务器已关闭房间功能。", "Комнаты на этом сервере отключены.")
X(RoomOnlyPlayers, "Solo los jugadores pueden usar las salas.", "Only players can use rooms.", "只有玩家可以使用房间。",
  "Комнатами могут пользоваться только игроки.")
X(RoomNotInWorld, "Tienes que estar en la partida del servidor para usar las salas.",
  "You must be in the server's game to use rooms.", "你必须在服务器的游戏中才能使用房间。",
  "Чтобы пользоваться комнатами, нужно быть в игре сервера.")
X(RoomNone, "No estás en ninguna sala.", "You are not in a room.", "你不在任何房间中。", "Вы не находитесь в комнате.")
X(RoomOnlyHost, "Solo el anfitrión de la sala puede hacer eso.", "Only the room's host can do that.",
  "只有房主可以这样做。", "Это может сделать только хозяин комнаты.")
X(RoomNotWaiting, "La sala no está esperando confirmaciones.", "The room is not waiting for confirmations.",
  "房间当前不在等待确认。", "Комната сейчас не ждёт подтверждений.")
X(RoomReadyYou, "Estás listo.", "You are ready.", "你已准备好。", "Вы готовы.")
X(RoomNotReadyYou, "Ya no estás listo.", "You are no longer ready.", "你已取消准备。", "Вы больше не готовы.")
X(RoomInviteSent, "Invitación a la sala enviada a {0}.", "Room invitation sent to {0}.", "已向 {0} 发送房间邀请。",
  "Приглашение в комнату отправлено игроку {0}.")
X(RoomInvitedYou, "{0} te invita a su sala: {1}. Escribe /sala aceptar {0} o /sala rechazar {0}.",
  "{0} invites you to their room: {1}. Type /room accept {0} or /room decline {0}.",
  "{0} 邀请你加入其房间:{1}。输入 /room accept {0} 或 /room decline {0}。",
  "{0} приглашает вас в свою комнату: {1}. Введите /room accept {0} или /room decline {0}.")
X(RoomAlreadyMember, "{0} ya está en tu sala.", "{0} is already in your room.", "{0} 已经在你的房间里。",
  "{0} уже в вашей комнате.")
X(RoomFull, "La sala está llena (máximo {0}).", "The room is full (at most {0}).", "房间已满(最多 {0} 人)。",
  "Комната заполнена (не больше {0}).")
X(RoomGone, "Esa sala ya no existe.", "That room no longer exists.", "该房间已不存在。", "Этой комнаты больше нет.")
X(RoomInviteNone, "No tienes invitaciones a salas.", "You have no room invitations.", "你没有房间邀请。",
  "У вас нет приглашений в комнаты.")
X(RoomInviteNoneFrom, "No tienes ninguna invitación a la sala de {0}.", "You have no invitation to {0}'s room.",
  "你没有来自 {0} 的房间邀请。", "У вас нет приглашения в комнату {0}.")
X(RoomInviteDeclinedYou, "Has rechazado la invitación a la sala de {0}.", "You declined the invitation to {0}'s room.",
  "你拒绝了 {0} 的房间邀请。", "Вы отклонили приглашение в комнату {0}.")
X(RoomInviteDeclinedOther, "{0} ha rechazado la invitación a tu sala.", "{0} declined the invitation to your room.",
  "{0} 拒绝了你的房间邀请。", "{0} отклонил приглашение в вашу комнату.")
X(RoomInviteExpiredYou, "La invitación a la sala de {0} ha caducado.", "The invitation to {0}'s room expired.",
  "{0} 的房间邀请已过期。", "Приглашение в комнату {0} истекло.")
X(RoomInviteExpiredOther, "{0} no respondió a la invitación a tu sala.",
  "{0} did not answer the invitation to your room.", "{0} 没有回应你的房间邀请。",
  "{0} не ответил на приглашение в вашу комнату.")
X(RoomAutoJoined, "{0} empieza {1}: estás en su sala. Pulsa Listo para jugar o sal de la sala si no quieres participar.",
  "{0} starts {1}: you are in their room. Press Ready to play, or leave the room if you do not want to take part.",
  "{0} 开始了{1}:你在其房间中。按“准备”参加,不想参加可离开房间。",
  "{0} начинает: {1}. Вы в его комнате. Нажмите «Готов», чтобы играть, или выйдите из комнаты, если не хотите "
  "участвовать.")
X(RoomJoinedYou, "Has entrado en la sala de {0}: {1}.", "You joined {0}'s room: {1}.", "你加入了 {0} 的房间:{1}。",
  "Вы вошли в комнату {0}: {1}.")
X(RoomJoinedOther, "{0} ha entrado en la sala.", "{0} joined the room.", "{0} 加入了房间。", "{0} вошёл в комнату.")
X(RoomLeftYou, "Has salido de la sala.", "You left the room.", "你离开了房间。", "Вы вышли из комнаты.")
X(RoomLeftOther, "{0} ha salido de la sala.", "{0} left the room.", "{0} 离开了房间。", "{0} вышел из комнаты.")
X(RoomKickedYou, "{0} te ha quitado de la sala.", "{0} removed you from the room.", "{0} 将你移出了房间。",
  "{0} удалил вас из комнаты.")
X(RoomKickedOther, "{0} ya no está en la sala.", "{0} is no longer in the room.", "{0} 已不在房间中。",
  "{0} больше нет в комнате.")
X(RoomNotMember, "{0} no está en tu sala.", "{0} is not in your room.", "{0} 不在你的房间里。", "{0} нет в вашей комнате.")
X(RoomClosed, "La sala se ha cerrado.", "The room closed.", "房间已关闭。", "Комната закрыта.")
X(RoomClosedTimeout, "La sala se ha cerrado: no empezó a tiempo.", "The room closed: it did not start in time.",
  "房间已关闭:未能及时开始。", "Комната закрыта: занятие не началось вовремя.")
X(RoomHostNow, "Ahora eres el anfitrión de la sala.", "You are now the room's host.", "你现在是房主。",
  "Теперь вы хозяин комнаты.")
X(RoomHostOther, "{0} es ahora el anfitrión de la sala.", "{0} is now the room's host.", "{0} 现在是房主。",
  "{0} теперь хозяин комнаты.")
X(RoomStarted, "¡Empieza {0}!", "{0} starts!", "{0}开始了!", "{0}: начали!")
X(RoomNotOpen, "La sala de {0} no está abierta: necesitas una invitación.",
  "{0}'s room is not open: you need an invitation.", "{0} 的房间未开放:你需要邀请。",
  "Комната {0} закрыта: нужно приглашение.")
X(RoomNoRoomOf, "{0} no tiene ninguna sala.", "{0} has no room.", "{0} 没有房间。", "У {0} нет комнаты.")
X(RoomAlreadyIn, "Ya estás en esa sala.", "You are already in that room.", "你已经在该房间中。", "Вы уже в этой комнате.")
X(RoomGroupNobody, "No hay nadie de tu grupo para invitar a la sala.",
  "There is nobody of your group to invite to the room.", "你的小队中没有可邀请到房间的人。",
  "В вашей группе некого пригласить в комнату.")
X(RoomSettingsNow, "Sala: abierta {0}, viaje {1}, premios {2}.", "Room: open {0}, travel {1}, rewards {2}.",
  "房间:开放 {0},传送 {1},奖励 {2}。", "Комната: открыта {0}, перемещение {1}, награды {2}.")
X(RoomYes, "sí", "yes", "是", "да")
X(RoomNo, "no", "no", "否", "нет")
X(RoomBadValue, "Usa sí o no.", "Use yes or no.", "请使用 yes 或 no。", "Используйте yes или no.")
X(RoomTitle, "Sala de {0}: {1}, {2} ({3}/{4}):", "{0}'s room: {1}, {2} ({3}/{4}):", "{0} 的房间:{1},{2}({3}/{4}):",
  "Комната {0}: {1}, {2} ({3}/{4}):")
X(RoomStateLobby, "esperando confirmaciones", "waiting for confirmations", "等待确认", "ждёт подтверждений")
X(RoomStateStarting, "a punto de empezar", "about to start", "即将开始", "вот-вот начнётся")
X(RoomStateRunning, "en marcha", "running", "进行中", "идёт")
X(RoomStateEnded, "terminada", "over", "已结束", "закончилась")
X(RoomHostTag, " (anfitrión)", " (host)", "(房主)", " (хозяин)")
X(RoomReadyTag, " - listo", " - ready", " - 已准备", " - готов")
X(RoomWaitingTag, " - esperando", " - waiting", " - 等待中", " - ждёт")
X(RoomInvitesTitle, "Invitaciones a salas:", "Room invitations:", "房间邀请:", "Приглашения в комнаты:")
X(RoomInviteLine, "{0}: {1} ({2} s)", "{0}: {1} ({2} s)", "{0}:{1}({2} 秒)", "{0}: {1} ({2} с)")
X(RoomChatEmpty, "Escribe algo para la sala.", "Type something for the room.", "请输入要发送到房间的内容。",
  "Напишите что-нибудь для комнаты.")
X(LogRoomOpen, "{0} abre una sala: {1}", "{0} opens a room: {1}", "{0} 开设了房间:{1}", "{0} открывает комнату: {1}")
X(LogRoomJoin, "{0} entra en la sala de {1} ({2})", "{0} joins {1}'s room ({2})", "{0} 加入了 {1} 的房间({2})",
  "{0} входит в комнату {1} ({2})")
X(LogRoomLeave, "{0} sale de una sala ({1})", "{0} leaves a room ({1})", "{0} 离开了房间({1})",
  "{0} выходит из комнаты ({1})")
X(LogRoomInvite, "{0} invita a {1} a su sala ({2})", "{0} invites {1} to their room ({2})",
  "{0} 邀请 {1} 加入其房间({2})", "{0} приглашает {1} в свою комнату ({2})")
X(LogRoomStart, "Empieza la sala de {0}: {1}", "{0}'s room starts: {1}", "{0} 的房间开始了:{1}",
  "Комната {0} начинает: {1}")
X(LogRoomChat, "[sala de {0}] <{1}> {2}", "[{0}'s room] <{1}> {2}", "[{0} 的房间] <{1}> {2}", "[комната {0}] <{1}> {2}")
X(InvRoom, "operación de sala inválida", "invalid room operation", "无效的房间操作", "неверная операция с комнатой")

// ---- Clock ----
X(ClockFormat, "Día {0}, {1}", "Day {0}, {1}", "第 {0} 天 {1}", "День {0}, {1}")

// ---- Mods (server/Mods): scripts, plugins and the functions they call ----
X(ModLogLine, "[{0}] {1}", "[{0}] {1}", "[{0}] {1}", "[{0}] {1}")
X(ModHandlerError, "[{0}] error en {1}: {2}", "[{0}] error in {1}: {2}", "[{0}] {1} 出错:{2}", "[{0}] ошибка в {1}: {2}")
X(ModErrorsSilenced, "[{0}] no se registrarán más errores de {1}", "[{0}] further errors of {1} will not be logged",
  "[{0}] {1} 的后续错误将不再记录", "[{0}] дальнейшие ошибки {1} в журнал не попадут")
X(ModDepthLimit, "Demasiados eventos de mods encadenados: '{0}' no se ha repartido",
  "Too many chained mod events: '{0}' was not delivered", "模组事件嵌套过多:“{0}”未被分发",
  "Слишком много вложенных событий модов: «{0}» не доставлено")
X(ModUnknownEvent, "evento desconocido: '{0}' (los eventos propios de los mods llevan ':' en el nombre)",
  "unknown event: '{0}' (the mods' own events have a ':' in their name)",
  "未知事件:“{0}”(模组自定义事件的名称中带有“:”)",
  "неизвестное событие: «{0}» (собственные события модов содержат «:» в имени)")
X(ModNotFound, "No hay ningún mod cargado llamado '{0}'.", "No mod named '{0}' is loaded.", "没有名为“{0}”的已加载模组。",
  "Мод с именем «{0}» не загружен.")
X(ModUnknownKind, "{0}: no es un script (.lua) ni un plugin (.dll, .so)", "{0}: not a script (.lua) or a plugin (.dll, .so)",
  "{0}:既不是脚本(.lua)也不是插件(.dll、.so)", "{0}: не скрипт (.lua) и не плагин (.dll, .so)")
X(ModLoaded, "Mod cargado: {0} ({1})", "Mod loaded: {0} ({1})", "模组已加载:{0}({1})", "Мод загружен: {0} ({1})")
X(ModUnloaded, "Mod descargado: {0}", "Mod unloaded: {0}", "模组已卸载:{0}", "Мод выгружен: {0}")
X(ModLoadFailed, "No se pudo cargar el mod {0}: {1}", "Could not load the mod {0}: {1}", "无法加载模组 {0}:{1}",
  "Не удалось загрузить мод {0}: {1}")
X(ModNameTaken, "ya hay un mod cargado con el nombre '{0}'", "a mod named '{0}' is already loaded",
  "已经加载了名为“{0}”的模组", "мод с именем «{0}» уже загружен")
X(ModFileMissing, "no se encuentra el archivo {0}", "the file {0} was not found", "找不到文件 {0}", "файл {0} не найден")
X(ModBadCommandName, "nombre de comando inválido: '{0}' (de 1 a 24 letras minúsculas, números o _)",
  "invalid command name: '{0}' (1 to 24 lowercase letters, numbers or _)",
  "命令名称无效:“{0}”(1 到 24 个小写字母、数字或下划线)",
  "недопустимое имя команды: «{0}» (от 1 до 24 строчных букв, цифр или _)")
X(ModCommandExists, "el comando /{0} ya existe", "the command /{0} already exists", "命令 /{0} 已存在",
  "команда /{0} уже существует")
X(ModCommandFailed, "El comando ha fallado (el registro del servidor dice por qué).",
  "The command failed (the server's log says why).", "命令执行失败(原因见服务器日志)。",
  "Команда завершилась с ошибкой (причина — в журнале сервера).")
X(PluginOpenFailed, "{0}: no se pudo abrir la librería ({1})", "{0}: the library could not be opened ({1})",
  "{0}:无法打开该库({1})", "{0}: не удалось открыть библиотеку ({1})")
X(PluginNoEntry, "{0}: no es un plugin (no exporta CoopPlugin_Abi y CoopPlugin_Load)",
  "{0}: not a plugin (it does not export CoopPlugin_Abi and CoopPlugin_Load)",
  "{0}:不是插件(未导出 CoopPlugin_Abi 和 CoopPlugin_Load)",
  "{0}: не плагин (не экспортирует CoopPlugin_Abi и CoopPlugin_Load)")
X(PluginAbiMismatch, "{0}: el plugin es de la ABI {1} y este servidor usa la ABI {2}; compílalo con el SDK de este servidor",
  "{0}: the plugin was made for ABI {1} and this server uses ABI {2}; build it with this server's SDK",
  "{0}:该插件针对 ABI {1} 编译,而此服务器使用 ABI {2};请使用此服务器的 SDK 重新编译",
  "{0}: плагин собран для ABI {1}, а этот сервер использует ABI {2}; пересоберите его с SDK этого сервера")
X(PluginLoadRefused, "{0}: el plugin no quiso arrancar (CoopPlugin_Load devolvió 0)",
  "{0}: the plugin refused to start (CoopPlugin_Load returned 0)", "{0}:插件拒绝启动(CoopPlugin_Load 返回 0)",
  "{0}: плагин отказался запускаться (CoopPlugin_Load вернул 0)")
X(PluginBadAnswer, "la respuesta del plugin no es un objeto JSON", "the plugin's answer is not a JSON object",
  "插件的返回值不是 JSON 对象", "ответ плагина не является JSON-объектом")
X(ModsUsage, "/mods", "/mods", "/mods", "/mods")
X(ModsHelp, "mods que tiene cargados el servidor", "mods the server has loaded", "服务器已加载的模组", "моды, загруженные сервером")
X(ModUsage, "/mod reload [nombre] | /mod load <archivo> | /mod unload <nombre>",
  "/mod reload [name] | /mod load <file> | /mod unload <name>", "/mod reload [名称] | /mod load <文件> | /mod unload <名称>",
  "/mod reload [имя] | /mod load <файл> | /mod unload <имя>")
X(ModHelp, "recargar un mod (o todos); cargar y descargar, solo desde la consola",
  "reload a mod (or all of them); load and unload, from the console only", "重新加载某个(或全部)模组;加载和卸载仅限控制台",
  "перезагрузить мод (или все); загрузка и выгрузка — только из консоли")
X(ModsNone, "No hay ningún mod cargado.", "No mod is loaded.", "没有加载任何模组。", "Нет загруженных модов.")
X(ModsTitle, "Mods cargados ({0}):", "Loaded mods ({0}):", "已加载的模组({0}):", "Загруженные моды ({0}):")
X(ModReloaded, "Mod recargado: {0}", "Mod reloaded: {0}", "模组已重新加载:{0}", "Мод перезагружен: {0}")
X(ModReloadedAll, "Mods recargados: {0}", "Mods reloaded: {0}", "已重新加载的模组:{0}", "Перезагружено модов: {0}")
X(ModOnlyConsole, "Solo la consola del servidor puede cargar o descargar mods.",
  "Only the server's console can load or unload mods.", "只有服务器控制台可以加载或卸载模组。",
  "Загружать и выгружать моды может только консоль сервера.")
X(ModsDisabledLog, "Mods desactivados (\"mods.enabled\" es false o se arrancó con --no-mods).",
  "Mods are disabled (\"mods.enabled\" is false or the server was started with --no-mods).",
  "模组已禁用(\"mods.enabled\" 为 false,或启动时使用了 --no-mods)。",
  "Моды отключены («mods.enabled» равно false или сервер запущен с --no-mods).")
X(ModStorageBad, "{0}: los datos guardados no se pueden leer; el mod empieza sin datos",
  "{0}: the stored data cannot be read; the mod starts without data", "{0}:无法读取已保存的数据;模组将从空数据开始",
  "{0}: сохранённые данные не читаются; мод начинает без данных")
X(ModStorageSaveFail, "No se pudieron guardar los datos del mod {0}: {1}", "Could not save the data of the mod {0}: {1}",
  "无法保存模组 {0} 的数据:{1}", "Не удалось сохранить данные мода {0}: {1}")
X(ModDocsWritten, "Referencia de los mods escrita en {0} (API.md, IDS.md).",
  "Mod reference written to {0} (API.md, IDS.md).", "模组参考已写入 {0}(API.md、IDS.md)。",
  "Справочник по модам записан в {0} (API.md, IDS.md).")
X(ModDocsFailed, "No se pudo escribir la referencia de los mods en {0}: {1}",
  "Could not write the mod reference to {0}: {1}", "无法将模组参考写入 {0}:{1}",
  "Не удалось записать справочник по модам в {0}: {1}")
X(RejectByMod, "El servidor no te ha dejado entrar.", "The server did not let you in.", "服务器没有让你进入。",
  "Сервер не пустил вас.")
// Errors of the API's functions (they follow "<function>: ")
X(ApiUnknownFunction, "función desconocida: {0}", "unknown function: {0}", "未知函数:{0}", "неизвестная функция: {0}")
X(ApiArgsNotList, "los argumentos deben ser una lista", "the arguments must be a list", "参数必须是一个列表",
  "аргументы должны быть списком")
X(ApiArgMissing, "falta el argumento {0} ({1})", "argument {0} ({1}) is missing", "缺少第 {0} 个参数({1})",
  "отсутствует аргумент {0} ({1})")
X(ApiArgType, "el argumento {0} ({1}) debe ser {2}", "argument {0} ({1}) must be {2}", "第 {0} 个参数({1})必须是{2}",
  "аргумент {0} ({1}) должен быть {2}")
X(ApiArgRange, "el argumento {0} ({1}) debe estar entre {2} y {3}", "argument {0} ({1}) must be between {2} and {3}",
  "第 {0} 个参数({1})必须在 {2} 到 {3} 之间", "аргумент {0} ({1}) должен быть от {2} до {3}")
X(ApiArgTooLong, "el argumento {0} ({1}) es demasiado largo (máximo {2} bytes)",
  "argument {0} ({1}) is too long ({2} bytes at most)", "第 {0} 个参数({1})过长(最多 {2} 字节)",
  "аргумент {0} ({1}) слишком длинный (не более {2} байт)")
X(ApiArgValue, "{0} debe ser uno de: {1}", "{0} must be one of: {1}", "{0} 必须是以下之一:{1}", "{0} должен быть одним из: {1}")
X(ApiArgEmpty, "el argumento {0} ({1}) está vacío", "argument {0} ({1}) is empty", "第 {0} 个参数({1})为空",
  "аргумент {0} ({1}) пуст")
X(ApiFieldMissing, "falta el campo '{0}'", "the field '{0}' is missing", "缺少字段“{0}”", "отсутствует поле «{0}»")
X(ApiFieldRange, "el campo '{0}' debe ser un número entre {1} y {2}", "the field '{0}' must be a number between {1} and {2}",
  "字段“{0}”必须是 {1} 到 {2} 之间的数字", "поле «{0}» должно быть числом от {1} до {2}")
X(ApiNoPlayer, "no hay ningún jugador conectado '{0}'", "no player '{0}' is connected", "没有在线玩家“{0}”",
  "игрок «{0}» не подключён")
X(ApiUnknownKey, "clave desconocida: '{0}'", "unknown key: '{0}'", "未知的键:“{0}”", "неизвестный ключ: «{0}»")
X(ApiSecretKey, "'{0}' no se puede leer desde un mod", "'{0}' cannot be read from a mod", "模组无法读取“{0}”",
  "«{0}» нельзя прочитать из мода")
X(ApiReadOnlyKey, "'{0}' no se puede cambiar con el servidor en marcha", "'{0}' cannot be changed while the server runs",
  "服务器运行时无法更改“{0}”", "«{0}» нельзя изменить при работающем сервере")
X(ApiTypeInt, "un número entero", "a whole number", "一个整数", "целым числом")
X(ApiTypeNumber, "un número", "a number", "一个数字", "числом")
X(ApiTypeText, "un texto", "a text", "一段文本", "строкой")
X(ApiTypeBool, "true o false", "true or false", "true 或 false", "true или false")
X(ApiTypeTable, "una tabla", "a table", "一个表", "таблицей")
X(ApiTypeList, "una lista", "a list", "一个列表", "списком")
X(ApiTypePlayer, "el id o el nick de un jugador", "a player's id or nick", "玩家的 id 或昵称", "id или ником игрока")
X(StopByMod, "detenido por el mod {0}", "stopped by the mod {0}", "由模组 {0} 关闭", "остановлен модом {0}")
X(ApiBadField, "campo del mundo desconocido: '{0}'", "unknown world field: '{0}'", "未知的世界字段:“{0}”",
  "неизвестное поле мира: «{0}»")
X(ApiFieldKind, "esta función no sirve para el campo '{0}', que es de tipo {1}",
  "this function does not work on the field '{0}', which is of kind {1}", "此函数不适用于类型为 {1} 的字段“{0}”",
  "эта функция не работает с полем «{0}» типа {1}")
X(ApiFieldAlign, "un contador de dos bytes empieza en un byte par", "a two-byte counter starts at an even byte",
  "双字节计数器从偶数字节开始", "двухбайтовый счётчик начинается с чётного байта")
// The world's speed and changes made by the server itself
X(TimeSpeedSuffix, " · velocidad x{0}", " · speed x{0}", " · 速度 x{0}", " · скорость x{0}")
X(TimeSpeedChanged, "La velocidad del tiempo es ahora x{0} ({1}).", "The speed of time is now x{0} ({1}).",
  "时间流速现在是 x{0}({1})。", "Скорость времени теперь x{0} ({1}).")
X(StopFrozen, "detenido a propósito", "stopped on purpose", "已被手动暂停", "остановлено намеренно")
X(WorldOpsOutside, "ese cambio no cabe en el mundo (campo, byte o tipo equivocado)",
  "that change does not fit the world (wrong field, byte or kind)", "该更改不符合世界结构(字段、字节或类型错误)",
  "это изменение не подходит к миру (неверное поле, байт или тип)")
// The games: what the mods order, the settings they force and what the games report
X(InvGameEvent, "aviso del juego inválido", "invalid game report", "无效的游戏报告", "недопустимое сообщение игры")
X(InvStat, "estado del jugador inválido", "invalid player status", "无效的玩家状态", "недопустимое состояние игрока")
X(ApiBadItem, "objeto desconocido: {0}", "unknown item: {0}", "未知的物品:{0}", "неизвестный предмет: {0}")
X(ApiBadActor, "actor desconocido: {0}", "unknown actor: {0}", "未知的角色:{0}", "неизвестный актёр: {0}")
X(ApiBadScene, "escena desconocida: {0}", "unknown scene: {0}", "未知的场景:{0}", "неизвестная сцена: {0}")
X(ApiNotGivable, "{0} no es un objeto que se pueda dar o quitar", "{0} is not an item that can be given or taken",
  "{0} 不是可以给予或收回的物品", "{0} — не предмет, который можно выдать или забрать")
X(ApiNoSpawn, "el actor {0} no se puede crear", "the actor {0} cannot be created", "无法创建角色 {0}",
  "актёра {0} создать нельзя")
X(ApiArgNotZero, "el argumento {0} ({1}) no puede ser 0", "argument {0} ({1}) cannot be 0", "第 {0} 个参数({1})不能为 0",
  "аргумент {0} ({1}) не может быть 0")
X(ApiBadSetting, "la opción '{0}' no se puede forzar (solo opciones de juego: gEnhancements., gCheats., gModes., gFixes.)",
  "the setting '{0}' cannot be forced (gameplay settings only: gEnhancements., gCheats., gModes., gFixes.)",
  "无法强制设置“{0}”(仅限游戏性选项:gEnhancements.、gCheats.、gModes.、gFixes.)",
  "настройку «{0}» нельзя навязать (только игровые: gEnhancements., gCheats., gModes., gFixes.)")
X(ApiBadSettingValue, "el valor de '{0}' debe ser un número entre -1000000 y 1000000, o true/false",
  "the value of '{0}' must be a number between -1000000 and 1000000, or true/false",
  "“{0}”的值必须是 -1000000 到 1000000 之间的数字,或 true/false",
  "значение «{0}» должно быть числом от -1000000 до 1000000 либо true/false")
X(ApiTooManySettings, "demasiadas opciones forzadas (máximo {0} por jugador)", "too many forced settings ({0} per player at most)",
  "强制设置过多(每位玩家最多 {0} 项)", "слишком много навязанных настроек (не более {0} на игрока)")
X(GameSettingRefused, "server.json, gameSettings: {0}", "server.json, gameSettings: {0}", "server.json,gameSettings:{0}",
  "server.json, gameSettings: {0}")

// ---- Game mods (.o2r) the players download (O2rStore.cpp, Handlers/O2rHandlers.cpp) ----
X(O2rShared, "Mod del juego (.o2r) para los jugadores: {0} ({1})", "Game mod (.o2r) for the players: {0} ({1})",
  "提供给玩家的游戏模组(.o2r):{0}({1})", "Мод игры (.o2r) для игроков: {0} ({1})")
X(O2rSkipped, "{0}: no se reparte ({1})", "{0}: not shared ({1})", "{0}:不提供({1})", "{0}: не раздаётся ({1})")
X(O2rWhyUnreadable, "no se puede leer", "it cannot be read", "无法读取", "не удаётся прочитать")
X(O2rWhyEmpty, "está vacío", "it is empty", "文件为空", "файл пуст")
X(O2rWhyTooBig, "pasa de 4 GB", "it is over 4 GB", "超过 4 GB", "больше 4 ГБ")
X(O2rWhyRepeated, "es igual que otro o se llama igual", "it is the same as another one or has the same name",
  "与另一个文件相同或同名", "совпадает с другим файлом или называется так же")
X(O2rTooMany, "Solo se reparten los primeros {0} archivos .o2r", "Only the first {0} .o2r files are shared",
  "只提供前 {0} 个 .o2r 文件", "Раздаются только первые {0} файлов .o2r")
X(O2rInScriptsDir, "Hay archivos .o2r en '{0}': los mods del juego van en '{1}' (server.json \"o2r\")",
  "There are .o2r files in '{0}': game mods go in '{1}' (server.json \"o2r\")",
  "'{0}' 中有 .o2r 文件:游戏模组应放在 '{1}'(server.json \"o2r\")",
  "В '{0}' есть файлы .o2r: моды игры кладутся в '{1}' (server.json \"o2r\")")
X(O2rModsTitle, "Mods del juego (.o2r) que se descargan los jugadores ({0}):", "Game mods (.o2r) the players download ({0}):",
  "玩家需下载的游戏模组(.o2r)({0}):", "Моды игры (.o2r), которые скачивают игроки ({0}):")
X(O2rReadFailed, "No se pudo leer {0} para enviarlo", "Could not read {0} to send it", "无法读取 {0} 以发送",
  "Не удалось прочитать {0} для отправки")
X(O2rReadFailedPlayer, "El servidor no pudo leer el mod {0}. Avisa al administrador.",
  "The server could not read the mod {0}. Tell the admin.", "服务器无法读取模组 {0}。请告诉管理员。",
  "Сервер не смог прочитать мод {0}. Сообщите администратору.")
X(O2rEnterMissing, "Tu juego no tiene cargados los mods del servidor (.o2r): vuelve a conectarte para descargarlos.",
  "Your game has not loaded the server's mods (.o2r): reconnect to download them.",
  "你的游戏没有加载服务器的模组(.o2r):请重新连接以下载。",
  "Ваша игра не загрузила моды сервера (.o2r): переподключитесь, чтобы скачать их.")
X(O2rEnterMissingLog, "{0} intentó entrar en la partida sin los mods del servidor (.o2r)",
  "{0} tried to enter the world without the server's mods (.o2r)", "{0} 试图在没有服务器模组(.o2r)的情况下进入世界",
  "{0} пытался войти в мир без модов сервера (.o2r)")
X(InvO2rGet, "petición o2r_get inválida", "invalid o2r_get request", "无效的 o2r_get 请求", "неверный запрос o2r_get")
X(InvO2rFlood, "demasiadas peticiones o2r_get", "too many o2r_get requests", "o2r_get 请求过多",
  "слишком много запросов o2r_get")

// ---- Base-game saves into the co-op (common/SaveImport.cpp) ----
X(ImpNotSave, "No es una partida de 2 Ship (no tiene newCycleSave ni owlSave).",
  "This is not a 2 Ship save (it has neither newCycleSave nor owlSave).",
  "这不是 2 Ship 的存档(没有 newCycleSave 或 owlSave)。", "Это не сохранение 2 Ship (нет ни newCycleSave, ни owlSave).")
X(ImpEmpty, "La partida está vacía o borrada.", "The save is empty or erased.", "该存档为空或已被删除。",
  "Сохранение пустое или удалено.")
X(ImpRando, "Las partidas del randomizer no se pueden usar en el co-op.", "Randomizer saves cannot be used in the co-op.",
  "随机模式(randomizer)的存档无法在合作模式中使用。", "Сохранения рандомайзера (randomizer) нельзя использовать в кооперативе.")
X(ImpNoOwl, "Esta partida no tiene guardado de búho.", "This save has no owl save.", "此存档没有猫头鹰存档。",
  "В этом сохранении нет сохранения у совы.")
X(ImpNoCycle, "Esta partida no tiene guardado de inicio de ciclo.", "This save has no start-of-cycle save.",
  "此存档没有周期开始时的存档。", "В этом сохранении нет сохранения начала цикла.")
X(ImpMissing, "Partida dañada: falta o no vale '{0}'.", "Damaged save: '{0}' is missing or invalid.",
  "存档已损坏:“{0}”缺失或无效。", "Сохранение повреждено: «{0}» отсутствует или недопустимо.")
X(ImpNewer, "La partida es de una versión más nueva de 2 Ship ({0}); se convierte igualmente.",
  "The save comes from a newer 2 Ship version ({0}); it is converted anyway.",
  "该存档来自更新的 2 Ship 版本({0});仍会进行转换。",
  "Сохранение из более новой версии 2 Ship ({0}); оно всё равно будет преобразовано.")
X(ImpSumName, "Partida de {0} (versión {1}): {2}.", "{0}'s save (version {1}): {2}.", "{0} 的存档(版本 {1}):{2}。",
  "Сохранение {0} (версия {1}): {2}.")
X(ImpSourceOwl, "guardado de búho, {0}", "owl save, {0}", "猫头鹰存档,{0}", "сохранение у совы, {0}")
X(ImpSourceCycle, "inicio del ciclo (Amanecer del Primer Día)", "start of the cycle (Dawn of the First Day)",
  "周期开始(第一天的黎明)", "начало цикла (Рассвет первого дня)")
X(ImpSumStats, "Corazones: {0} (+{1}/4) · Máscaras: {2}/24 · Canciones: {3}/10 · Restos: {4}/4 · Ciclos: {5}",
  "Hearts: {0} (+{1}/4) · Masks: {2}/24 · Songs: {3}/10 · Remains: {4}/4 · Cycles: {5}",
  "心:{0}(+{1}/4)· 面具:{2}/24 · 歌曲:{3}/10 · 遗骸:{4}/4 · 周期:{5}",
  "Сердца: {0} (+{1}/4) · Маски: {2}/24 · Песни: {3}/10 · Останки: {4}/4 · Циклы: {5}")
X(ImpSumAdded, "Añadido para que funcione en el co-op: {0}.", "Added so it works in the co-op: {0}.",
  "为使其在合作模式中正常运行而添加:{0}。", "Добавлено, чтобы всё работало в кооперативе: {0}.")
X(ImpAddTatl, "Tatl", "Tatl", "Tatl", "Татл")
X(ImpAddOcarina, "Ocarina del Tiempo", "Ocarina of Time", "时之笛", "Окарина времени")
X(ImpAddDekuNut, "nueces Deku", "Deku Nuts", "德库坚果", "орехи Деку")
X(ImpAddDekuMask, "Máscara Deku", "Deku Mask", "德库面具", "маска Деку")
X(ImpAddSongTime, "Canción del Tiempo", "Song of Time", "时之歌", "Песнь времени")
X(ImpAddSongHealing, "Canción de Curación", "Song of Healing", "治愈之歌", "Песнь исцеления")
X(ImpAddMagic, "magia", "magic", "魔力", "магия")
X(ImpAddReset, "un ciclo completado", "one cycle done", "已完成一个周期", "один пройденный цикл")
X(ImpAddIntro, "la introducción vista", "the intro seen", "已看过的开场", "просмотренное вступление")

// ---- Saving: backups, recovery, import and restore (World/SharedWorld.cpp, World/WorldBackups.cpp,
//      Commands/SaveCommands.cpp) ----
X(WorldNoBackup, "No hay copias de seguridad: el primer jugador que entre creará un mundo nuevo.",
  "There are no backups: the first player to enter will create a new world.", "没有备份:第一个进入的玩家将创建新世界。",
  "Резервных копий нет: первый вошедший игрок создаст новый мир.")
X(WorldRecovered, "Se ha recuperado el mundo de la copia de seguridad {0}.", "The world was recovered from the backup {0}.",
  "已从备份 {0} 恢复世界。", "Мир восстановлен из резервной копии {0}.")
X(WorldRecoverFail, "No se pudo recuperar la copia de seguridad {0}: {1}", "Could not recover the backup {0}: {1}",
  "无法恢复备份 {0}:{1}", "Не удалось восстановить резервную копию {0}: {1}")
X(WorldBlocked,
  "No se puede leer ni apartar {0} ({1}): el servidor no creará un mundo nuevo encima. Ciérralo en los demás programas y reinicia el servidor.",
  "{0} can be neither read nor moved aside ({1}): the server will not create a new world over it. Close it in other programs and restart the server.",
  "无法读取也无法移走 {0}({1}):服务器不会在其上创建新世界。请在其他程序中关闭它并重启服务器。",
  "{0} нельзя ни прочитать, ни переместить ({1}): сервер не создаст поверх него новый мир. Закройте его в других программах и перезапустите сервер.")
X(WorldBlockedEnter, "El mundo guardado del servidor no se puede leer: avisa al administrador.",
  "The server's saved world cannot be read: tell the admin.", "无法读取服务器保存的世界:请告诉管理员。",
  "Сохранённый мир сервера не читается: сообщите администратору.")
X(ServerLockOther,
  "Otro servidor (proceso {0}) parece estar usando esta carpeta: su server.lock se queda como está. Dos servidores con los mismos archivos se pisan las partidas guardadas.",
  "Another server (process {0}) seems to be using this folder: its server.lock is left as it is. Two servers on the same files overwrite each other's saves.",
  "另一个服务器(进程 {0})似乎正在使用此文件夹:其 server.lock 保持不变。两个服务器使用同一组文件会互相覆盖存档。",
  "Похоже, эту папку использует другой сервер (процесс {0}): его server.lock оставлен как есть. Два сервера с одними и теми же файлами перезаписывают сохранения друг друга.")
X(PlayerFileSetAside, "players/{0} no es válido; se ha apartado como {1}", "players/{0} is not valid; it was moved aside as {1}",
  "players/{0} 无效;已移至 {1}", "players/{0} недопустим; перемещён в {1}")
X(BackupMade, "Copia de seguridad guardada: {0}", "Backup saved: {0}", "备份已保存:{0}", "Резервная копия сохранена: {0}")
X(BackupFail, "No se pudo hacer la copia de seguridad: {0}", "Could not make the backup: {0}", "无法创建备份:{0}",
  "Не удалось сделать резервную копию: {0}")
X(BackupOff, "Este servidor no guarda archivos: no hay copias de seguridad.",
  "This server keeps no files: there are no backups.", "此服务器不保存文件:没有备份。",
  "Этот сервер не хранит файлы: резервных копий нет.")
X(BackupUsage, "/backup [lista]", "/backup [list]", "/backup [list]", "/backup [list]")
X(BackupHelp, "guardar ahora una copia de seguridad del mundo, o ver la lista de copias",
  "save a backup of the world now, or see the list of backups", "立即保存世界备份,或查看备份列表",
  "сохранить резервную копию мира сейчас или посмотреть список копий")
X(BackupListHead, "Copias de seguridad (de la más nueva a la más vieja):", "Backups (newest first):", "备份(从新到旧):",
  "Резервные копии (сначала новые):")
X(BackupNone, "No hay copias de seguridad.", "There are no backups.", "没有备份。", "Резервных копий нет.")
X(BackupNotFound, "No hay ninguna copia de seguridad llamada '{0}'. Usa /backup lista.",
  "There is no backup called '{0}'. Use /backup list.", "没有名为“{0}”的备份。请使用 /backup list。",
  "Резервной копии «{0}» нет. Используйте /backup list.")
X(RestoreUsage, "/restaurar <copia|ultima>", "/restore <backup|last>", "/restore <备份|last>", "/restore <копия|last>")
X(RestoreHelp, "poner en marcha una copia de seguridad del mundo (todos vuelven a entrar)",
  "put a backup of the world in place (everyone enters again)", "启用一份世界备份(所有人重新进入)",
  "вернуть резервную копию мира (все входят заново)")
X(ImportUsage, "/importar <archivo> [nick] [auto|buho|ciclo]", "/import <file> [nick] [auto|owl|cycle]",
  "/import <文件> [昵称] [auto|owl|cycle]", "/import <файл> [ник] [auto|owl|cycle]")
X(ImportHelp, "convertir una partida del juego base (saves/fileN.json) y ponerla como mundo del servidor",
  "convert a base-game save (saves/fileN.json) and make it the server's world",
  "转换基础游戏的存档(saves/fileN.json)并将其设为服务器的世界",
  "преобразовать сохранение базовой игры (saves/fileN.json) и сделать его миром сервера")
X(ImportNeedNick, "Falta el nick del jugador: el nombre de la partida ('{0}') no vale como nick.",
  "The player's nick is missing: the save's name ('{0}') is not a valid nick.",
  "缺少玩家昵称:存档名称(“{0}”)不能作为昵称。", "Не указан ник игрока: имя в сохранении («{0}») не подходит как ник.")
X(ImportBadSource, "Parte desconocida '{0}': usa auto, buho o ciclo.", "Unknown part '{0}': use auto, owl or cycle.",
  "未知部分“{0}”:请使用 auto、owl 或 cycle。", "Неизвестная часть «{0}»: используйте auto, owl или cycle.")
X(ImportDone, "Partida importada para {0}.", "Save imported for {0}.", "已为 {0} 导入存档。", "Сохранение импортировано для {0}.")
X(ReplaceBusy, "Ahora no: el mundo se está creando o reiniciando.", "Not now: the world is being created or reset.",
  "现在不行:世界正在创建或重置。", "Не сейчас: мир создаётся или перезапускается.")
X(WorldImported, "{0} ha cargado una partida convertida del juego base ({1}): todos vuelven a entrar.",
  "{0} loaded a save converted from the base game ({1}): everyone enters again.",
  "{0} 载入了从基础游戏转换的存档({1}):所有人重新进入。",
  "{0} загрузил сохранение, преобразованное из базовой игры ({1}): все входят заново.")
X(WorldRestored, "{0} ha restaurado la copia de seguridad {1}: todos vuelven a entrar.",
  "{0} restored the backup {1}: everyone enters again.", "{0} 恢复了备份 {1}:所有人重新进入。",
  "{0} восстановил резервную копию {1}: все входят заново.")
X(DescBackups, "\nCopias de seguridad: {0}", "\nBackups: {0}", "\n备份:{0}", "\nРезервные копии: {0}")
X(NotInWorld, "{0} no está en la partida del servidor: tiene que entrar primero.",
  "{0} is not in the server's world: they must enter first.", "{0} 不在服务器的世界中:需要先进入。",
  "{0} не в мире сервера: сначала нужно войти.")

// ---- Converter tool (tools/SaveConvert.cpp: 2ship-coop-convert) ----
X(CvUsage,
  "Uso: 2ship-coop-convert <partida.json> [opciones]\n  --nick N            nick del jugador dueño de la partida\n  --out CARPETA       carpeta del servidor (por defecto, la de este programa)\n  --source auto|owl|cycle  guardado de búho o inicio del ciclo\n  --player-only       solo la ficha del jugador (el mundo no se toca)\n  --info              solo enseña el resumen\n  --yes               no pregunta nada\n  --force             aunque el servidor esté en marcha\n  --lang es|en|zh|ru\nTambién puedes arrastrar la partida (saves/file1.json) encima de este programa.",
  "Usage: 2ship-coop-convert <save.json> [options]\n  --nick N            nick of the player who owns the save\n  --out FOLDER        the server's folder (by default, this program's)\n  --source auto|owl|cycle  owl save or start of the cycle\n  --player-only       only the player's file (the world is left alone)\n  --info              only show the summary\n  --yes               ask nothing\n  --force             even if the server is running\n  --lang es|en|zh|ru\nYou can also drag the save (saves/file1.json) onto this program.",
  "用法:2ship-coop-convert <存档.json> [选项]\n  --nick N            存档所属玩家的昵称\n  --out 文件夹         服务器文件夹(默认为本程序所在文件夹)\n  --source auto|owl|cycle  猫头鹰存档或周期开始\n  --player-only       只写玩家文件(不动世界)\n  --info              只显示摘要\n  --yes               不询问任何问题\n  --force             即使服务器正在运行\n  --lang es|en|zh|ru\n也可以把存档(saves/file1.json)拖到本程序上。",
  "Использование: 2ship-coop-convert <сохранение.json> [параметры]\n  --nick N            ник игрока, которому принадлежит сохранение\n  --out ПАПКА         папка сервера (по умолчанию папка этой программы)\n  --source auto|owl|cycle  сохранение у совы или начало цикла\n  --player-only       только файл игрока (мир не трогается)\n  --info              только показать сводку\n  --yes               ничего не спрашивать\n  --force             даже если сервер запущен\n  --lang es|en|zh|ru\nМожно также перетащить сохранение (saves/file1.json) на эту программу.")
X(CvReading, "Leyendo {0}...", "Reading {0}...", "正在读取 {0}……", "Чтение {0}...")
X(CvAskNick, "Nick del jugador dueño de esta partida [{0}]: ", "Nick of the player who owns this save [{0}]: ",
  "此存档所属玩家的昵称 [{0}]:", "Ник игрока, которому принадлежит это сохранение [{0}]: ")
X(CvAskReplace, "Ya hay un mundo en {0}: se guardará una copia en backups/ y se reemplazará. ¿Seguir? (s/n): ",
  "There is already a world in {0}: a copy goes to backups/ and it will be replaced. Continue? (y/n): ",
  "{0} 中已有一个世界:将在 backups/ 中保存副本并替换它。继续吗?(y/n):",
  "В {0} уже есть мир: копия попадёт в backups/, а мир будет заменён. Продолжить? (y/n): ")
X(CvNeedYes, "Ya hay un mundo en {0}: añade --yes para reemplazarlo (antes se guarda una copia en backups/).",
  "There is already a world in {0}: add --yes to replace it (a copy goes to backups/ first).",
  "{0} 中已有一个世界:添加 --yes 以替换它(会先在 backups/ 中保存副本)。",
  "В {0} уже есть мир: добавьте --yes, чтобы заменить его (сначала копия попадёт в backups/).")
X(CvCancelled, "Cancelado: no se ha cambiado nada.", "Cancelled: nothing was changed.", "已取消:没有任何更改。",
  "Отменено: ничего не изменено.")
X(CvServerRunning,
  "El servidor está en marcha en {0} (proceso {1}): ciérralo antes, o usa /importar en su consola (o --force).",
  "The server is running in {0} (process {1}): close it first, or use /import in its console (or --force).",
  "服务器正在 {0} 中运行(进程 {1}):请先关闭它,或在其控制台中使用 /import(或 --force)。",
  "Сервер запущен в {0} (процесс {1}): сначала закройте его или используйте /import в его консоли (или --force).")
X(CvWritten, "Escrito: {0}", "Written: {0}", "已写入:{0}", "Записано: {0}")
X(CvDone, "Listo: arranca el servidor y entrad en su partida.", "Done: start the server and enter its world.",
  "完成:启动服务器并进入它的世界。", "Готово: запустите сервер и войдите в его мир.")
X(CvDonePlayer, "Listo: ficha de {0} preparada para el mundo actual (ciclo {1}).",
  "Done: {0}'s file is ready for the current world (cycle {1}).", "完成:{0} 的文件已为当前世界准备好(第 {1} 个周期)。",
  "Готово: файл {0} готов для текущего мира (цикл {1}).")
X(CvPressEnter, "Pulsa Intro para salir.", "Press Enter to exit.", "按回车键退出。", "Нажмите Enter для выхода.")
X(CvNeedWorld, "--player-only necesita un world.json válido en {0}: {1}", "--player-only needs a valid world.json in {0}: {1}",
  "--player-only 需要 {0} 中有一个有效的 world.json:{1}", "--player-only требует корректный world.json в {0}: {1}")
X(CvBackup, "Copia de seguridad de lo que había: {0}", "Backup of what was there: {0}", "原有内容的备份:{0}",
  "Резервная копия прежнего: {0}")
X(CvBadArg, "Argumento desconocido: {0}", "Unknown argument: {0}", "未知参数:{0}", "Неизвестный аргумент: {0}")
X(CvNoFile, "No se puede leer {0}: {1}", "Cannot read {0}: {1}", "无法读取 {0}:{1}", "Не удаётся прочитать {0}: {1}")
X(CvFailed, "No se pudo escribir: {0}", "Could not write: {0}", "无法写入:{0}", "Не удалось записать: {0}")

// ---- Shared drops (Handlers/ItemHandlers.cpp) ----
X(LogItemDropped, "Objeto soltado por {0}: escena {1}, clave {2}", "Item dropped by {0}: scene {1}, key {2}",
  "{0} 掉落了物品:场景 {1},键 {2}", "{0} уронил предмет: сцена {1}, ключ {2}")
X(LogItemTaken, "{0} cogió el objeto {2} de la escena {1}", "{0} picked up item {2} in scene {1}",
  "{0} 拾取了场景 {1} 的物品 {2}", "{0} подобрал предмет {2} в сцене {1}")
