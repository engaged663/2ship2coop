// [COOP] Every co-op text of the game, in the server's four languages: X(Id, es, en, zh, ru). See CoopText.h.
// Placeholders: {0} {1} {2}... are replaced by the arguments of Tx(Txt::Id, { ... }); every language uses the same
// ones (coop-tests checks it). To add a text: one line here and Tx/Tc(Txt::Id) where it is shown.
// Chinese tooltips get their line breaks by themselves (Tip); a long Chinese label may carry its own "\n".

// ---- Language (I18n/CoopText.cpp) ----
X(LangLabel, "Idioma", "Language", "语言", "Язык")
X(LangAuto, "Automático: {0}", "Automatic: {0}", "自动：{0}", "Автоматически: {0}")
X(LangTip,
  "Idioma de los textos del co-op en tu juego: este menú, las ventanas y los avisos del chat. Automático: el del "
  "servidor mientras estás conectado (lo cambia /lang) y el de tu sistema si no. Lo que dice el servidor siempre "
  "llega en su idioma.",
  "Language of the co-op texts in your game: this menu, the windows and the chat notices. Automatic: the server's "
  "while you are connected (changed with /lang), otherwise your system's. What the server says always comes in its "
  "own language.",
  "游戏中联机文字的语言：本菜单、各个窗口和聊天提示。自动：连接时使用服务器的语言（可用 /lang 更改），未连接时使用系统语言。"
  "服务器发送的消息始终使用服务器的语言。",
  "Язык текстов кооператива в вашей игре: это меню, окна и сообщения чата. Автоматически: язык сервера, пока вы "
  "подключены (меняется командой /lang), иначе язык системы. То, что пишет сервер, всегда приходит на его языке.")
X(LangNoChineseFont, "No se ha encontrado ninguna fuente china en el sistema: el chino se verá con '?'.",
  "No Chinese font was found on this system: Chinese text will show as '?'. Put the path of a .ttf/.ttc in "
  "gCoop.ChineseFont.",
  "系统中未找到中文字体：中文会显示为“?”。", "В системе не найден китайский шрифт: китайский текст будет показан как «?».")

// ---- Shared words ----
X(Cancel, "Cancelar", "Cancel", "取消", "Отмена")
X(Accept, "Aceptar", "Accept", "接受", "Принять")
X(Decline, "Rechazar", "Decline", "拒绝", "Отклонить")
X(Invite, "Invitar", "Invite", "邀请", "Пригласить")
X(TagYou, "(tú)", "(you)", "（你）", "(вы)")
X(TagLeader, "(líder)", "(leader)", "（队长）", "(лидер)")
X(TagHost, "(anfitrión)", "(host)", "（房主）", "(хозяин)")
X(TagPlays, "(juega)", "(plays)", "（游玩中）", "(играет)")
X(NotConnected, "No estás conectado a ningún servidor.", "You are not connected to any server.", "你没有连接到任何服务器。",
  "Вы не подключены ни к одному серверу.")
X(OnlyInWorld, "Solo dentro de la partida del servidor.", "Only inside the server's game.", "仅限在服务器的游戏中。",
  "Только внутри игры сервера.")
X(TimeSuffix, " ({0})", " ({0})", "（{0}）", " ({0})")
X(Points, "{0} puntos", "{0} points", "{0} 分", "очков: {0}")

// ---- Co-op menu (Menu/CoopMenu.cpp) ----
X(MenuConnection, "Conexión", "Connection", "连接", "Подключение")
X(MenuNick, "Nick (3-16 letras, números o _)", "Nick (3-16 letters, digits or _)", "昵称（3-16 个字母、数字或 _）",
  "Ник (3-16 латинских букв, цифр или _)")
X(MenuHost, "Servidor (IP o dominio)", "Server (IP or domain)", "服务器（IP 或域名）", "Сервер (IP или домен)")
X(MenuPort, "Puerto (UDP)", "Port (UDP)", "端口（UDP）", "Порт (UDP)")
X(MenuPassword, "Contraseña del servidor (opcional)", "Server password (optional)", "服务器密码（可选）",
  "Пароль сервера (необязательно)")
X(MenuConnect, "Conectar", "Connect", "连接", "Подключиться")
X(MenuDisconnect, "Desconectar", "Disconnect", "断开连接", "Отключиться")
X(StatusDisconnected, "Estado: desconectado", "Status: disconnected", "状态：未连接", "Состояние: не подключено")
X(StatusConnecting, "Estado: conectando con {0}...", "Status: connecting to {0}...", "状态：正在连接 {0}...",
  "Состояние: подключение к {0}...")
X(StatusHandshaking, "Estado: identificándose...", "Status: identifying...", "状态：正在验证身份...",
  "Состояние: идентификация...")
X(StatusConnected, "Estado: conectado a {0} como {1} (ping {2} ms)", "Status: connected to {0} as {1} (ping {2} ms)",
  "状态：已以 {1} 的身份连接到 {0}（延迟 {2} ms）", "Состояние: подключено к {0} как {1} (пинг {2} мс)")
X(MenuPlayers, "Jugadores conectados", "Connected players", "在线玩家", "Игроки на сервере")
X(MenuWorld, "Partida del servidor", "The server's game", "服务器的游戏", "Игра сервера")
X(WorldEnter, "Entrar en la partida del servidor", "Enter the server's game", "进入服务器的游戏", "Войти в игру сервера")
X(WorldEnterTip,
  "Deja la partida que tengas abierta (sin guardarla) y entra en el mundo compartido del servidor.",
  "Leaves the game you have open (without saving it) and enters the server's shared world.",
  "离开当前打开的游戏（不保存），进入服务器的共享世界。",
  "Закрывает открытую игру (без сохранения) и входит в общий мир сервера.")
X(WorldLeave, "Salir de la partida del servidor", "Leave the server's game", "离开服务器的游戏", "Выйти из игры сервера")
X(WorldLeaveTip, "Tu progreso queda en el servidor. Vuelves a la selección de archivo.",
  "Your progress stays on the server. You go back to the file select screen.",
  "你的进度保存在服务器上。你将回到存档选择界面。", "Ваш прогресс остаётся на сервере. Вы вернётесь к выбору файла.")
X(AutoEnter, "Entrar automáticamente al conectar", "Enter automatically when connecting", "连接后自动进入",
  "Входить автоматически при подключении")
X(AutoEnterTip,
  "Al conectarte desde la pantalla de título o la selección de archivo entras directamente en la partida del "
  "servidor. Allí tu progreso se guarda en el servidor, nunca en tus archivos de guardado.",
  "When you connect from the title screen or the file select screen you go straight into the server's game. There "
  "your progress is saved on the server, never in your save files.",
  "在标题画面或存档选择界面连接时，直接进入服务器的游戏。在那里，你的进度保存在服务器上，绝不会写入你的存档文件。",
  "При подключении с титульного экрана или экрана выбора файла вы сразу входите в игру сервера. Там ваш прогресс "
  "сохраняется на сервере, а не в ваших файлах сохранения.")
X(MenuOptions, "Opciones", "Options", "选项", "Настройки")
X(AutoConnect, "Conectar automáticamente al abrir el juego", "Connect automatically when the game starts",
  "启动游戏时自动连接", "Подключаться автоматически при запуске игры")
X(ShowOwnName, "Mostrar mi propio nick encima de mi Link", "Show my own nick above my Link", "在我的林克头上显示我的昵称",
  "Показывать мой ник над моим Линком")
X(LiveMenu, "El inventario no pausa el juego (partida del servidor)",
  "The inventory does not pause the game (server's game)", "物品栏不暂停游戏（服务器的游戏）",
  "Инвентарь не ставит игру на паузу (игра сервера)")
X(LiveMenuTip,
  "Con el menú de pausa abierto el mundo sigue en directo detrás: Link, la cámara, el sonido y los demás jugadores. "
  "Link no recibe los botones del menú, pero puede caer o recibir daño. El menú se cierra solo si el juego necesita "
  "a Link (un diálogo, una cinemática, cambiar de escena, morir). Desactivado: el menú sobre una imagen fija y Link "
  "esperando, como antes. Se aplica la próxima vez que abras el menú.",
  "With the pause menu open the world goes on live behind it: Link, the camera, the sound and the other players. "
  "Link does not get the menu's buttons, but can fall or take damage. The menu closes by itself if the game needs "
  "Link (a dialogue, a cutscene, a scene change, dying). Off: the menu over a still picture with Link waiting, as "
  "before. It applies the next time you open the menu.",
  "打开暂停菜单时，后面的世界仍在实时运行：林克、镜头、声音和其他玩家。林克不会接收菜单的按键，但可能掉落或受到伤害。"
  "如果游戏需要林克（对话、过场动画、切换场景、死亡），菜单会自动关闭。关闭此项：菜单显示在静止画面上，林克原地等待，"
  "和以前一样。下次打开菜单时生效。",
  "Когда открыто меню паузы, мир за ним продолжает жить: Линк, камера, звук и другие игроки. Линк не получает "
  "нажатия кнопок меню, но может упасть или получить урон. Меню закрывается само, если игре нужен Линк (диалог, "
  "сцена, смена локации, смерть). Выключено: меню поверх застывшей картинки, Линк ждёт, как раньше. Применяется при "
  "следующем открытии меню.")
X(ModsAllow, "Permitir los mods del servidor (partida del servidor)", "Allow the server's mods (server's game)",
  "允许服务器的模组（服务器的游戏）", "Разрешить моды сервера (игра сервера)")
X(ModsAllowTip,
  "Los scripts del servidor pueden darte objetos, curarte o hacerte daño, mostrarte avisos y mensajes, crear "
  "enemigos, llevarte a otra escena y cambiar opciones de juego de 2 Ship (dificultad, trucos) mientras juegas en su "
  "partida; al salir recuperas tus opciones. Nunca tocan tus archivos de guardado. Desactivado: el juego ignora sus "
  "órdenes y opciones y no les cuenta lo que pasa en tu partida.",
  "The server's scripts can give you items, heal or hurt you, show you notices and messages, spawn enemies, take you "
  "to another scene and change 2 Ship game options (difficulty, cheats) while you play in its game; when you leave "
  "you get your options back. They never touch your save files. Off: the game ignores their orders and options and "
  "does not tell them what happens in your game.",
  "服务器的脚本可以在你游玩其游戏时给你物品、治疗或伤害你、向你显示提示和消息、生成敌人、把你带到其他场景，以及更改 2 Ship "
  "的游戏选项（难度、作弊）；离开后你的选项会恢复。它们绝不会改动你的存档文件。关闭此项：游戏会忽略它们的指令和选项，"
  "也不会告诉它们你的游戏中发生了什么。",
  "Скрипты сервера могут выдавать вам предметы, лечить или ранить вас, показывать уведомления и сообщения, создавать "
  "врагов, переносить вас в другую сцену и менять настройки игры 2 Ship (сложность, читы), пока вы играете в его "
  "игре; при выходе ваши настройки возвращаются. Ваши файлы сохранения они не трогают никогда. Выключено: игра "
  "игнорирует их команды и настройки и не сообщает им, что происходит в вашей игре.")
X(ChatScale, "Tamaño del chat", "Chat size", "聊天文字大小", "Размер чата")
X(ChatOpacity, "Opacidad del fondo del chat", "Chat background opacity", "聊天背景不透明度", "Непрозрачность фона чата")
X(ChatHelp,
  "Chat: Enter para escribir, / para comandos (/help), Esc para cerrar. Los otros Links solo se ven cuando estáis en "
  "el mismo escenario.",
  "Chat: Enter to type, / for commands (/help), Esc to close. The other Links are only visible when you are in the "
  "same scene.",
  "聊天：按 Enter 输入，按 / 输入命令（/help），按 Esc 关闭。只有在同一场景中才能看到其他林克。",
  "Чат: Enter — написать, / — команды (/help), Esc — закрыть. Других Линков видно, только когда вы в одной сцене.")
X(MenuTesting, "Pruebas", "Testing", "测试", "Тестирование")
X(BootClockTown, "Arrancar directamente en Ciudad Reloj (partida de prueba)", "Start straight in Clock Town (test game)",
  "直接在时钟镇启动（测试存档）", "Запускать сразу в Часовом городе (тестовая игра)")
X(BootClockTownTip,
  "Al abrir el juego entra en Ciudad Reloj Sur con la partida de depuración (todos los objetos y máscaras). Esa "
  "partida nunca se guarda. Desactívalo para jugar con tus archivos normales.",
  "When the game starts it enters South Clock Town with the debug save (every item and mask). That game is never "
  "saved. Turn it off to play with your normal files.",
  "启动游戏时使用调试存档（全部物品和面具）进入南时钟镇。该存档永远不会保存。关闭此项即可使用你的普通存档游玩。",
  "При запуске игра входит в Южный Часовой город с отладочным сохранением (все предметы и маски). Эта игра никогда "
  "не сохраняется. Выключите, чтобы играть со своими обычными файлами.")

// ---- Connection (Client/NetClient.cpp, Client/Session.cpp) ----
X(NetLost, "Se ha perdido la conexión con el servidor.", "The connection to the server was lost.", "与服务器的连接已断开。",
  "Соединение с сервером потеряно.")
X(NetCantConnect, "No se pudo conectar con el servidor.", "Could not connect to the server.", "无法连接到服务器。",
  "Не удалось подключиться к серверу.")
X(NetTimeout, "No se pudo conectar con el servidor (tiempo agotado). Revisa la IP, el puerto y el firewall.",
  "Could not connect to the server (timed out). Check the IP, the port and the firewall.",
  "无法连接到服务器（超时）。请检查 IP、端口和防火墙。",
  "Не удалось подключиться к серверу (время ожидания истекло). Проверьте IP, порт и брандмауэр.")
X(NetYouLeft, "Te has desconectado del servidor.", "You disconnected from the server.", "你已断开与服务器的连接。",
  "Вы отключились от сервера.")
X(SessionConnected, "Conectado a {0} como {1}", "Connected to {0} as {1}", "已以 {1} 的身份连接到 {0}",
  "Подключено к {0} как {1}")
X(SessionBadNick, "Elige un nick válido (3-16 letras, números o _) en el menú Co-op.",
  "Choose a valid nick (3-16 letters, digits or _) in the Co-op menu.", "请在 Co-op 菜单中设置有效的昵称（3-16 个字母、数字或 _）。",
  "Выберите допустимый ник (3-16 латинских букв, цифр или _) в меню Co-op.")
X(SessionBadHost, "Revisa la IP y el puerto del servidor en el menú Co-op.",
  "Check the server's IP and port in the Co-op menu.", "请在 Co-op 菜单中检查服务器的 IP 和端口。",
  "Проверьте IP и порт сервера в меню Co-op.")

// ---- Chat (Chat/ChatModel.cpp) ----
X(ChatPmTo, "[tú -> {0}] {1}", "[you -> {0}] {1}", "[你 -> {0}] {1}", "[вы -> {0}] {1}")
X(ChatPmFrom, "[{0} -> tú] {1}", "[{0} -> you] {1}", "[{0} -> 你] {1}", "[{0} -> вы] {1}")
X(ChatJoined, "{0} se ha unido a la partida.", "{0} joined the game.", "{0} 加入了游戏。", "{0} присоединился к игре.")
X(ChatLeft, "{0} ha salido de la partida.", "{0} left the game.", "{0} 离开了游戏。", "{0} покинул игру.")
X(ChatLeftReason, "{0} ha salido de la partida ({1}).", "{0} left the game ({1}).", "{0} 离开了游戏（{1}）。",
  "{0} покинул игру ({1}).")

// ---- The server's game (World/WorldSession.cpp, World/ClockSync.cpp) ----
X(WorldBad, "El mundo del servidor no es válido ({0}).", "The server's world is not valid ({0}).",
  "服务器的世界数据无效（{0}）。", "Мир сервера повреждён ({0}).")
X(WorldSword, "La Canción del Tiempo ha devuelto a la partida la espada que robó Takkuri.",
  "The Song of Time gave back to the game the sword Takkuri stole.", "时之歌把 Takkuri 偷走的剑还给了这局游戏。",
  "Песнь времени вернула в игру меч, украденный Таккури.")
X(WorldStale, "Tu inventario era de un ciclo anterior: vuelves con lo que conserva la Canción del Tiempo.",
  "Your inventory was from an earlier cycle: you come back with what the Song of Time keeps.",
  "你的物品栏来自之前的循环：你带着时之歌会保留的东西回来了。",
  "Ваш инвентарь был из прошлого цикла: вы возвращаетесь с тем, что сохраняет Песнь времени.")
X(WorldIn, "Estás en la partida del servidor. /tiempo muestra el reloj.",
  "You are in the server's game. /clock shows the clock.", "你已进入服务器的游戏。输入 /clock 查看时间。",
  "Вы в игре сервера. /clock показывает часы.")
X(WorldEnterFail, "No se pudo entrar en la partida del servidor.", "Could not enter the server's game.",
  "无法进入服务器的游戏。", "Не удалось войти в игру сервера.")
X(WorldLeft, "Has salido de la partida del servidor.", "You left the server's game.", "你已离开服务器的游戏。",
  "Вы вышли из игры сервера.")
X(WorldLost, "Sin conexión con el servidor: vuelves a la selección de archivo.",
  "No connection to the server: you go back to the file select screen.", "与服务器失去连接：你将回到存档选择界面。",
  "Нет связи с сервером: вы возвращаетесь к выбору файла.")
X(WorldStatusOutside, "No estás en la partida del servidor.", "You are not in the server's game.", "你不在服务器的游戏中。",
  "Вы не в игре сервера.")
X(WorldStatusConnectFirst, "Conéctate a un servidor para jugar en su partida.",
  "Connect to a server to play in its game.", "连接到服务器即可在其游戏中游玩。",
  "Подключитесь к серверу, чтобы играть в его игре.")
X(WorldStatusRequested, "Esperando al servidor...", "Waiting for the server...", "正在等待服务器...", "Ожидание сервера...")
X(WorldStatusBooting, "Entrando en la partida del servidor...", "Entering the server's game...", "正在进入服务器的游戏...",
  "Вход в игру сервера...")
X(WorldStatusActive, "En la partida del servidor: {0}.", "In the server's game: {0}.", "在服务器的游戏中：{0}。",
  "В игре сервера: {0}.")
X(WorldConnectFirst, "Conéctate primero a un servidor (menú Co-op).", "Connect to a server first (Co-op menu).",
  "请先连接到服务器（Co-op 菜单）。", "Сначала подключитесь к серверу (меню Co-op).")
X(WorldWaitO2r, "Espera: tu juego aún está preparando los mods del servidor (.o2r).",
  "Wait: your game is still getting the server's mods (.o2r) ready.", "请稍候：你的游戏仍在准备服务器的模组（.o2r）。",
  "Подождите: ваша игра ещё готовит моды сервера (.o2r).")
X(WorldSaved, "Partida guardada en el servidor.", "Game saved on the server.", "游戏已保存到服务器。",
  "Игра сохранена на сервере.")
X(ClockSot, "Has propuesto volver al Amanecer del Primer Día.", "You proposed going back to the Dawn of the First Day.",
  "你提议回到第一天的黎明。", "Вы предложили вернуться к Рассвету первого дня.")
X(ClockSlowed, "tiempo ralentizado", "time slowed down", "时间减慢", "время замедлено")
X(ClockStopped, "reloj parado", "clock stopped", "时钟停止", "часы остановлены")

// ---- The server's game mods (O2r/O2rWindow.cpp, O2r/O2rSync.cpp) ----
X(O2rAsk, "El servidor usa mods del juego (.o2r) que no tienes:",
  "The server uses game mods (.o2r) that you do not have:", "服务器使用了你没有的游戏模组（.o2r）：",
  "Сервер использует моды игры (.o2r), которых у вас нет:")
X(O2rWhere, "Se guardan en la carpeta coop_mods y solo se usan mientras juegas en este servidor.",
  "They are saved in the coop_mods folder and only used while you play on this server.",
  "它们保存在 coop_mods 文件夹中，只在你在此服务器上游玩时使用。",
  "Они сохраняются в папке coop_mods и используются только во время игры на этом сервере.")
X(O2rDownload, "Descargar", "Download", "下载", "Скачать")
X(O2rRefuse, "No, desconectar", "No, disconnect", "不，断开连接", "Нет, отключиться")
X(O2rAlways, "Descargar siempre sin preguntar", "Always download without asking", "总是直接下载，不再询问",
  "Всегда скачивать без вопросов")
X(O2rProgress, "Descargando mods del servidor ({0}/{1}): {2}", "Downloading the server's mods ({0}/{1}): {2}",
  "正在下载服务器的模组（{0}/{1}）：{2}", "Загрузка модов сервера ({0}/{1}): {2}")
X(O2rAlsoMenu, "(también en F1 -> Co-op)", "(also in F1 -> Co-op)", "（也可在 F1 -> Co-op 中操作）", "(также в F1 -> Co-op)")
X(O2rLoading, "Cargando los mods del servidor...", "Loading the server's mods...", "正在加载服务器的模组...",
  "Загрузка модов сервера...")
X(O2rTitle, "Mods del juego del servidor (.o2r)", "The server's game mods (.o2r)", "服务器的游戏模组（.o2r）",
  "Моды игры сервера (.o2r)")
X(O2rNone, "Este servidor no usa mods del juego.", "This server uses no game mods.", "此服务器未使用游戏模组。",
  "Этот сервер не использует моды игры.")
X(O2rStateLoaded, "cargado", "loaded", "已加载", "загружен")
X(O2rStateDownloaded, "descargado", "downloaded", "已下载", "скачан")
X(O2rStateMissing, "falta", "missing", "缺失", "отсутствует")
X(O2rReady, "Listo: tu juego tiene los mods del servidor.", "Ready: your game has the server's mods.",
  "就绪：你的游戏已拥有服务器的模组。", "Готово: в вашей игре есть моды сервера.")
X(O2rAutoDownload, "Descargar los mods del servidor sin preguntar", "Download the server's mods without asking",
  "直接下载服务器的模组，不再询问", "Скачивать моды сервера без вопросов")
X(O2rAltAssets, "Activar \"Enable Mods\" en la partida del servidor", "Turn on \"Enable Mods\" in the server's game",
  "在服务器的游戏中开启“Enable Mods”", "Включать «Enable Mods» в игре сервера")
X(O2rAltAssetsTip,
  "Los mods de gráficos de 2 Ship solo se ven con \"Enable Mods\" (tecla Tab). Si los mods del servidor lo necesitan, "
  "se activa mientras juegas en su partida y al salir vuelve a como lo tenías.",
  "2 Ship's graphics mods only show with \"Enable Mods\" (Tab key). If the server's mods need it, it is turned on "
  "while you play in its game and goes back to how you had it when you leave.",
  "2 Ship 的图形模组只有在开启“Enable Mods”（Tab 键）时才会显示。如果服务器的模组需要，它会在你游玩其游戏时开启，"
  "离开后恢复为你原来的设置。",
  "Графические моды 2 Ship видны только с «Enable Mods» (клавиша Tab). Если моды сервера этого требуют, опция "
  "включается, пока вы играете в его игре, а после выхода возвращается как была.")
X(O2rFolderNote,
  "Se guardan en la carpeta coop_mods, junto a 2ship.exe. Una vez cargados siguen cargados hasta que cierres el "
  "juego. Lo que un mod cambie de la música o los sonidos no se aplica (2 Ship carga el audio al arrancar).",
  "They are saved in the coop_mods folder, next to 2ship.exe. Once loaded they stay loaded until you close the "
  "game. What a mod changes in the music or the sounds does not apply (2 Ship loads the audio when it starts).",
  "它们保存在 2ship.exe 旁边的 coop_mods 文件夹中。加载后会一直保持加载，直到你关闭游戏。模组对音乐或音效的修改不会生效"
  "（2 Ship 在启动时加载音频）。",
  "Они сохраняются в папке coop_mods рядом с 2ship.exe. После загрузки они остаются загруженными, пока вы не "
  "закроете игру. Изменения музыки и звуков из модов не применяются (2 Ship загружает звук при запуске).")
X(O2rErrOpen, "No se pudo crear el archivo del mod en la carpeta coop_mods.",
  "Could not create the mod's file in the coop_mods folder.", "无法在 coop_mods 文件夹中创建模组文件。",
  "Не удалось создать файл мода в папке coop_mods.")
X(O2rErrWrite, "No se pudo guardar el mod en la carpeta coop_mods (¿disco lleno?).",
  "Could not save the mod in the coop_mods folder (disk full?).", "无法将模组保存到 coop_mods 文件夹（磁盘已满？）。",
  "Не удалось сохранить мод в папку coop_mods (диск заполнен?).")
X(O2rErrHash, "El mod descargado no coincide con el del servidor.", "The downloaded mod does not match the server's.",
  "下载的模组与服务器的不一致。", "Скачанный мод не совпадает с модом сервера.")
X(O2rErrChunk, "El servidor ha enviado un trozo de mod que no se esperaba.",
  "The server sent a piece of a mod that was not expected.", "服务器发送了意外的模组数据块。",
  "Сервер прислал неожиданную часть мода.")
X(O2rDownloading, "Descargando los mods del servidor ({0})...", "Downloading the server's mods ({0})...",
  "正在下载服务器的模组（{0}）...", "Загрузка модов сервера ({0})...")
X(O2rBadList, "El servidor ha enviado una lista de mods que no es válida.", "The server sent an invalid list of mods.",
  "服务器发送的模组列表无效。", "Сервер прислал недопустимый список модов.")
X(O2rUses, "El servidor usa mods del juego (.o2r): {0}.", "The server uses game mods (.o2r): {0}.",
  "服务器使用了游戏模组（.o2r）：{0}。", "Сервер использует моды игры (.o2r): {0}.")
X(O2rNeed, "Te faltan {0}: acepta la descarga en la ventana de arriba o en F1 -> Co-op.",
  "You are missing {0}: accept the download in the window at the top or in F1 -> Co-op.",
  "你缺少 {0}：请在上方窗口或 F1 -> Co-op 中接受下载。",
  "Вам не хватает {0}: примите загрузку в окне сверху или в F1 -> Co-op.")
X(O2rDeclined, "Sin los mods del servidor no puedes jugar en él: te has desconectado.",
  "Without the server's mods you cannot play on it: you disconnected.", "没有服务器的模组就无法在其中游玩：你已断开连接。",
  "Без модов сервера на нём нельзя играть: вы отключились.")
X(O2rStalled, "El servidor ha dejado de enviar los mods (.o2r).", "The server stopped sending the mods (.o2r).",
  "服务器停止发送模组（.o2r）。", "Сервер перестал присылать моды (.o2r).")
X(O2rLoadFail, "No se pudo cargar el mod del servidor {0}.", "Could not load the server's mod {0}.",
  "无法加载服务器的模组 {0}。", "Не удалось загрузить мод сервера {0}.")
X(O2rAllLoaded, "Mods del servidor cargados: {0}.", "The server's mods are loaded: {0}.", "服务器的模组已加载：{0}。",
  "Моды сервера загружены: {0}.")

// ---- Group (Group/GroupMenu.cpp, Group/InviteWindow.cpp, Group/GroupState.cpp) ----
X(GroupTitle, "Grupo e invitaciones", "Group and invitations", "小队与邀请", "Группа и приглашения")
X(GroupEnterFirst, "Entra en la partida del servidor para invitar a otros a minijuegos y misiones.",
  "Enter the server's game to invite others to minigames and quests.", "进入服务器的游戏后，才能邀请他人一起玩小游戏和做任务。",
  "Войдите в игру сервера, чтобы приглашать других в мини-игры и задания.")
X(GroupNobody, "No hay nadie más conectado.", "Nobody else is connected.", "没有其他人在线。", "Больше никто не подключён.")
X(GroupInYours, "(en tu grupo)", "(in your group)", "（在你的小队中）", "(в вашей группе)")
X(InviteAll, "Invitar a todos", "Invite everyone", "邀请所有人", "Пригласить всех")
X(GroupNone, "No estás en ningún grupo.", "You are not in a group.", "你不在任何小队中。", "Вы не состоите в группе.")
X(GroupWaiting, "Esperando a que acepten tu invitación...", "Waiting for your invitation to be accepted...",
  "正在等待对方接受你的邀请...", "Ждём, пока примут ваше приглашение...")
X(GroupActivity, "Actividad del líder: {0}", "Leader's activity: {0}", "队长的活动：{0}", "Занятие лидера: {0}")
X(GroupLeave, "Salir del grupo", "Leave the group", "离开小队", "Выйти из группы")
X(GroupInvites, "Invitaciones:", "Invitations:", "邀请：", "Приглашения:")
X(InviteToGroupLine, "{0} te invita a su grupo ({1} s)", "{0} invites you to their group ({1} s)",
  "{0} 邀请你加入其小队（{1} 秒）", "{0} приглашает вас в свою группу ({1} с)")
X(InviteToActivityLine, "{0} te invita a {1} ({2} s)", "{0} invites you to {1} ({2} s)", "{0} 邀请你参加：{1}（{2} 秒）",
  "{0} приглашает вас: {1} ({2} с)")
X(InviteToRoomLine, "{0} te invita a su sala: {1} ({2} s)", "{0} invites you to their room: {1} ({2} s)",
  "{0} 邀请你加入其房间：{1}（{2} 秒）", "{0} приглашает вас в свою комнату: {1} ({2} с)")
X(Participate, "Participar", "Take part", "参加", "Участвовать")
X(InviteChatHint, "(o en el chat: /aceptar, /rechazar)", "(or in the chat: /accept, /decline)",
  "（或在聊天中输入：/accept、/decline）", "(или в чате: /accept, /decline)")
X(InviteChatHintRooms, "(o en el chat: /aceptar, /rechazar, /sala aceptar, /sala rechazar)",
  "(or in the chat: /accept, /decline, /room accept, /room decline)",
  "（或在聊天中输入：/accept、/decline、/room accept、/room decline）",
  "(или в чате: /accept, /decline, /room accept, /room decline)")
X(GroupOptions, "Opciones del grupo", "Group options", "小队选项", "Настройки группы")
X(GroupOptDialogues, "Ver los diálogos de mi grupo", "See my group's dialogues", "查看我小队的对话", "Видеть диалоги моей группы")
X(GroupOptDialoguesTip, "Cuando alguien de tu grupo habla con un NPC cerca de ti, ves el mismo texto.",
  "When someone in your group talks to an NPC near you, you see the same text.",
  "当你的队友在你附近与 NPC 对话时，你会看到相同的文字。",
  "Когда кто-то из вашей группы говорит с NPC рядом с вами, вы видите тот же текст.")
X(GroupOptCutscenes, "Ver las cinemáticas de mi grupo y de los jefes", "See my group's and the bosses' cutscenes",
  "观看我小队和首领的过场动画", "Смотреть сцены моей группы и боссов")
X(GroupOptCutscenesTip, "Las cinemáticas de tu grupo (y las de los jefes, para todos) se ven con su cámara.",
  "Your group's cutscenes (and the bosses', for everyone) are shown with their camera.",
  "你小队的过场动画（以及所有人的首领过场动画）会以其镜头播放。",
  "Сцены вашей группы (и сцены боссов — для всех) показываются их камерой.")
X(GroupOptMinigames, "Unirme a los minijuegos de mi grupo", "Join my group's minigames", "加入我小队的小游戏",
  "Присоединяться к мини-играм моей группы")
X(GroupOptMinigamesTip, "Juegas con el mismo equipo y marcador, o le acompañas a su minijuego.",
  "You play with the same equipment and score, or go along to their minigame.", "你会使用相同的装备和计分，或陪同队友参加其小游戏。",
  "Вы играете с тем же снаряжением и счётом или отправляетесь вместе в их мини-игру.")
X(GroupOptRewards, "Recibir los premios de mi grupo", "Get my group's prizes", "获得我小队的奖励",
  "Получать награды моей группы")
X(GroupOptRewardsTip, "Rupias, botellas y munición que gane tu grupo cerca de ti.",
  "Rupees, bottles and ammo your group wins near you.", "你的小队在你附近赢得的卢比、瓶子和弹药。",
  "Рупии, бутылки и боеприпасы, которые ваша группа выигрывает рядом с вами.")
X(GroupOptCinemaActors, "Ver los actores de las cinemáticas que miro", "See the actors of the cutscenes I watch",
  "显示我所观看的过场动画中的角色", "Видеть персонажей сцен, которые я смотрю")
X(GroupOptCinemaActorsTip, "En la cinemática de otro ves también sus personajes de cinemática (Dm_*, Demo_*).",
  "In someone else's cutscene you also see its cutscene characters (Dm_*, Demo_*).",
  "在别人的过场动画中，也会显示其过场角色（Dm_*、Demo_*）。",
  "В чужой сцене вы видите и её персонажей (Dm_*, Demo_*).")
X(GroupOptEffects, "Ver los efectos de los demás", "See the others' effects", "显示其他人的特效", "Видеть эффекты других")
X(GroupOptEffectsTip, "Partículas de los otros Links, de los enemigos que simulan y de sus cinemáticas.",
  "Particles of the other Links, of the enemies they simulate and of their cutscenes.",
  "其他林克、他们模拟的敌人以及他们的过场动画的粒子效果。",
  "Частицы других Линков, врагов, которых они симулируют, и их сцен.")
X(GroupOptTurns, "Avisarme cuando me toque (minijuegos por turnos)", "Tell me when it is my turn (turn-based minigames)",
  "轮到我时提醒我（轮流进行的小游戏）", "Сообщать, когда моя очередь (мини-игры по очереди)")
X(GroupOptTurnsTip, "Cuando alguien de tu grupo termina un minijuego de un jugador, te avisa de tu ronda.",
  "When someone in your group finishes a one-player minigame, it tells you it is your round.",
  "当你的队友完成一个单人小游戏时，会提醒你轮到你了。",
  "Когда кто-то из вашей группы заканчивает мини-игру на одного, вам сообщают, что ваш черёд.")
X(GroupLeaderYou, "Ahora eres el líder del grupo.", "You are now the group's leader.", "你现在是小队的队长。",
  "Теперь вы лидер группы.")
X(GroupLeaderOther, "{0} es ahora el líder del grupo.", "{0} is now the group's leader.", "{0} 现在是小队的队长。",
  "{0} теперь лидер группы.")

// ---- Activity rooms (Room/RoomMenu.cpp, Room/RoomWindow.cpp, Room/RoomState.cpp, Room/RoomHold.cpp) ----
X(RoomPhaseLobby, "esperando confirmaciones", "waiting for confirmations", "等待确认", "ждёт подтверждений")
X(RoomPhaseStarting, "a punto de empezar", "about to start", "即将开始", "вот-вот начнётся")
X(RoomPhaseRunning, "en marcha", "running", "进行中", "идёт")
X(RoomPhaseEnded, "terminada", "over", "已结束", "закончена")
X(RoomStateWaiting, "esperando", "waiting", "等待中", "ожидание")
X(RoomStateStarting, "empezando", "starting", "正在开始", "начинается")
X(RoomsTitle, "Salas de actividad", "Activity rooms", "活动房间", "Комнаты занятий")
X(RoomsEnterFirst, "Entra en la partida del servidor para usar las salas de actividad.",
  "Enter the server's game to use the activity rooms.", "进入服务器的游戏后才能使用活动房间。",
  "Войдите в игру сервера, чтобы пользоваться комнатами занятий.")
X(RoomNoneHint, "No estás en ninguna sala. Se abre una al empezar un minijuego (con o sin grupo).",
  "You are not in a room. One opens when a minigame starts (with or without a group).",
  "你不在任何房间中。开始小游戏时（无论是否组队）会打开一个房间。",
  "Вы не в комнате. Она открывается, когда начинается мини-игра (с группой или без).")
X(RoomYours, "Tu sala: {0} ({1}, {2} jugadores)", "Your room: {0} ({1}, {2} players)", "你的房间：{0}（{1}，{2} 名玩家）",
  "Ваша комната: {0} ({1}, игроков: {2})")
X(RoomOpenWindow, "Abrir la ventana de la sala", "Open the room's window", "打开房间窗口", "Открыть окно комнаты")
X(RoomLeave, "Salir de la sala", "Leave the room", "离开房间", "Выйти из комнаты")
X(RoomOpenList, "Salas abiertas:", "Open rooms:", "开放的房间：", "Открытые комнаты:")
X(RoomOpenNone, "Ninguna.", "None.", "无。", "Нет.")
X(RoomJoin, "Unirme", "Join", "加入", "Войти")
X(RoomOptEnabled, "Abrir una sala al empezar un minijuego", "Open a room when a minigame starts", "开始小游戏时打开房间",
  "Открывать комнату при начале мини-игры")
X(RoomOptEnabledTip,
  "Tu grupo entra solo y puedes invitar a quien quieras solo para esa actividad. Desactivado: los minijuegos "
  "funcionan como antes (solo el grupo, de cerca).",
  "Your group joins by itself and you can invite whoever you want just for that activity. Off: minigames work as "
  "before (only the group, from nearby).",
  "你的小队会自动加入，你还可以只为这项活动邀请任何人。关闭此项：小游戏和以前一样（仅限附近的小队成员）。",
  "Ваша группа входит сама, и вы можете пригласить кого угодно только на это занятие. Выключено: мини-игры работают "
  "как раньше (только группа, рядом).")
X(RoomOptHold, "Esperar a que todos confirmen antes de empezar", "Wait until everyone confirms before starting",
  "开始前等待所有人确认", "Ждать подтверждения всех перед началом")
X(RoomOptHoldTip,
  "Tu partida se congela al empezar el minijuego hasta que todos los de la sala pulsan Listo (tú también: botón "
  "Listo o START). Desactivado: empieza al momento y los demás se unen al confirmar.",
  "Your game freezes when the minigame starts until everyone in the room presses Ready (you too: the Ready button "
  "or START). Off: it starts at once and the others join when they confirm.",
  "小游戏开始时你的游戏会暂停，直到房间里的所有人都按下“准备”（你也一样：“准备”按钮或 START）。关闭此项：立即开始，"
  "其他人确认后加入。",
  "Ваша игра замирает в начале мини-игры, пока все в комнате не нажмут «Готов» (вы тоже: кнопка «Готов» или "
  "START). Выключено: начинается сразу, а остальные присоединяются, когда подтвердят.")
X(RoomScale, "Tamaño de la ventana de la sala", "Room window size", "房间窗口大小", "Размер окна комнаты")
X(RoomWaitAll, "Esperando a que todos estén listos (se cancela en {0})", "Waiting for everyone to be ready (cancelled in {0})",
  "正在等待所有人准备（{0} 后取消）", "Ждём готовности всех (отмена через {0})")
X(RoomAllReady, "¡Todos listos! Empieza en {0}...", "Everyone is ready! Starting in {0}...", "所有人已准备！{0} 秒后开始...",
  "Все готовы! Начало через {0}...")
X(RoomRunning, "En marcha", "Running", "进行中", "Идёт")
X(RoomEnded, "Terminada: la sala se cierra en {0} si nadie empieza otra ronda",
  "Over: the room closes in {0} unless someone starts another round", "已结束：如果没人开始新一轮，房间将在 {0} 后关闭",
  "Закончено: комната закроется через {0}, если никто не начнёт новый раунд")
X(RoomHeld, "Tu partida espera a que todos confirmen (START o el botón Listo).",
  "Your game waits until everyone confirms (START or the Ready button).", "你的游戏正在等待所有人确认（START 或“准备”按钮）。",
  "Ваша игра ждёт, пока все подтвердят (START или кнопка «Готов»).")
X(MemberReady, "- listo", "- ready", "- 已准备", "- готов")
X(MemberTravelling, "- viajando", "- travelling", "- 传送中", "- в пути")
X(MemberWaiting, "- esperando", "- waiting", "- 等待中", "- ждёт")
X(ResultWon, "ganó", "won", "获胜", "победил")
X(ResultLost, "no ganó", "did not win", "未获胜", "не победил")
X(RoomKick, "Quitar", "Remove", "移出", "Удалить")
X(RoomInviteTitle, "Invitar (solo para esta actividad)", "Invite (just for this activity)", "邀请（仅限本次活动）",
  "Пригласить (только на это занятие)")
X(RoomInviteGroup, "Invitar a mi grupo", "Invite my group", "邀请我的小队", "Пригласить мою группу")
X(RoomInvited, "(invitado)", "(invited)", "（已邀请）", "(приглашён)")
X(RoomNobodyToInvite, "No hay nadie más en el servidor para invitar.", "There is nobody else on the server to invite.",
  "服务器上没有其他可邀请的人。", "На сервере больше некого пригласить.")
X(RoomSettings, "Configuración", "Settings", "设置", "Настройки")
X(RoomSettingsGuest, "Configuración (la cambia el anfitrión)", "Settings (the host changes them)", "设置（由房主更改）",
  "Настройки (меняет хозяин)")
X(RoomSetOpen, "Sala abierta: cualquiera de la partida puede unirse", "Open room: anyone in the game can join",
  "开放房间：游戏中的任何人都可加入", "Открытая комната: войти может любой в игре")
X(RoomSetTravel, "Llevar a los participantes al confirmar", "Take the participants along when they confirm",
  "确认后将参与者传送过来", "Переносить участников при подтверждении")
X(RoomSetRewards, "Compartir los premios de la actividad", "Share the activity's prizes", "共享活动奖励",
  "Делить награды занятия")
X(RoomSend, "Enviar", "Send", "发送", "Отправить")
X(RoomReadyStart, "Listo: empezar", "Ready: start", "准备：开始", "Готов: начать")
X(RoomReady, "Listo", "Ready", "准备", "Готов")
X(RoomNotReady, "No estoy listo", "I am not ready", "取消准备", "Я не готов")
X(RoomNoParticipate, "No participar (salir de la sala)", "Do not take part (leave the room)", "不参加（离开房间）",
  "Не участвовать (выйти из комнаты)")
X(RoomClose, "Cerrar sala", "Close the room", "关闭房间", "Закрыть комнату")
X(RoomParticipants, "Participantes ({0}/4)", "Participants ({0}/4)", "参与者（{0}/4）", "Участники ({0}/4)")
X(RoomInvitedLine, "Invitado: {0} ({1} s)", "Invited: {0} ({1} s)", "已邀请：{0}（{1} 秒）", "Приглашён: {0} ({1} с)")
X(RoomChatTitle, "Chat de la sala", "Room chat", "房间聊天", "Чат комнаты")
X(RoomWindowTitle, "Sala: {0}", "Room: {0}", "房间：{0}", "Комната: {0}")
X(RoomCollapse, "Reducir", "Shrink", "缩小", "Свернуть")
X(RoomExpand, "Abrir", "Open", "展开", "Развернуть")
X(RoomChatLine, "[Sala] <{0}> {1}", "[Room] <{0}> {1}", "[房间] <{0}> {1}", "[Комната] <{0}> {1}")
X(RoomHoldWaiting, "Tu partida espera a que todos los de la sala estén listos (START: listo).",
  "Your game waits until everyone in the room is ready (START: ready).", "你的游戏正在等待房间里的所有人准备（START：准备）。",
  "Ваша игра ждёт, пока все в комнате будут готовы (START — готов).")
X(RoomCantStart, "La actividad no puede empezar ya: sales de su sala.",
  "The activity cannot start any more: you leave its room.", "活动已无法开始：你离开了它的房间。",
  "Занятие уже не может начаться: вы выходите из его комнаты.")

// ---- Sync (Sync/SyncMenu.cpp, Sync/SyncOptions.cpp) ----
X(SyncTitle, "Sincronización", "Sync", "同步", "Синхронизация")
X(SyncPartTip, "Apágalo si algo de esta parte falla: se nota sin reiniciar.",
  "Turn it off if something in this part fails: it takes effect without restarting.", "如果这部分出现问题请关闭它：无需重启即可生效。",
  "Выключите, если в этой части что-то не работает: действует без перезапуска.")
X(SyncServerOff, "(apagado en el servidor)", "(off on the server)", "（服务器已关闭）", "(выключено на сервере)")
X(SyncSounds, "Sonidos de los demás Links y de lo que simulan sus juegos",
  "Sounds of the other Links and of what their games simulate", "其他林克以及其游戏所模拟内容的声音",
  "Звуки других Линков и того, что симулируют их игры")
X(SyncAmbient, "Música y temblores de jefes, minijefes y eventos", "Music and quakes of bosses, minibosses and events",
  "首领、小首领和事件的音乐与震动", "Музыка и тряска боссов, мини-боссов и событий")
X(SyncPlayerObjects, "Objetos de los jugadores (flechas, bombas, gancho...) y lo que llevan en la mano",
  "The players' objects (arrows, bombs, hookshot...) and what they carry", "玩家的物体（箭、炸弹、钩索……）及其手中所持之物",
  "Предметы игроков (стрелы, бомбы, крюк...) и то, что у них в руках")
X(SyncSceneFlags, "Banderas de la escena en vivo (interruptores, cofres, salas)",
  "Live scene flags (switches, chests, rooms)", "实时场景标志（开关、宝箱、房间）",
  "Флаги сцены в реальном времени (переключатели, сундуки, помещения)")
X(SyncSceneObjects, "Maquinaria de la escena (plataformas, ascensores, interruptores, bloques)",
  "Scene machinery (platforms, lifts, switches, blocks)", "场景机关（平台、升降机、开关、方块）",
  "Механизмы сцены (платформы, лифты, переключатели, блоки)")
X(SyncOcarina, "Ocarina de los demás (sus notas y sus canciones)", "The others' ocarina (their notes and songs)",
  "其他人的陶笛（音符和乐曲）", "Окарина других (их ноты и мелодии)")
X(SyncDrops, "Objetos que caen (corazones, rupias, hadas...): los mismos para todos, el primero que llega se los queda",
  "Dropped items (hearts, rupees, fairies...): the same for everyone, the first to get there keeps them",
  "掉落物（心、卢比、妖精……）：所有人看到的都一样，先到先得",
  "Выпадающие предметы (сердца, рупии, феи...): одни на всех, достаются тому, кто успел первым")
X(SyncFlagReload, "Recrear objetos que otro jugador cambia (paredes, rocas, bloques, templos)",
  "Recreate objects another player changes (walls, rocks, blocks, temples)", "重建被其他玩家改变的物体（墙、岩石、方块、神殿）",
  "Пересоздавать объекты, которые меняет другой игрок (стены, камни, блоки, храмы)")
X(SyncFlagReloadTip,
  "Un objeto que solo mira su bandera al crearse se crea de nuevo cuando otro jugador la cambia. También los eventos "
  "del mundo: el templo de Woodfall aparece cuando otro jugador toca la Sonata.",
  "An object that only reads its flag when it is created is created again when another player changes it. World "
  "events too: the Woodfall temple appears when another player plays the Sonata.",
  "只在创建时读取标志的物体，会在其他玩家改变该标志时重新创建。世界事件也是如此：当其他玩家演奏奏鸣曲时，Woodfall 神殿会出现。",
  "Объект, который читает свой флаг только при создании, создаётся заново, когда другой игрок его меняет. Так же и "
  "события мира: храм Вудфолла появляется, когда другой игрок играет Сонату.")
X(SyncListPointers, "Las copias miran los objetos de tu juego (el globo de Jim)",
  "Copies look at your game's objects (Jim's balloon)", "复制体会使用你游戏中的物体（Jim 的气球）",
  "Копии смотрят на объекты вашей игры (шарик Джима)")
X(SyncListPointersTip,
  "Un personaje copiado de otro juego que mira algo que cada juego tiene por su cuenta (el globo al que dispara Jim, "
  "un cartel, una puerta) mira el de tu juego. Apagado: como antes.",
  "A character copied from another game that looks at something each game has on its own (the balloon Jim shoots "
  "at, a sign, a door) looks at your game's. Off: as before.",
  "从其他游戏复制来的角色，如果关注的是每个游戏各自拥有的东西（Jim 射击的气球、告示牌、门），会改为关注你游戏中的那个。"
  "关闭此项：和以前一样。",
  "Персонаж, скопированный из другой игры, который смотрит на то, что у каждой игры своё (шарик, в который стреляет "
  "Джим, табличка, дверь), смотрит на объект вашей игры. Выключено: как раньше.")
X(SyncSongEffects, "Ver el efecto de las canciones de los demás (destello, lluvia)",
  "See the effect of the others' songs (flash, rain)", "显示其他人乐曲的效果（闪光、雨）",
  "Видеть эффект мелодий других (вспышка, дождь)")
X(SyncSongEffectsTip,
  "Cuando otro jugador toca bien una canción a tu lado la oyes siempre; esto añade su destello en tu pantalla (o la "
  "lluvia de la Canción de la Tormenta).",
  "When another player plays a song right next to you, you always hear it; this adds its flash on your screen (or "
  "the rain of the Song of Storms).",
  "当其他玩家在你身边正确演奏乐曲时，你总能听到；此项会在你的屏幕上加上它的闪光（或风暴之歌的雨）。",
  "Когда другой игрок рядом с вами правильно играет мелодию, вы всегда её слышите; это добавляет её вспышку на ваш "
  "экран (или дождь Песни бури).")
X(SyncVolume, "Volumen de los demás (%)", "The others' volume (%)", "其他人的音量（%）", "Громкость других (%)")
X(SyncShared, "Compartir también (ids o nombres)", "Also share (ids or names)", "额外共享（ID 或名称）",
  "Также синхронизировать (id или имена)")
X(SyncSharedTip,
  "Actores que no estaban en la lista y quieres que sean iguales para todos, separados por comas: OBJ_RAILLIFT, "
  "0x1C3... Se aplica al recargar la escena.",
  "Actors that were not on the list and that you want the same for everyone, separated by commas: OBJ_RAILLIFT, "
  "0x1C3... It applies when the scene reloads.",
  "原本不在列表中、但你希望所有人看到一致的角色，用逗号分隔：OBJ_RAILLIFT, 0x1C3... 重新加载场景后生效。",
  "Акторы, которых не было в списке и которые должны быть у всех одинаковыми, через запятую: OBJ_RAILLIFT, 0x1C3... "
  "Применяется при перезагрузке сцены.")
X(SyncLocal, "Nunca compartir (ids o nombres)", "Never share (ids or names)", "永不共享（ID 或名称）",
  "Никогда не синхронизировать (id или имена)")
X(SyncLocalTip, "Actores que cada juego tendrá a su aire aunque estén en una lista. Se aplica al recargar la escena.",
  "Actors each game will run on its own even if they are on a list. It applies when the scene reloads.",
  "即使在列表中，每个游戏也会各自运行这些角色。重新加载场景后生效。",
  "Акторы, которые каждая игра будет вести сама, даже если они в списке. Применяется при перезагрузке сцены.")
X(SyncNearby, "Ver lo sincronizado cerca (para avisar de un fallo)", "See what is synced nearby (to report a bug)",
  "查看附近同步的内容（用于报告问题）", "Показать синхронизируемое рядом (чтобы сообщить об ошибке)")
X(SyncNothingNear, "Nada sincronizado a menos de 1500 unidades.", "Nothing synced within 1500 units.",
  "1500 单位内没有同步的内容。", "В радиусе 1500 единиц ничего не синхронизируется.")
X(ColActor, "Actor", "Actor", "角色", "Актор")
X(ColId, "Id", "Id", "ID", "Id")
X(ColRoom, "Sala", "Room", "房间", "Помещение")
X(ColSimulatedBy, "Lo simula", "Simulated by", "模拟者", "Кто симулирует")
X(ColDistance, "Distancia", "Distance", "距离", "Расстояние")
X(CellPlayer, "jugador", "player", "玩家", "игрок")
X(CellNobodyYet, "nadie aún", "nobody yet", "暂无", "пока никто")
X(CellYou, "tú", "you", "你", "вы")
X(SyncItems, "Ver los objetos que caen cerca (corazones, rupias, hadas)",
  "See the dropped items nearby (hearts, rupees, fairies)", "查看附近的掉落物（心、卢比、妖精）",
  "Показать выпавшие предметы рядом (сердца, рупии, феи)")
X(SyncNoItems, "Ningún objeto compartido a menos de 1500 unidades.", "No shared item within 1500 units.",
  "1500 单位内没有共享的物品。", "В радиусе 1500 единиц нет общих предметов.")
X(ColItem, "Objeto", "Item", "物品", "Предмет")
X(ColKey, "Clave", "Key", "键", "Ключ")
X(ColDroppedIn, "Cayó en", "Dropped in", "掉落于", "Выпал в")
X(ColState, "Estado", "State", "状态", "Состояние")
X(CellYourGame, "tu juego", "your game", "你的游戏", "ваша игра")
X(CellOtherGame, "otro juego", "another game", "其他游戏", "другая игра")
X(CellOnGround, "en el suelo", "on the ground", "在地上", "на земле")
X(CellAsking, "pidiéndolo", "asking for it", "请求中", "запрошен")
X(CellYours, "tuyo", "yours", "你的", "ваш")

// ---- Activities: names (Activities/ActivityTable.cpp; the server repeats them in its own language) ----
X(ActGaleriaCiudad, "Galería de tiro de la ciudad", "Town Shooting Gallery", "时钟镇射击场", "Тир в городе")
X(ActGaleriaPantano, "Galería de tiro del pantano", "Swamp Shooting Gallery", "沼泽射击场", "Тир на болоте")
X(ActHoneyDarling, "Honey y Darling", "Honey & Darling", "Honey & Darling", "Хани и Дарлинг")
X(ActCasaEspiritus, "Casa de los espíritus", "Spirit House", "幽灵屋", "Дом духов")
X(ActCarreraGoron, "Carrera goron", "Goron Race", "格隆族赛跑", "Гонка горонов")
X(ActCarreraGorman, "Carrera de los hermanos Gorman", "Gorman Brothers' Race", "Gorman 兄弟赛马",
  "Скачки братьев Горман")
X(ActCarreraCastores, "Carrera de los castores", "Beaver Race", "河狸赛跑", "Гонка бобров")
X(ActCarreraPerros, "Carrera de perros", "Doggy Race", "赛狗", "Собачьи бега")
X(ActGlobosRomani, "Tiro con Romani", "Romani's Archery", "Romani 的射箭练习", "Стрельба с Романи")
X(ActCofres, "Cofres del tesoro", "Treasure Chest Shop", "宝箱屋", "Лавка сундуков")
X(ActPatioDeku, "Patio de los Deku", "Deku Scrub Playground", "德库游乐场", "Площадка деку")
X(ActEspadachin, "Escuela de espadachín", "Swordsman's School", "剑术学校", "Школа фехтования")
X(ActCartero, "Juego del cartero", "Postman's Game", "邮差的游戏", "Игра почтальона")
X(ActBarcaKoume, "Tiro en barca de Koume", "Koume's Boat Archery", "Koume 的乘船射箭", "Стрельба с лодки Коуме")
X(ActSaltosPescador, "Saltos del pescador", "Fisherman's Jumping Game", "渔夫的跳跃游戏", "Прыжки рыбака")
X(ActMayordomoDeku, "Carrera del mayordomo deku", "Deku Butler's Race", "德库管家的赛跑", "Гонка дворецкого деку")
X(ActBombers, "El escondite de los Bombers", "The Bombers' Hide-and-Seek", "Bombers 的捉迷藏", "Прятки с Бомберами")
X(ActAnjuKafei, "Anju y Kafei", "Anju and Kafei", "Anju 与 Kafei", "Анджу и Кафей")
X(ActPollitosGrog, "Los pollitos de Grog", "Grog's Chicks", "Grog 的小鸡", "Цыплята Грога")
X(ActRomaniCremia, "Romani y Cremia", "Romani and Cremia", "Romani 与 Cremia", "Романи и Кремия")
X(ActAncianaBombas, "La anciana de la tienda de bombas", "The Bomb Shop Lady", "炸弹店的老奶奶",
  "Старушка из бомбовой лавки")
X(ActKamaro, "La danza de Kamaro", "Kamaro's Dance", "Kamaro 之舞", "Танец Камаро")
X(ActAbuelaAnju, "Los cuentos de la abuela", "Grandma's Stories", "老奶奶的故事", "Бабушкины сказки")
X(ActKeaton, "El acertijo de Keaton", "Keaton's Quiz", "Keaton 的问答", "Загадки Китона")
X(ActMascaraPiedra, "La máscara de piedra", "The Stone Mask", "石头面具", "Каменная маска")
X(ActMascaraGibdo, "El padre de Pamela", "Pamela's Father", "Pamela 的父亲", "Отец Памелы")
X(ActManoRetrete, "La mano del retrete", "The Hand in the Toilet", "厕所里的手", "Рука из туалета")
X(ActCompaniaGorman, "La compañía de Gorman", "Gorman's Troupe", "Gorman 的剧团", "Труппа Гормана")
X(ActFantasmasDampe, "Los fantasmas de Dampé", "Dampé's Ghosts", "Dampé 的幽灵", "Призраки Дампе")
X(ActHuevosZora, "Los huevos zora", "The Zora Eggs", "佐拉之卵", "Яйца зора")
X(ActCasaSkulltulas, "La casa de las Skulltulas", "The Skulltula House", "骷髅蜘蛛之家", "Дом скультул")
X(ActComercioDeku, "El comercio de los Deku", "The Deku Trade", "德库的交易", "Торговля деку")

// ---- Activities: how they are played, chat lines and HUD (Activities/*.cpp) ----
X(ModeTogether, "Juntos: todos jugáis la misma partida", "Together: you all play the same round",
  "一起：所有人玩同一局", "Вместе: все играют одну и ту же партию")
X(ModeEach, "Cada uno: cada uno su carrera, los mismos rivales", "Each one: each their own race, the same rivals",
  "各自：各跑各的，对手相同", "Каждый сам: у каждого свой забег, соперники те же")
X(ModeTurns, "Por turnos: uno juega y los demás miran", "Turns: one plays and the others watch",
  "轮流：一人游玩，其他人观看", "По очереди: один играет, остальные смотрят")
X(ModeShared, "Compartido: el progreso es de todos", "Shared: the progress is everyone's", "共享：进度属于所有人",
  "Общее: прогресс у всех один")
X(ActGroupPlays, "{0}: tu grupo juega contigo.", "{0}: your group plays with you.", "{0}：你的小队和你一起玩。",
  "{0}: ваша группа играет с вами.")
X(ActYouWon, "Has ganado {0}{1}.", "You won {0}{1}.", "你赢得了{0}{1}。", "Вы победили: {0}{1}.")
X(ActYouLost, "No has ganado {0}{1}.", "You did not win {0}{1}.", "你没有赢得{0}{1}。", "Вы не победили: {0}{1}.")
X(GuestTogether, "Juegas {0} con {1}: los puntos son del grupo.", "You play {0} with {1}: the points are the group's.",
  "你和 {1} 一起玩{0}：分数属于小队。", "Вы играете в «{0}» с {1}: очки общие для группы.")
X(GuestEach, "Juegas {0} a la vez que {1}.", "You play {0} at the same time as {1}.", "你和 {1} 同时玩{0}。",
  "Вы играете в «{0}» одновременно с {1}.")
X(GuestTurns, "Miras la ronda de {1} en {0}.", "You watch {1}'s round of {0}.", "你在观看 {1} 的{0}回合。",
  "Вы смотрите раунд {1}: {0}.")
X(GuestShared, "Jugáis {0} con {1}: el progreso es de todos.", "You and {1} play {0}: the progress is everyone's.",
  "你和 {1} 一起玩{0}：进度属于所有人。", "Вы с {1} играете в «{0}»: прогресс общий.")
X(GuestHudTime, "Tiempo: {0}", "Time: {0}", "时间：{0}", "Время: {0}")
X(GuestHudPoints, "Puntos: {0}", "Points: {0}", "分数：{0}", "Очки: {0}")
X(GuestTurnEnds, "{0} termina {1}.", "{0} finishes {1}.", "{0} 完成了{1}。", "{0} заканчивает: {1}.")
X(GuestTurnEndsResult, "{0} termina {1}: {2}.", "{0} finishes {1}: {2}.", "{0} 完成了{1}：{2}。",
  "{0} заканчивает: {1} — {2}.")
X(GuestYourTurn, "Te toca: {0}. Habla con quien lo lleva para jugar tu ronda.",
  "Your turn: {0}. Talk to whoever runs it to play your round.", "轮到你了：{0}。和负责的人对话来进行你的回合。",
  "Ваш черёд: {0}. Поговорите с ведущим, чтобы сыграть свой раунд.")
X(GuestStarts, "{0} empieza {1}.", "{0} starts {1}.", "{0} 开始了{1}。", "{0} начинает: {1}.")
X(GuestWins, "{0} gana {1}{2}.", "{0} wins {1}{2}.", "{0} 赢得了{1}{2}。", "{0} побеждает: {1}{2}.")
X(GuestLoses, "{0} no gana {1}{2}.", "{0} does not win {1}{2}.", "{0} 没有赢得{1}{2}。", "{0} не побеждает: {1}{2}.")
X(RewardRupees, "+{0} rupias (premio de {1}).", "+{0} rupees ({1}'s prize).", "+{0} 卢比（{1} 的奖励）。",
  "+{0} рупий (награда {1}).")
X(RewardNoBottle, "No tenías una botella vacía para el premio de {0}.", "You had no empty bottle for {0}'s prize.",
  "你没有空瓶子来装 {0} 的奖励。", "У вас не было пустой бутылки для награды {0}.")
X(RewardGroup, "Premio del grupo (de {0}).", "Group prize (from {0}).", "小队奖励（来自 {0}）。", "Награда группы (от {0}).")
X(BombersCaught, "¡Han atrapado a un Bomber! ({0}/5)", "A Bomber was caught! ({0}/5)", "抓到了一个 Bomber！（{0}/5）",
  "Пойман один Бомбер! ({0}/5)")
X(BombersAll, "¡Los cinco Bombers atrapados! Jim espera en el norte de la ciudad con el código.",
  "All five Bombers caught! Jim waits in North Clock Town with the code.", "五个 Bombers 都抓到了！Jim 在北时钟镇带着密码等你。",
  "Все пять Бомберов пойманы! Джим ждёт с кодом в северной части города.")
X(BombersFormDeku, "forma deku", "Deku form", "德库形态", "облик деку")
X(BombersFormHuman, "forma humana", "human form", "人类形态", "облик человека")
X(BombersProgress, "Bombers atrapados: {0}/5 ({1})", "Bombers caught: {0}/5 ({1})", "已抓到的 Bombers：{0}/5（{1}）",
  "Поймано Бомберов: {0}/5 ({1})")

// ---- Features (Features/*.cpp) ----
X(UnlockWarn, "Aviso del desbloqueo: {0}", "Unlock warning: {0}", "解锁警告：{0}", "Предупреждение разблокировки: {0}")
X(UnlockDone,
  "Tienes todos los objetos, máscaras, canciones y corazones (lo compartido es para toda la partida y ya está "
  "guardado en el servidor).",
  "You have every item, mask, song and heart (what is shared is for the whole game and is already saved on the "
  "server).",
  "你已拥有所有物品、面具、乐曲和心（共享的部分属于整局游戏，且已保存到服务器）。",
  "У вас есть все предметы, маски, мелодии и сердца (общее — для всей игры и уже сохранено на сервере).")
X(UnlockOutside, "El desbloqueo solo funciona dentro de la partida del servidor.",
  "Unlocking only works inside the server's game.", "解锁只能在服务器的游戏中使用。",
  "Разблокировка работает только внутри игры сервера.")
X(UnlockFailed, "No se pudo aplicar el desbloqueo (la partida no estaba lista): pídelo otra vez.",
  "The unlock could not be applied (the game was not ready): ask for it again.", "无法应用解锁（游戏尚未就绪）：请再请求一次。",
  "Не удалось применить разблокировку (игра была не готова): запросите её ещё раз.")
X(GiveRupees, "Has recibido {0} rupias.", "You got {0} rupees.", "你收到了 {0} 卢比。", "Вы получили {0} рупий.")
X(GiveRupeesBank, "Has recibido {0} rupias ({1} al banco).", "You got {0} rupees ({1} to the bank).",
  "你收到了 {0} 卢比（{1} 存入银行）。", "Вы получили {0} рупий ({1} — в банк).")
X(SelfTestOk, "Prueba de campos del mundo: correcta.", "World field self-test: correct.", "世界字段自检：正确。",
  "Самопроверка полей мира: в порядке.")
X(SelfTestFail, "Prueba de campos del mundo: {0}", "World field self-test: {0}", "世界字段自检：{0}",
  "Самопроверка полей мира: {0}")
X(EndFierceDeity, "Tenéis las 20 máscaras: la Máscara de la Fiera Deidad es vuestra.",
  "You have the 20 masks: the Fierce Deity's Mask is yours.", "你们集齐了 20 个面具：鬼神面具归你们了。",
  "У вас все 20 масок: Маска Свирепого божества ваша.")
X(EndGiants, "Los gigantes detienen la luna. ¡A por Majora!", "The giants hold back the moon. Go get Majora!",
  "巨人们挡住了月亮。去打倒姆吉拉吧！", "Великаны удерживают луну. Вперёд, на Маджору!")
X(EndServerGame, "La partida del servidor", "The server's game", "服务器的游戏", "Игра сервера")
X(EndDefeated, "¡{0} ha derrotado a Majora!", "{0} has defeated Majora!", "{0} 打倒了姆吉拉！", "{0} победил Маджору!")
X(EndRooftop, "{0} ha subido a la torre del reloj: vais todos allí.", "{0} climbed the clock tower: you all go there.",
  "{0} 登上了钟楼：所有人都前往那里。", "{0} поднялся на часовую башню: все отправляются туда.")
X(EndLair, "{0} ha tocado la Canción del Juramento: ¡todos contra Majora!",
  "{0} played the Oath to Order: everyone against Majora!", "{0} 演奏了誓约之歌：所有人一起对抗姆吉拉！",
  "{0} сыграл Клятву порядку: все против Маджоры!")
X(EndStarts, "{0} empieza el final del juego: lo veis todos juntos.", "{0} starts the game's ending: you all watch it together.",
  "{0} 开始了游戏的结局：所有人一起观看。", "{0} начинает финал игры: вы смотрите его все вместе.")
X(EndingBegins, "Empieza el final del juego. Lo veis todos a la vez.", "The game's ending begins. You all watch it at once.",
  "游戏的结局开始了。所有人同时观看。", "Начинается финал игры. Все смотрят его одновременно.")
X(EndingFin, "Fin. En unos segundos volvéis al Amanecer del Primer Día.",
  "The end. In a few seconds you go back to the Dawn of the First Day.", "完。几秒后你们将回到第一天的黎明。",
  "Конец. Через несколько секунд вы вернётесь к Рассвету первого дня.")
X(GiftRefund, "Se te han devuelto {0} rupias.", "{0} rupees were given back to you.", "已退还给你 {0} 卢比。",
  "Вам вернули {0} рупий.")
X(GiftRefundReason, "Se te han devuelto {0} rupias ({1}).", "{0} rupees were given back to you ({1}).",
  "已退还给你 {0} 卢比（{1}）。", "Вам вернули {0} рупий ({1}).")
X(GiftRefundFail, "No se pudieron devolver {0} rupias: no estás en una partida.",
  "{0} rupees could not be given back: you are not in a game.", "无法退还 {0} 卢比：你不在游戏中。",
  "Не удалось вернуть {0} рупий: вы не в игре.")
X(TpMoved, "Te han teletransportado.", "You were teleported.", "你被传送了。", "Вас телепортировали.")
X(TpNextTo, "Te has teletransportado junto a {0}.", "You teleported next to {0}.", "你被传送到了 {0} 身边。",
  "Вы телепортировались к {0}.")
X(TpTravel, "Viajando...", "Travelling...", "正在传送...", "Перемещение...")
X(TpTravelTo, "Viajando hasta {0}...", "Travelling to {0}...", "正在传送到 {0} 身边...", "Перемещение к {0}...")
X(TpDefaultTarget, "el jugador", "the player", "该玩家", "игрок")
X(TpBad, "El destino no es válido.", "The destination is not valid.", "目的地无效。", "Место назначения недопустимо.")
X(TpBadOf, "El destino de {0} no es válido.", "{0}'s destination is not valid.", "{0} 的位置无效。",
  "Место назначения {0} недопустимо.")
X(TpLater, "Irás junto a {0} en cuanto puedas ({1}).", "You will go to {0} as soon as you can ({1}).",
  "一旦可以，你就会前往 {0} 身边（{1}）。", "Вы переместитесь к {0}, как только сможете ({1}).")
X(TpBlocked, "No puedes teletransportarte ahora: {0}.", "You cannot teleport now: {0}.", "你现在无法传送：{0}。",
  "Сейчас нельзя телепортироваться: {0}.")
X(WarpNotInGame, "no estás dentro de una partida", "you are not inside a game", "你不在游戏中", "вы не в игре")
X(WarpChangingScene, "estás cambiando de escenario", "you are changing scenes", "你正在切换场景", "вы переходите в другую сцену")
X(WarpRoomHold, "tu partida espera a que empiece tu sala", "your game waits for your room to start",
  "你的游戏正在等待你的房间开始", "ваша игра ждёт начала вашей комнаты")
X(WarpDead, "Link no tiene energía", "Link has no health left", "林克没有生命了", "у Линка не осталось здоровья")
X(WarpCutscene, "hay una cinemática en curso", "a cutscene is playing", "正在播放过场动画", "идёт сцена")
X(WarpPaused, "el menú de pausa está abierto", "the pause menu is open", "暂停菜单已打开", "открыто меню паузы")
X(WarpTalking, "estás en un diálogo", "you are in a dialogue", "你正在对话中", "вы в диалоге")
X(WarpRiding, "vas a caballo", "you are on horseback", "你正在骑马", "вы верхом")
