
#include "Game.h"
#include <iterator>
#include <filesystem>
#include <regex> 

#define BUILD_WITH_EASY_PROFILER
#include <easy/profiler.h>

#define timegm _mkgmtime

#define W_PI 3.14159265358979323846

int Game::init(int argc, char *argv[]) {
    //EASY_PROFILER_ENABLE;
    profiler::startListen();
    EASY_MAIN_THREAD;
    EASY_FUNCTION(GAME_PROFILER_COLOR);

    EASY_EVENT("Start Loading", profiler::colors::Magenta);

    // init console & print title
    #pragma region
    EASY_BLOCK("init console");
    if(_setmode(_fileno(stdout), _O_WTEXT) == -1) {
        std::wcout << "Error initting console: " << std::strerror(errno) << std::endl;
    }

    EASY_BLOCK("print ascii title");
    std::locale ulocale(locale(), new codecvt_utf8<wchar_t>);
    std::wifstream ifs("assets/title.txt");
    std::locale prevlocale = ifs.imbue(ulocale);
    std::wcout << ifs.rdbuf() << std::endl;
    EASY_END_BLOCK;

    EASY_BLOCK("start spdlog");
    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern("%D %T (%s:%#) [%^%l%$] \t%v");
    EASY_END_BLOCK;
    EASY_END_BLOCK;
    #pragma endregion

    logInfo("Starting game...");

    EASY_BLOCK("wait for client/server choice");
    char ch = getchar();
    EASY_END_BLOCK;

    // TODO: commandline option
    this->gameDir = "gamedir/";

    networkMode = ch == 's' ? NetworkMode::SERVER : NetworkMode::HOST;
    if(argc >= 2) {
        if(strstr(argv[1], "server")) {
            networkMode = NetworkMode::SERVER;
        }
    }
    Networking::init();
    if(networkMode == NetworkMode::SERVER) {
        int port = 1337;
        if(argc >= 3) {
            port = atoi(argv[2]);
        }
        server = Server::start(port);
        //SDL_SetWindowTitle(window, "Falling Sand Survival (Server)");

        /*while (true) {
            logDebug("[SERVER] tick {0:d}", server->server->connectedPeers);
            server->tick();
            Sleep(500);
        }
        return 0;*/

    } else {
        client = Client::start();
        //client->connect("172.23.16.150", 1337);

        //while (true) {
        //	logDebug("[CLIENT] tick");

        //	/* Create a reliable packet of size 7 containing "packet\0" */
        //	ENetPacket * packet = enet_packet_create("keepalive",
        //		strlen("keepalive") + 1, 0);
        //	/* Send the packet to the peer over channel id 0. */
        //	/* One could also broadcast the packet by         */
        //	/* enet_host_broadcast (host, 0, packet);         */
        //	enet_peer_send(client->peer, 0, packet);

        //	for (int i = 0; i < 1000; i++) {
        //		client->tick();
        //		Sleep(1);
        //	}
        //}
        //return 0;

        //SDL_SetWindowTitle(window, "Falling Sand Survival (Client)");
    }

    std::thread initFmodThread;
    if(networkMode != NetworkMode::SERVER) {
        #if BUILD_WITH_STEAM
        // SteamAPI_RestartAppIfNecessary
        #pragma region
        logInfo("SteamAPI_RestartAppIfNecessary...");
        EASY_BLOCK("SteamAPI_RestartAppIfNecessary", STEAM_PROFILER_COLOR);
        if(SteamAPI_RestartAppIfNecessary(STEAM_APPID)) {
            return 1; // restart game through steam (if ran directly)
        }
        EASY_END_BLOCK;
        #pragma endregion
        #endif

        // init fmod
        #pragma region
        initFmodThread = std::thread([&] {
            EASY_THREAD("Audio init");
            EASY_BLOCK("init fmod");
            logInfo("Initializing audio engine...");

            EASY_BLOCK("init audioEngine");
            audioEngine.Init();
            EASY_END_BLOCK;

            EASY_BLOCK("load banks");
            audioEngine.LoadBank("assets/audio/fmod/Build/Desktop/Master.bank", FMOD_STUDIO_LOAD_BANK_NORMAL);
            audioEngine.LoadBank("assets/audio/fmod/Build/Desktop/Master.strings.bank", FMOD_STUDIO_LOAD_BANK_NORMAL);
            EASY_END_BLOCK;

            EASY_BLOCK("load events");
            audioEngine.LoadEvent("event:/Title");
            audioEngine.LoadEvent("event:/Jump");
            audioEngine.LoadEvent("event:/Fly");
            audioEngine.LoadEvent("event:/Wind");
            audioEngine.LoadEvent("event:/Impact");
            audioEngine.LoadEvent("event:/Sand");
            EASY_END_BLOCK;

            EASY_BLOCK("play events");
            audioEngine.PlayEvent("event:/Fly");
            audioEngine.PlayEvent("event:/Wind");
            audioEngine.PlayEvent("event:/Sand");
            EASY_END_BLOCK;

            EASY_END_BLOCK; // init fmod
        });
        #pragma endregion
    }

    // init sdl
    #pragma region
    EASY_BLOCK("init SDL");
    EASY_BLOCK("SDL_Init", SDL_PROFILER_COLOR);
    logInfo("Initializing SDL...");
    uint32 sdl_init_flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
    if(SDL_Init(sdl_init_flags) < 0) {
        logCritical("SDL_Init failed: {}", SDL_GetError());
        return EXIT_FAILURE;
    }

    //SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "direct3d11", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "opengles2", SDL_HINT_OVERRIDE);
    EASY_END_BLOCK;

    EASY_BLOCK("TTF_Init");
    logInfo("Initializing SDL_TTF...");
    if(TTF_Init() != 0) {
        logCritical("TTF_Init failed: {}", TTF_GetError());
        return EXIT_FAILURE;
    }
    EASY_END_BLOCK;

    if(networkMode != NetworkMode::SERVER) {
        EASY_BLOCK("load fonts");
        font64 = TTF_OpenFont("assets/fonts/pixel_operator/PixelOperator.ttf", 64);
        font16 = TTF_OpenFont("assets/fonts/pixel_operator/PixelOperator.ttf", 16);
        font14 = TTF_OpenFont("assets/fonts/pixel_operator/PixelOperator.ttf", 14);
        EASY_END_BLOCK;
    }

    EASY_BLOCK("IMG_Init");
    logInfo("Initializing SDL_IMG...");
    int imgFlags = IMG_INIT_PNG;
    if(!(IMG_Init(imgFlags) & imgFlags)) {
        logCritical("IMG_Init failed: {}", IMG_GetError());
        return EXIT_FAILURE;
    }
    EASY_END_BLOCK;
    EASY_END_BLOCK;
    #pragma endregion

    if(networkMode != NetworkMode::SERVER) {
        // create the window
        #pragma region
        EASY_BLOCK("create window");
        logInfo("Creating game window...");
        EASY_BLOCK("SDL_CreateWindow", SDL_PROFILER_COLOR);
        window = SDL_CreateWindow("Falling Sand Survival", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL);
        EASY_END_BLOCK;

        if(window == nullptr) {
            logCritical("Could not create SDL_Window: {}", SDL_GetError());
            return EXIT_FAILURE;
        }

        EASY_BLOCK("SDL_SetWindowIcon", SDL_PROFILER_COLOR);
        SDL_SetWindowIcon(window, Textures::loadTexture("assets/Icon_32x.png"));
        EASY_END_BLOCK;
        EASY_END_BLOCK;
        #pragma endregion

        // create gpu target
        #pragma region
        EASY_BLOCK("create gpu target");
        logInfo("Creating gpu target...");
        EASY_BLOCK("GPU_SetPreInitFlags", GPU_PROFILER_COLOR);
        GPU_SetDebugLevel(GPU_DEBUG_LEVEL_MAX);
        GPU_SetPreInitFlags(GPU_INIT_DISABLE_VSYNC);
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetInitWindow", GPU_PROFILER_COLOR);
        GPU_SetInitWindow(SDL_GetWindowID(window));
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_Init", GPU_PROFILER_COLOR);
        target = GPU_Init(WIDTH, HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);
        EASY_END_BLOCK;

        if(target == NULL) {
            logCritical("Could not create GPU_Target: {}", SDL_GetError());
            return EXIT_FAILURE;
        }
        realTarget = target;
        EASY_END_BLOCK;
        #pragma endregion

        // load splash screen
        #pragma region
        EASY_BLOCK("load splash screen");
        logInfo("Loading splash screen...");
        EASY_BLOCK("GPU_Clear", GPU_PROFILER_COLOR);
        GPU_Clear(target);
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_Flip", GPU_PROFILER_COLOR);
        GPU_Flip(target);
        EASY_END_BLOCK;

        SDL_Surface* splashSurf = Textures::loadTexture("assets/title/splash.png");
        EASY_BLOCK("GPU_CopyImageFromSurface", GPU_PROFILER_COLOR);
        GPU_Image* splashImg = GPU_CopyImageFromSurface(splashSurf);
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(splashImg, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_BlitRect", GPU_PROFILER_COLOR);
        GPU_BlitRect(splashImg, NULL, target, NULL);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_FreeImage", GPU_PROFILER_COLOR);
        GPU_FreeImage(splashImg);
        EASY_END_BLOCK;
        EASY_BLOCK("SDL_FreeSurface", SDL_PROFILER_COLOR);
        SDL_FreeSurface(splashSurf);
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_Flip", GPU_PROFILER_COLOR);
        GPU_Flip(target);
        EASY_END_BLOCK;
        EASY_EVENT("splash screen visible", profiler::colors::Green);

        EASY_BLOCK("wait for Fmod", THREAD_WAIT_PROFILER_COLOR);
        initFmodThread.join();
        EASY_END_BLOCK;
        EASY_BLOCK("play audio");
        audioEngine.PlayEvent("event:/Title");
        audioEngine.Update();
        EASY_END_BLOCK;
        EASY_END_BLOCK;
        #pragma endregion

        #if BUILD_WITH_STEAM
        // load SteamAPI
        #pragma region
        logInfo("Initializing SteamAPI...");
        EASY_BLOCK("Init SteamAPI", STEAM_PROFILER_COLOR);
        EASY_BLOCK("SteamAPI_Init", STEAM_PROFILER_COLOR);
        steamAPI = SteamAPI_Init();
        EASY_END_BLOCK;
        if(!steamAPI) {
            logError("SteamAPI_Init failed.");
        } else {
            logInfo("SteamAPI_Init successful.");
            EASY_BLOCK("SteamHookMessages", STEAM_PROFILER_COLOR);
            SteamHookMessages();
            EASY_END_BLOCK;
        }
        EASY_END_BLOCK;
        #pragma endregion
        #endif

        #if BUILD_WITH_DISCORD
        // init discord
        #pragma region
        logInfo("Initializing Discord Game SDK...");
        EASY_BLOCK("init discord", DISCORD_PROFILER_COLOR);
        EASY_BLOCK("discord::Core::Create", DISCORD_PROFILER_COLOR);
        auto result = discord::Core::Create(DISCORD_CLIENTID, DiscordCreateFlags_NoRequireDiscord, &core);
        EASY_END_BLOCK;
        if(discordAPI = (result == discord::Result::Ok)) {
            logInfo("discord::Core::Create successful.");
            EASY_BLOCK("SetLogHook", DISCORD_PROFILER_COLOR);
            core->SetLogHook(discord::LogLevel::Debug, [](auto level, const char* txt) {
                switch(level) {
                case discord::LogLevel::Info:
                    logInfo("[DISCORD SDK] {}", txt);
                    break;
                case discord::LogLevel::Debug:
                    logDebug("[DISCORD SDK] {}", txt);
                    break;
                case discord::LogLevel::Warn:
                    logWarn("[DISCORD SDK] {}", txt);
                    break;
                case discord::LogLevel::Error:
                    logError("[DISCORD SDK] {}", txt);
                    break;
                }
            });
            EASY_END_BLOCK;
            EASY_BLOCK("set activity", DISCORD_PROFILER_COLOR);
            discord::Activity activity {};
            activity.SetDetails("In world: \"chunks\"");
            activity.GetAssets().SetLargeImage("largeicon");
            activity.GetAssets().SetLargeText("Falling Sand Survival");
            activity.SetType(discord::ActivityType::Playing);
            core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
                logInfo("[DISCORD] UpdateActivity returned {0:d}", result);
            });
            EASY_END_BLOCK;
        } else {
            logError("discord::Core::Create failed.");
        }
        EASY_END_BLOCK;
        #pragma endregion
        #endif

        // init the background
        #pragma region
        EASY_BLOCK("load backgrounds");
        logInfo("Loading backgrounds...");
        Backgrounds::TEST_OVERWORLD.init();
        EASY_END_BLOCK;
        #pragma endregion
    }

    // init the rng
    #pragma region
    EASY_BLOCK("seed RNG");
    logInfo("Seeding RNG...");
    unsigned int seed = (unsigned int)Time::millis();
    srand(seed);
    EASY_END_BLOCK;
    #pragma endregion

    // register & set up materials
    #pragma region
    EASY_BLOCK("init materials");
    logInfo("Setting up materials...");
    Materials::init();
    EASY_END_BLOCK;
    #pragma endregion

    // init the world
    #pragma region
    EASY_BLOCK("init world");
    logInfo("Initializing world...");
    world = new World();
    world->init((char*)getWorldDir("mainMenu").c_str(), (int)ceil(WIDTH / 3 / (double)CHUNK_W) * CHUNK_W + CHUNK_W * 3, (int)ceil(HEIGHT / 3 / (double)CHUNK_H) * CHUNK_H + CHUNK_H * 3, target, &audioEngine, networkMode);
    EASY_END_BLOCK;
    #pragma endregion

    if(networkMode != NetworkMode::SERVER) {
        // set up main menu ui
        #pragma region
        EASY_BLOCK("init main menu UI");
        logInfo("Setting up main menu...");
        TTF_Font* labelFont = TTF_OpenFont("assets/fonts/pixel_operator/PixelOperator.ttf", 32);
        TTF_Font* uiFont = TTF_OpenFont("assets/fonts/pixel_operator/PixelOperator.ttf", 16);

        mainMenuUI = new UI(new SDL_Rect {WIDTH / 2 - 200, HEIGHT / 2 - 250, 400, 500});
        mainMenuUI->background = new SolidBackground(0x80000000);
        mainMenuUI->drawBorder = true;

        int mainMenuButtonsWidth = 250;
        int mainMenuButtonsYOffset = 50;

        UILabel* mainMenuTitleLabel = new UILabel(new SDL_Rect {200, 10, 100, 30}, "Falling Sand Survival", labelFont, 0xffffff, ALIGN_CENTER);
        mainMenuUI->children.push_back(mainMenuTitleLabel);

        int connectWidth = 200;
        UIButton* connectButton;
        UITextArea* connectInput = new UITextArea(new SDL_Rect {200 - connectWidth / 2 - 60 / 2, 60, connectWidth, 20}, "", "ip:port", uiFont);
        connectInput->maxLength = 24;
        connectInput->callback = [&](std::string text) {
            regex connectInputRegex("([^:]+):(\\d+)");
            connectButton->disabled = !regex_match(text, connectInputRegex);
        };
        mainMenuUI->children.push_back(connectInput);

        connectButton = new UIButton(new SDL_Rect {connectInput->bounds->x + connectInput->bounds->w + 4, connectInput->bounds->y, 60, 20}, "Connect", uiFont, 0xffffff, ALIGN_CENTER);
        connectButton->drawBorder = true;
        connectButton->disabled = true;
        connectButton->selectCallback = [&]() {
            logInfo("connectButton select");
            if(client->connect("172.23.16.150", 1337)) {
                networkMode = NetworkMode::CLIENT;
                mainMenuUI->visible = false;
                state = LOADING;
                stateAfterLoad = INGAME;
            }
        };
        mainMenuUI->children.push_back(connectButton);


        UIButton* mainMenuNewButton = new UIButton(new SDL_Rect {200 - mainMenuButtonsWidth / 2, 60 + mainMenuButtonsYOffset, mainMenuButtonsWidth, 40}, "New", labelFont, 0xffffff, ALIGN_CENTER);
        mainMenuNewButton->drawBorder = true;
        mainMenuUI->children.push_back(mainMenuNewButton);

        mainMenuNewButton->selectCallback = [&]() {
            char* wn = (char*)"newWorld";
            logInfo("Selected world: {}", wn);
            mainMenuUI->visible = false;
            state = LOADING;
            stateAfterLoad = INGAME;

            EASY_BLOCK("Close world");
            delete world;
            world = nullptr;
            EASY_END_BLOCK;

            EASY_BLOCK("Load world");
            world = new World();
            world->init((char*)getWorldDir(wn).c_str(), (int)ceil(WIDTH / 3 / (double)CHUNK_W) * CHUNK_W + CHUNK_W * 3, (int)ceil(HEIGHT / 3 / (double)CHUNK_H) * CHUNK_H + CHUNK_H * 3, target, &audioEngine, networkMode);

            EASY_BLOCK("Queue chunk loading");
            logInfo("Queueing chunk loading...");
            for(int x = -CHUNK_W * 4; x < world->width + CHUNK_W * 4; x += CHUNK_W) {
                for(int y = -CHUNK_H * 3; y < world->height + CHUNK_H * 8; y += CHUNK_H) {
                    world->queueLoadChunk(x / CHUNK_W, y / CHUNK_H, true, true);
                }
            }
            EASY_END_BLOCK;
            EASY_END_BLOCK;
        };

        int nMainMenuButtons = 1;
        for(auto& p : experimental::filesystem::directory_iterator(getFileInGameDir("worlds/"))) {
            string worldName = p.path().filename().generic_string();

            WorldMeta meta = WorldMeta::loadWorldMeta((char*)getWorldDir(worldName).c_str());

            UIButton* worldButton = new UIButton(new SDL_Rect {200 - mainMenuButtonsWidth / 2, 60 + mainMenuButtonsYOffset + (nMainMenuButtons++ * 60), mainMenuButtonsWidth, 50}, meta.worldName.c_str(), labelFont, 0xffffff, ALIGN_CENTER);

            tm* tm_utc = gmtime(&meta.lastOpenedTime);

            // convert to local time
            time_t time_utc = timegm(tm_utc);
            time_t time_local = mktime(tm_utc);
            time_local += time_utc - time_local;
            tm* tm_local = localtime(&time_local);

            char* formattedTime = new char[100];
            strftime(formattedTime, 100, "%D %I:%M %p", tm_local);

            char* filenameAndTimestamp = new char[200];
            snprintf(filenameAndTimestamp, 100, "%s (%s)", worldName.c_str(), formattedTime);

            UILabel* worldFileName = new UILabel(new SDL_Rect {mainMenuButtonsWidth / 2, 30, 0, 0}, filenameAndTimestamp, font16, 0xcccccc, ALIGN_CENTER);
            worldButton->children.push_back(worldFileName);
            worldButton->drawBorder = true;

            mainMenuUI->children.push_back(worldButton);

            worldButton->selectCallback = [&, worldName]() {
                logInfo("Selected world: {}", worldName.c_str());
                mainMenuUI->visible = false;

                fadeOutStart = now;
                fadeOutLength = 250;
                fadeOutCallback = [&, worldName]() {
                    state = LOADING;
                    stateAfterLoad = INGAME;

                    EASY_BLOCK("Close world");
                    delete world;
                    world = nullptr;
                    EASY_END_BLOCK;

                    //std::thread loadWorldThread([&] () {
                    EASY_BLOCK("Load world");
                    World* w = new World();
                    w->init((char*)getWorldDir(worldName).c_str(), (int)ceil(WIDTH / 3 / (double)CHUNK_W) * CHUNK_W + CHUNK_W * 3, (int)ceil(HEIGHT / 3 / (double)CHUNK_H) * CHUNK_H + CHUNK_H * 3, target, &audioEngine, networkMode);

                    EASY_BLOCK("Queue chunk loading");
                    logInfo("Queueing chunk loading...");
                    for(int x = -CHUNK_W * 4; x < w->width + CHUNK_W * 4; x += CHUNK_W) {
                        for(int y = -CHUNK_H * 3; y < w->height + CHUNK_H * 8; y += CHUNK_H) {
                            w->queueLoadChunk(x / CHUNK_W, y / CHUNK_H, true, true);
                        }
                    }
                    EASY_END_BLOCK;
                    EASY_END_BLOCK;

                    world = w;
                    //});

                    fadeInStart = now;
                    fadeInLength = 250;
                    fadeInWaitFrames = 4;
                };
            };

        }

        uis.push_back(mainMenuUI);
        EASY_END_BLOCK;
        #pragma endregion

        // set up debug ui
        #pragma region
        logInfo("Setting up debug UI...");
        EASY_BLOCK("init debug UI");
        debugUI = new UI(new SDL_Rect {WIDTH - 200 - 15, 25, 200, 350});
        debugUI->background = new SolidBackground(0x80000000);
        debugUI->drawBorder = true;

        UILabel* titleLabel = new UILabel(new SDL_Rect {100, 10, 1, 30}, "Debug", labelFont, 0xffffff, ALIGN_CENTER);
        debugUI->children.push_back(titleLabel);

        uis.push_back(debugUI);
        #pragma endregion

        // set up debug ui checkboxes
        #pragma region
        int checkIndex = 0;

        #define SETTINGS_CHECK(settingName, v_cb) { \
	UICheckbox* check = new UICheckbox(new SDL_Rect{ 20, 50 + 18 * checkIndex++, 12, 12 }, Settings::settingName); \
	check->drawBorder = true; \
	debugUI->children.push_back(check); \
	check->callback = [&](bool checked) { \
	Settings::settingName = checked; \
	v_cb(checked); }; \
	UILabel* checkLabel = new UILabel(new SDL_Rect{ 16, 0, 150, 14 }, QUOTE(settingName), uiFont, 0xffffff, ALIGN_LEFT); \
	check->children.push_back(checkLabel); }

        SETTINGS_CHECK(draw_frame_graph, [](bool checked) {});

        SETTINGS_CHECK(draw_background, [&](bool checked) {
            for(int x = 0; x < world->width; x++) {
                for(int y = 0; y < world->height; y++) {
                    world->dirty[x + y * world->width] = true;
                    world->layer2Dirty[x + y * world->width] = true;
                }
            }
        });

        SETTINGS_CHECK(draw_background_grid, [&](bool checked) {
            for(int x = 0; x < world->width; x++) {
                for(int y = 0; y < world->height; y++) {
                    world->dirty[x + y * world->width] = true;
                    world->layer2Dirty[x + y * world->width] = true;
                }
            }
        });

        SETTINGS_CHECK(draw_load_zones, [](bool checked) {});
        SETTINGS_CHECK(draw_physics_meshes, [](bool checked) {});
        SETTINGS_CHECK(draw_chunk_state, [](bool checked) {});
        SETTINGS_CHECK(draw_chunk_queue, [](bool checked) {});
        SETTINGS_CHECK(draw_material_info, [](bool checked) {});
        SETTINGS_CHECK(draw_uinode_bounds, [](bool checked) {});
        SETTINGS_CHECK(draw_light_map, [](bool checked) {});
        SETTINGS_CHECK(draw_temperature_map, [](bool checked) {});
        SETTINGS_CHECK(draw_shaders, [](bool checked) {});
        SETTINGS_CHECK(tick_world, [](bool checked) {});
        SETTINGS_CHECK(tick_box2d, [](bool checked) {});
        SETTINGS_CHECK(tick_temperature, [](bool checked) {});
        #undef SETTINGS_CHECK
        #pragma endregion

        // set up debug draw ui
        #pragma region
        debugDrawUI = new UI(new SDL_Rect {15, 25, 200, (int)(80 + 37 * (Materials::MATERIALS.size() / 5 + 1) + 5)});
        debugDrawUI->background = new SolidBackground(0x80000000);
        debugDrawUI->drawBorder = true;

        titleLabel = new UILabel(new SDL_Rect {100, 10, 1, 30}, "Draw", labelFont, 0xffffff, ALIGN_CENTER);
        debugDrawUI->children.push_back(titleLabel);

        UILabel* hoverMaterialLabelL = new UILabel(new SDL_Rect {15, 40, 1, 30}, "Hover:", uiFont, 0xffffff, ALIGN_LEFT);
        debugDrawUI->children.push_back(hoverMaterialLabelL);

        UILabel* hoverMaterialLabel = new UILabel(new SDL_Rect {75, 40, 1, 30}, "None", uiFont, 0xffffff, ALIGN_LEFT);
        debugDrawUI->children.push_back(hoverMaterialLabel);

        UILabel* selectMaterialLabelL = new UILabel(new SDL_Rect {15, 52, 1, 30}, "Selected:", uiFont, 0xffffff, ALIGN_LEFT);
        debugDrawUI->children.push_back(selectMaterialLabelL);

        UILabel* selectMaterialLabel = new UILabel(new SDL_Rect {75, 52, 1, 30}, "None", uiFont, 0xffffff, ALIGN_LEFT);
        debugDrawUI->children.push_back(selectMaterialLabel);

        UILabel* brushSizeLabelL = new UILabel(new SDL_Rect {15, 64, 1, 30}, "Brush size:", uiFont, 0xffffff, ALIGN_LEFT);
        debugDrawUI->children.push_back(brushSizeLabelL);

        this->brushSizeLabel = new UILabel(new SDL_Rect {85, 64, 1, 30}, std::to_string(brushSize), uiFont, 0xffffff, ALIGN_LEFT);
        debugDrawUI->children.push_back(this->brushSizeLabel);

        for(size_t i = 0; i < Materials::MATERIALS.size(); i++) {
            MaterialNode* mn = new MaterialNode(new SDL_Rect {10 + 37 * (int)(i % 5), 80 + 37 * (int)(i / 5), 16, 16}, Materials::MATERIALS[i]);
            mn->bounds->w = 32;
            mn->bounds->h = 32;
            mn->drawBorder = true;
            mn->hoverCallback = [hoverMaterialLabel](Material* mat) {
                hoverMaterialLabel->text = mat->name;
                hoverMaterialLabel->updateTexture();
            };
            mn->selectCallback = [&](Material* mat) {
                selectMaterialLabel->text = mat->name;
                selectMaterialLabel->updateTexture();
                selectedMaterial = mat;
            };
            debugDrawUI->children.push_back(mn);
        }

        uis.push_back(debugDrawUI);
        EASY_END_BLOCK;
        #pragma endregion

        // create textures
        #pragma region
        EASY_BLOCK("create textures");
        logInfo("Creating world textures...");
        loadingOnColor = 0xFFFFFFFF;
        loadingOffColor = 0x000000FF;
        EASY_BLOCK("GPU_CreateImage", GPU_PROFILER_COLOR);
        loadingTexture = GPU_CreateImage(
            loadingScreenW = (WIDTH / 20), loadingScreenH = (HEIGHT / 20),
            GPU_FormatEnum::GPU_FORMAT_RGBA
        );
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(loadingTexture, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_CreateImage", GPU_PROFILER_COLOR);
        texture = GPU_CreateImage(
            world->width, world->height,
            GPU_FormatEnum::GPU_FORMAT_RGBA
        );
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(texture, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_CreateImage", GPU_PROFILER_COLOR);
        worldTexture = GPU_CreateImage(
            world->width, world->height,
            GPU_FormatEnum::GPU_FORMAT_RGBA
        );
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(worldTexture, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_LoadTarget", GPU_PROFILER_COLOR);
        GPU_LoadTarget(worldTexture);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_CreateImage", GPU_PROFILER_COLOR);
        lightingTexture = GPU_CreateImage(
            world->width, world->height,
            GPU_FormatEnum::GPU_FORMAT_RGBA
        );
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(lightingTexture, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_LoadTarget", GPU_PROFILER_COLOR);
        GPU_LoadTarget(lightingTexture);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_CreateImage", GPU_PROFILER_COLOR);
        textureFire = GPU_CreateImage(
            world->width, world->height,
            GPU_FormatEnum::GPU_FORMAT_RGBA
        );
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(textureFire, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_CreateImage", GPU_PROFILER_COLOR);
        texture2Fire = GPU_CreateImage(
            world->width, world->height,
            GPU_FormatEnum::GPU_FORMAT_RGBA
        );
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(texture2Fire, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_LoadTarget", GPU_PROFILER_COLOR);
        GPU_LoadTarget(texture2Fire);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_CreateImage", GPU_PROFILER_COLOR);
        textureLayer2 = GPU_CreateImage(
            world->width, world->height,
            GPU_FormatEnum::GPU_FORMAT_RGBA
        );
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(textureLayer2, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_CreateImage", GPU_PROFILER_COLOR);
        textureBackground = GPU_CreateImage(
            world->width, world->height,
            GPU_FormatEnum::GPU_FORMAT_RGBA
        );
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(textureBackground, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_CreateImage", GPU_PROFILER_COLOR);
        textureObjects = GPU_CreateImage(
            world->width, world->height,
            GPU_FormatEnum::GPU_FORMAT_RGBA
        );
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(textureObjects, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_CreateImage", GPU_PROFILER_COLOR);
        textureObjectsBack = GPU_CreateImage(
            world->width, world->height,
            GPU_FormatEnum::GPU_FORMAT_RGBA
        );
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(textureObjectsBack, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_LoadTarget", GPU_PROFILER_COLOR);
        GPU_LoadTarget(textureObjects);
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_LoadTarget", GPU_PROFILER_COLOR);
        GPU_LoadTarget(textureObjectsBack);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_CreateImage", GPU_PROFILER_COLOR);
        textureParticles = GPU_CreateImage(
            world->width, world->height,
            GPU_FormatEnum::GPU_FORMAT_RGBA
        );
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(textureParticles, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_CreateImage", GPU_PROFILER_COLOR);
        textureEntities = GPU_CreateImage(
            world->width, world->height,
            GPU_FormatEnum::GPU_FORMAT_RGBA
        );
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_LoadTarget", GPU_PROFILER_COLOR);
        GPU_LoadTarget(textureEntities);
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(textureEntities, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_CreateImage", GPU_PROFILER_COLOR);
        lightMap = GPU_CreateImage(
            world->width - CHUNK_W * 2, world->height - CHUNK_H * 2,
            GPU_FormatEnum::GPU_FORMAT_RGBA
        );
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(lightMap, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_CreateImage", GPU_PROFILER_COLOR);
        temperatureMap = GPU_CreateImage(
            world->width, world->height,
            GPU_FormatEnum::GPU_FORMAT_RGBA
        );
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(temperatureMap, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;

        EASY_BLOCK("GPU_CreateImage", GPU_PROFILER_COLOR);
        backgroundImage = GPU_CreateImage(
            WIDTH, HEIGHT,
            GPU_FormatEnum::GPU_FORMAT_RGBA
        );
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_SetImageFilter", GPU_PROFILER_COLOR);
        GPU_SetImageFilter(backgroundImage, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;
        EASY_BLOCK("GPU_LoadTarget", GPU_PROFILER_COLOR);
        GPU_LoadTarget(backgroundImage);
        EASY_END_BLOCK;

        // create texture pixel buffers
        EASY_BLOCK("create texture pixel buffers");
        pixels = vector<unsigned char>(world->width * world->height * 4, 0);
        pixels_ar = &pixels[0];
        pixelsLayer2 = vector<unsigned char>(world->width * world->height * 4, 0);
        pixelsLayer2_ar = &pixelsLayer2[0];
        pixelsBackground = vector<unsigned char>(world->width * world->height * 4, 0);
        pixelsBackground_ar = &pixelsBackground[0];
        pixelsObjects = vector<unsigned char>(world->width * world->height * 4, SDL_ALPHA_TRANSPARENT);
        pixelsObjects_ar = &pixelsObjects[0];
        pixelsTemp = vector<unsigned char>(world->width * world->height * 4, SDL_ALPHA_TRANSPARENT);
        pixelsTemp_ar = &pixelsTemp[0];
        pixelsParticles = vector<unsigned char>(world->width * world->height * 4, SDL_ALPHA_TRANSPARENT);
        pixelsParticles_ar = &pixelsParticles[0];
        pixelsLoading = vector<unsigned char>(loadingTexture->w * loadingTexture->h * 4, SDL_ALPHA_TRANSPARENT);
        pixelsLoading_ar = &pixelsLoading[0];
        pixelsFire = vector<unsigned char>(world->width * world->height * 4, 0);
        pixelsFire_ar = &pixelsFire[0];
        EASY_END_BLOCK;
        EASY_END_BLOCK;
        #pragma endregion
    }

    // init threadpools
    #pragma region
    EASY_BLOCK("init threadpools");
    updateDirtyPool = new ctpl::thread_pool(6);
    rotateVectorsPool = new ctpl::thread_pool(3);
    EASY_END_BLOCK;
    #pragma endregion

    if(networkMode != NetworkMode::SERVER) {
        // load light texture
        #pragma region
        EASY_BLOCK("load light texture");
        lightSurf = Textures::loadTexture("assets/light.png");
        lightTex = GPU_CopyImageFromSurface(lightSurf);
        GPU_SetImageFilter(lightTex, GPU_FILTER_NEAREST);
        EASY_END_BLOCK;
        #pragma endregion

        // load shaders
        #pragma region
        EASY_BLOCK("load shaders");
        water_shader = 0;
        water_shader_block = Shaders::load_shader_program(&water_shader, "data/shaders/common.vert", "data/shaders/water.frag");
        Shaders::prepare_water_shader(water_shader, target, texture);

        raycastLightingShader = new RaycastLightingShader();
        simpleLightingShader = new SimpleLightingShader();
        simpleLighting2Shader = new SimpleLighting2Shader();
        fireShader = new FireShader();
        fire2Shader = new Fire2Shader();
        EASY_END_BLOCK;
        #pragma endregion
    }

    EASY_EVENT("Done Loading", profiler::colors::Magenta);
    EASY_END_BLOCK;

    return this->run(argc, argv);
}

int Game::run(int argc, char *argv[]) {
    EASY_FUNCTION(GAME_PROFILER_COLOR);
    startTime = Time::millis();

    // start loading chunks
    #pragma region
    EASY_BLOCK("Queue chunk loading");
    logInfo("Queueing chunk loading...");
    for(int x = -CHUNK_W * 4; x < world->width + CHUNK_W * 4; x += CHUNK_W) {
        for(int y = -CHUNK_H * 3; y < world->height + CHUNK_H * 8; y += CHUNK_H) {
            world->queueLoadChunk(x / CHUNK_W, y / CHUNK_H, true, true);
        }
    }
    EASY_END_BLOCK;
    #pragma endregion

    // start game loop
    #pragma region
    EASY_BLOCK("start game loop");
    logInfo("Starting game loop...");

    freeCamX = world->width / 2 - CHUNK_W / 2;
    freeCamY = world->height / 2 - (int)(CHUNK_H * 0.75);
    if(world->player) {
        plPosX = world->player->x;
        plPosY = world->player->y;
    } else {
        plPosX = freeCamX;
        plPosY = freeCamY;
    }

    SDL_Event windowEvent;

    long long lastFPS = Time::millis();
    int frames = 0;
    fps = 0;

    lastTime = Time::millis();
    lastTick = lastTime;
    long long lastTickPhysics = lastTime;

    mspt = 33;
    long msptPhysics = 16;

    scale = 2;
    ofsX = (int)(-CHUNK_W * 2);
    ofsY = (int)(-CHUNK_H * 2.5);

    for(int i = 0; i < frameTimeNum; i++) {
        frameTime[i] = 0;
    }
    objectDelete = new bool[world->width * world->height];

    EASY_END_BLOCK; // start game loop
    #pragma endregion

    fadeInStart = Time::millis();
    fadeInLength = 250;
    fadeInWaitFrames = 5;

    // game loop
    EASY_EVENT("Start of Game Loop", profiler::colors::Magenta);
    while(true) {
        EASY_BLOCK("frame");
        now = Time::millis();
        long long deltaTime = now - lastTime;

        if(networkMode != NetworkMode::SERVER) {
            #if BUILD_WITH_DISCORD
            if(discordAPI) {
                EASY_BLOCK("DiscordGameSDK tick");
                core->RunCallbacks();
                EASY_END_BLOCK;
            }
            #endif

            #if BUILD_WITH_STEAM
            if(steamAPI) {
                EASY_BLOCK("SteamAPI tick");

                EASY_END_BLOCK;
            }
            #endif

            // handle window events
            #pragma region
            EASY_BLOCK("poll SDL events");
            while(SDL_PollEvent(&windowEvent)) {

                if(windowEvent.type == SDL_QUIT) {
                    goto exit;
                }

                bool discardEvent = false;
                for(auto& v : uis) {
                    if(v->visible && v->checkEvent(windowEvent, target, world, 0, 0)) {
                        discardEvent = true;
                        break;
                    }
                }
                if(discardEvent) continue;

                if(windowEvent.type == SDL_MOUSEWHEEL) {
                    // zoom in/out
                    #pragma region
                    int deltaScale = windowEvent.wheel.y;
                    int oldScale = scale;
                    scale += deltaScale;
                    if(scale < 1) scale = 1;

                    ofsX = (ofsX - WIDTH / 2) / oldScale * scale + WIDTH / 2;
                    ofsY = (ofsY - HEIGHT / 2) / oldScale * scale + HEIGHT / 2;
                    #pragma endregion
                } else if(windowEvent.type == SDL_MOUSEMOTION && Controls::DEBUG_DRAW->get()) {
                    // draw material
                    #pragma region
                    int x = (int)((windowEvent.motion.x - ofsX - camX) / scale);
                    int y = (int)((windowEvent.motion.y - ofsY - camY) / scale);
                    for(int xx = -brushSize / 2; xx < (int)(ceil(brushSize / 2.0)); xx++) {
                        for(int yy = -brushSize / 2; yy < (int)(ceil(brushSize / 2.0)); yy++) {
                            MaterialInstance tp = Tiles::create(selectedMaterial, x + xx, y + yy);
                            world->tiles[(x + xx) + (y + yy) * world->width] = tp;
                            world->dirty[(x + xx) + (y + yy) * world->width] = true;
                            /*Particle* p = new Particle(tp, x + xx, y + yy, 0, 0, 0, (float)0.01f);
                            p->targetX = world->width/2;
                            p->targetY = world->height/2;
                            p->targetForce = 0.1f;
                            p->phase = true;
                            world->addParticle(p);*/
                        }
                    }
                    #pragma endregion
                } else if(windowEvent.type == SDL_MOUSEMOTION && Controls::mmouse) {
                    // erase material
                    #pragma region
                    // erase from world
                    int x = (int)((windowEvent.motion.x - ofsX - camX) / scale);
                    int y = (int)((windowEvent.motion.y - ofsY - camY) / scale);
                    for(int xx = -brushSize / 2; xx < (int)(ceil(brushSize / 2.0)); xx++) {
                        for(int yy = -brushSize / 2; yy < (int)(ceil(brushSize / 2.0)); yy++) {

                            if(abs(xx) + abs(yy) == brushSize) continue;
                            if(world->getTile(x + xx, y + yy).mat->physicsType != PhysicsType::AIR) {
                                world->setTile(x + xx, y + yy, Tiles::NOTHING);
                                world->lastMeshZone.x--;
                            }
                            if(world->getTileLayer2(x + xx, y + yy).mat->physicsType != PhysicsType::AIR) {
                                world->setTileLayer2(x + xx, y + yy, Tiles::NOTHING);
                            }
                        }
                    }

                    // erase from rigidbodies
                    // this copies the vector
                    vector<RigidBody*> rbs = world->rigidBodies;

                    for(size_t i = 0; i < rbs.size(); i++) {
                        RigidBody* cur = rbs[i];
                        if(cur->body->IsEnabled()) {
                            float s = sin(-cur->body->GetAngle());
                            float c = cos(-cur->body->GetAngle());
                            bool upd = false;
                            for(float xx = -3; xx <= 3; xx += 0.5) {
                                for(float yy = -3; yy <= 3; yy += 0.5) {
                                    if(abs(xx) + abs(yy) == 6) continue;
                                    // rotate point

                                    float tx = x + xx - cur->body->GetPosition().x;
                                    float ty = y + yy - cur->body->GetPosition().y;

                                    int ntx = (int)(tx * c - ty * s);
                                    int nty = (int)(tx * s + ty * c);

                                    if(ntx >= 0 && nty >= 0 && ntx < cur->surface->w && nty < cur->surface->h) {
                                        Uint32 pixel = PIXEL(cur->surface, ntx, nty);
                                        if(((pixel >> 24) & 0xff) != 0x00) {
                                            PIXEL(cur->surface, ntx, nty) = 0x00000000;
                                            upd = true;
                                        }

                                    }
                                }
                            }

                            if(upd) {
                                GPU_FreeImage(cur->texture);
                                cur->texture = GPU_CopyImageFromSurface(cur->surface);
                                GPU_SetImageFilter(cur->texture, GPU_FILTER_NEAREST);
                                //world->updateRigidBodyHitbox(cur);
                                cur->needsUpdate = true;
                            }
                        }
                    }

                    #pragma endregion
                } else if(windowEvent.type == SDL_KEYDOWN) {
                    EASY_BLOCK("Controls::keyEvent");
                    Controls::keyEvent(windowEvent.key);
                    EASY_END_BLOCK;
                } else if(windowEvent.type == SDL_KEYUP) {
                    EASY_BLOCK("Controls::keyEvent");
                    Controls::keyEvent(windowEvent.key);
                    EASY_END_BLOCK;
                }

                if(windowEvent.type == SDL_MOUSEBUTTONDOWN) {
                    if(windowEvent.button.button == SDL_BUTTON_LEFT) {
                        Controls::lmouse = true;

                        if(world->player && world->player->heldItem != NULL) {
                            if(world->player->heldItem->getFlag(ItemFlags::VACUUM)) {
                                world->player->holdVacuum = true;
                            } else if(world->player->heldItem->getFlag(ItemFlags::HAMMER)) {
                                //#define HAMMER_DEBUG_PHYSICS
                                #ifdef HAMMER_DEBUG_PHYSICS
                                int x = (int)((windowEvent.button.x - ofsX - camX) / scale);
                                int y = (int)((windowEvent.button.y - ofsY - camY) / scale);

                                world->physicsCheck(x, y);
                                #else
                                mx = windowEvent.button.x;
                                my = windowEvent.button.y;
                                int startInd = getAimSolidSurface(64);

                                if(startInd != -1) {
                                    //world->player->hammerX = x;
                                    //world->player->hammerY = y;
                                    world->player->hammerX = startInd % world->width;
                                    world->player->hammerY = startInd / world->width;
                                    world->player->holdHammer = true;
                                    //logDebug("hammer down: {0:d} {0:d} {0:d} {0:d} {0:d}", x, y, startInd, startInd % world->width, startInd / world->width);
                                    //world->setTile(world->player->hammerX, world->player->hammerY, MaterialInstance(&Materials::GENERIC_SOLID, 0x00ff00ff));
                                }
                                #endif
                                #undef HAMMER_DEBUG_PHYSICS
                            } else if(world->player->heldItem->getFlag(ItemFlags::CHISEL)) {
                                // if hovering rigidbody, open in chisel
                                #pragma region
                                int x = (int)((mx - ofsX - camX) / scale);
                                int y = (int)((my - ofsY - camY) / scale);

                                vector<RigidBody*> rbs = world->rigidBodies; // copy
                                for(size_t i = 0; i < rbs.size(); i++) {
                                    RigidBody* cur = rbs[i];

                                    bool connect = false;
                                    if(cur->body->IsEnabled()) {
                                        float s = sin(-cur->body->GetAngle());
                                        float c = cos(-cur->body->GetAngle());
                                        bool upd = false;
                                        for(float xx = -3; xx <= 3; xx += 0.5) {
                                            for(float yy = -3; yy <= 3; yy += 0.5) {
                                                if(abs(xx) + abs(yy) == 6) continue;
                                                // rotate point

                                                float tx = x + xx - cur->body->GetPosition().x;
                                                float ty = y + yy - cur->body->GetPosition().y;

                                                int ntx = (int)(tx * c - ty * s);
                                                int nty = (int)(tx * s + ty * c);

                                                if(ntx >= 0 && nty >= 0 && ntx < cur->surface->w && nty < cur->surface->h) {
                                                    Uint32 pixel = PIXEL(cur->surface, ntx, nty);
                                                    if(((pixel >> 24) & 0xff) != 0x00) {
                                                        connect = true;
                                                    }

                                                }
                                            }
                                        }
                                    }

                                    if(connect) {

                                        chiselUI = new UI(new SDL_Rect {0, 0, cur->surface->w * 4 + 10 + 10, cur->surface->h * 4 + 10 + 40});
                                        chiselUI->bounds->x = WIDTH / 2 - chiselUI->bounds->w / 2;
                                        chiselUI->bounds->y = HEIGHT / 2 - chiselUI->bounds->h / 2;

                                        UILabel* chiselUITitleLabel = new UILabel(new SDL_Rect {chiselUI->bounds->w / 2, 10, 1, 30}, "Chisel", TTF_OpenFont("assets/fonts/pixel_operator/PixelOperator.ttf", 32), 0xffffff, ALIGN_CENTER);
                                        chiselUI->children.push_back(chiselUITitleLabel);

                                        chiselUI->background = new SolidBackground(0xC0505050);
                                        chiselUI->drawBorder = true;

                                        ChiselNode* chisel = new ChiselNode(new SDL_Rect {10, 40, chiselUI->bounds->w - 20, chiselUI->bounds->h - 10 - 40});
                                        chisel->drawBorder = true;
                                        chisel->rb = cur;
                                        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(cur->surface->flags, cur->surface->w, cur->surface->h, cur->surface->format->BitsPerPixel, cur->surface->format->format);
                                        SDL_BlitSurface(cur->surface, NULL, surf, NULL);
                                        chisel->surface = surf;
                                        chisel->texture = GPU_CopyImageFromSurface(surf);
                                        GPU_SetImageFilter(chisel->texture, GPU_FILTER_NEAREST);
                                        chiselUI->children.push_back(chisel);

                                        uis.push_back(chiselUI);

                                        break;
                                    }

                                }
                                #pragma endregion
                            } else if(world->player->heldItem->getFlag(ItemFlags::TOOL)) {
                                // break with pickaxe
                                #pragma region
                                float breakSize = world->player->heldItem->breakSize;

                                int x = (int)(world->player->x + world->player->hw / 2 + world->loadZone.x + 10 * (float)cos((world->player->holdAngle + 180) * 3.1415f / 180.0f) - breakSize / 2);
                                int y = (int)(world->player->y + world->player->hh / 2 + world->loadZone.y + 10 * (float)sin((world->player->holdAngle + 180) * 3.1415f / 180.0f) - breakSize / 2);

                                SDL_Surface* tex = SDL_CreateRGBSurfaceWithFormat(0, (int)breakSize, (int)breakSize, 32, SDL_PIXELFORMAT_ARGB8888);

                                int n = 0;
                                for(int xx = 0; xx < breakSize; xx++) {
                                    for(int yy = 0; yy < breakSize; yy++) {
                                        float cx = (float)((xx / breakSize) - 0.5);
                                        float cy = (float)((yy / breakSize) - 0.5);

                                        if(cx * cx + cy * cy > 0.25f) continue;

                                        if(world->tiles[(x + xx) + (y + yy) * world->width].mat->physicsType == PhysicsType::SOLID) {
                                            PIXEL(tex, xx, yy) = world->tiles[(x + xx) + (y + yy) * world->width].color;
                                            world->tiles[(x + xx) + (y + yy) * world->width] = Tiles::NOTHING;
                                            world->dirty[(x + xx) + (y + yy) * world->width] = true;

                                            n++;
                                        }
                                    }
                                }

                                if(n > 0) {
                                    audioEngine.PlayEvent("event:/Impact");
                                    b2PolygonShape s;
                                    s.SetAsBox(1, 1);
                                    RigidBody* rb = world->makeRigidBody(b2_dynamicBody, (float)x, (float)y, 0, s, 1, (float)0.3, tex);

                                    b2Filter bf = {};
                                    bf.categoryBits = 0x0001;
                                    bf.maskBits = 0xffff;
                                    rb->body->GetFixtureList()[0].SetFilterData(bf);

                                    rb->body->SetLinearVelocity({(float)((rand() % 100) / 100.0 - 0.5), (float)((rand() % 100) / 100.0 - 0.5)});

                                    world->rigidBodies.push_back(rb);
                                    world->updateRigidBodyHitbox(rb);

                                    world->lastMeshLoadZone.x--;
                                    world->updateWorldMesh();
                                }
                                #pragma endregion
                            }
                        }

                    } else if(windowEvent.button.button == SDL_BUTTON_RIGHT) {
                        Controls::rmouse = true;
                        if(world->player) world->player->startThrow = Time::millis();
                    } else if(windowEvent.button.button == SDL_BUTTON_MIDDLE) {
                        Controls::mmouse = true;
                    }
                } else if(windowEvent.type == SDL_MOUSEBUTTONUP) {
                    if(windowEvent.button.button == SDL_BUTTON_LEFT) {
                        Controls::lmouse = false;

                        if(world->player) {
                            if(world->player->heldItem) {
                                if(world->player->heldItem->getFlag(ItemFlags::VACUUM)) {
                                    if(world->player->holdVacuum) {
                                        world->player->holdVacuum = false;
                                    }
                                } else if(world->player->heldItem->getFlag(ItemFlags::HAMMER)) {
                                    if(world->player->holdHammer) {
                                        int x = (int)((windowEvent.button.x - ofsX - camX) / scale);
                                        int y = (int)((windowEvent.button.y - ofsY - camY) / scale);

                                        int dx = world->player->hammerX - x;
                                        int dy = world->player->hammerY - y;
                                        float len = sqrtf(dx * dx + dy * dy);
                                        float udx = dx / len;
                                        float udy = dy / len;

                                        int ex = world->player->hammerX + dx;
                                        int ey = world->player->hammerY + dy;
                                        logDebug("hammer up: {0:d} {0:d} {0:d} {0:d}", ex, ey, dx, dy);
                                        int endInd = -1;

                                        int nSegments = 1 + len / 10;
                                        std::vector<std::tuple<int, int>> points = {};
                                        for(int i = 0; i < nSegments; i++) {
                                            int sx = world->player->hammerX + (int)((float)(dx / nSegments) * (i + 1));
                                            int sy = world->player->hammerY + (int)((float)(dy / nSegments) * (i + 1));
                                            sx += rand() % 3 - 1;
                                            sy += rand() % 3 - 1;
                                            points.push_back(std::tuple<int, int>(sx, sy));
                                        }

                                        int nTilesChanged = 0;
                                        for(int i = 0; i < points.size(); i++) {
                                            int segSx = i == 0 ? world->player->hammerX : std::get<0>(points[i - 1]);
                                            int segSy = i == 0 ? world->player->hammerY : std::get<1>(points[i - 1]);
                                            int segEx = std::get<0>(points[i]);
                                            int segEy = std::get<1>(points[i]);

                                            bool hitSolidYet = false;
                                            bool broke = false;
                                            world->forLineCornered(segSx, segSy, segEx, segEy, [&](int index) {
                                                if(world->tiles[index].mat->physicsType != PhysicsType::SOLID) {
                                                    if(hitSolidYet && (abs((index % world->width) - segSx) + (abs((index / world->width) - segSy)) > 1)) {
                                                        broke = true;
                                                        return true;
                                                    }
                                                    return false;
                                                }
                                                hitSolidYet = true;
                                                world->tiles[index] = MaterialInstance(&Materials::GENERIC_SAND, Drawing::darkenColor(world->tiles[index].color, 0.5f));
                                                world->dirty[index] = true;
                                                endInd = index;
                                                nTilesChanged++;
                                                return false;
                                            });

                                            //world->setTile(segSx, segSy, MaterialInstance(&Materials::GENERIC_SOLID, 0x00ff00ff));
                                            if(broke) break;
                                        }

                                        //world->setTile(ex, ey, MaterialInstance(&Materials::GENERIC_SOLID, 0xff0000ff));

                                        int hx = (world->player->hammerX + (endInd % world->width)) / 2;
                                        int hy = (world->player->hammerY + (endInd / world->width)) / 2;

                                        if(world->getTile((int)(hx + udy * 2), (int)(hy - udx * 2)).mat->physicsType == PhysicsType::SOLID) {
                                            world->physicsCheck((int)(hx + udy * 2), (int)(hy - udx * 2));
                                        }

                                        if(world->getTile((int)(hx - udy * 2), (int)(hy + udx * 2)).mat->physicsType == PhysicsType::SOLID) {
                                            world->physicsCheck((int)(hx - udy * 2), (int)(hy + udx * 2));
                                        }

                                        if(nTilesChanged > 0) {
                                            audioEngine.PlayEvent("event:/Impact");
                                        }

                                        //world->setTile((int)(hx), (int)(hy), MaterialInstance(&Materials::GENERIC_SOLID, 0xffffffff));
                                        //world->setTile((int)(hx + udy * 6), (int)(hy - udx * 6), MaterialInstance(&Materials::GENERIC_SOLID, 0xffff00ff));
                                        //world->setTile((int)(hx - udy * 6), (int)(hy + udx * 6), MaterialInstance(&Materials::GENERIC_SOLID, 0x00ffffff));

                                    }
                                    world->player->holdHammer = false;
                                }
                            }
                        }
                    } else if(windowEvent.button.button == SDL_BUTTON_RIGHT) {
                        Controls::rmouse = false;
                        // pick up / throw item
                        #pragma region
                        int x = (int)((mx - ofsX - camX) / scale);
                        int y = (int)((my - ofsY - camY) / scale);

                        bool swapped = false;
                        vector<RigidBody*> rbs = world->rigidBodies; // copy;
                        for(size_t i = 0; i < rbs.size(); i++) {
                            RigidBody* cur = rbs[i];

                            bool connect = false;
                            if(cur->body->IsEnabled()) {
                                float s = sin(-cur->body->GetAngle());
                                float c = cos(-cur->body->GetAngle());
                                bool upd = false;
                                for(float xx = -3; xx <= 3; xx += 0.5) {
                                    for(float yy = -3; yy <= 3; yy += 0.5) {
                                        if(abs(xx) + abs(yy) == 6) continue;
                                        // rotate point

                                        float tx = x + xx - cur->body->GetPosition().x;
                                        float ty = y + yy - cur->body->GetPosition().y;

                                        int ntx = (int)(tx * c - ty * s);
                                        int nty = (int)(tx * s + ty * c);

                                        if(ntx >= 0 && nty >= 0 && ntx < cur->surface->w && nty < cur->surface->h) {
                                            if(((PIXEL(cur->surface, ntx, nty) >> 24) & 0xff) != 0x00) {
                                                connect = true;
                                            }
                                        }
                                    }
                                }
                            }

                            if(connect) {
                                if(world->player) {
                                    world->player->setItemInHand(Item::makeItem(ItemFlags::RIGIDBODY, cur), world);

                                    world->b2world->DestroyBody(cur->body);
                                    world->rigidBodies.erase(std::remove(world->rigidBodies.begin(), world->rigidBodies.end(), cur), world->rigidBodies.end());

                                    swapped = true;
                                }
                                break;
                            }

                        }

                        if(!swapped) {
                            if(world->player) world->player->setItemInHand(NULL, world);
                        }
                        #pragma endregion
                    } else if(windowEvent.button.button == SDL_BUTTON_MIDDLE) {
                        Controls::mmouse = false;
                    }
                }

                if(windowEvent.type == SDL_MOUSEMOTION) {
                    mx = windowEvent.motion.x;
                    my = windowEvent.motion.y;
                }

            }
            EASY_END_BLOCK;
            #pragma endregion
        }

        EASY_BLOCK("tick");

        if(networkMode == NetworkMode::SERVER) {
            server->tick();
        } else if(networkMode == NetworkMode::CLIENT) {
            client->tick();
        }

        if(networkMode != NetworkMode::SERVER) {
            //if(Settings::tick_world)
            updateFrameEarly();
        }

        while(now - lastTick > mspt) {
            if(Settings::tick_world && networkMode != NetworkMode::CLIENT)
                tick();
            target = realTarget;
            lastTick = now;
            tickTime++;
        }

        if(networkMode != NetworkMode::SERVER) {
            if(Settings::tick_world)
                updateFrameLate();
        }
        EASY_END_BLOCK;

        if(networkMode != NetworkMode::SERVER) {
            // render
            #pragma region
            EASY_BLOCK("render");
            target = realTarget;
            EASY_BLOCK("GPU_Clear", GPU_PROFILER_COLOR);
            GPU_Clear(target);
            EASY_END_BLOCK;

            renderEarly();
            target = realTarget;

            renderLate();
            target = realTarget;

            EASY_BLOCK("fades");
            if(fadeInWaitFrames > 0) {
                fadeInWaitFrames--;
                fadeInStart = now;
                GPU_RectangleFilled(target, 0, 0, WIDTH, HEIGHT, {0, 0, 0, 255});
            } else if(fadeInStart > 0 && fadeInLength > 0) {
                EASY_BLOCK("fadeIn");
                float thru = 1 - (float)(now - fadeInStart) / fadeInLength;

                if(thru >= 0 && thru <= 1) {
                    GPU_RectangleFilled(target, 0, 0, WIDTH, HEIGHT, {0, 0, 0, (uint8)(thru * 255)});
                } else {
                    fadeInStart = 0;
                    fadeInLength = 0;
                }
                EASY_END_BLOCK;
            }


            if(fadeOutWaitFrames > 0) {
                fadeOutWaitFrames--;
                fadeOutStart = now;
            } else if(fadeOutStart > 0 && fadeOutLength > 0) {
                EASY_BLOCK("fadeIn");
                float thru = (float)(now - fadeOutStart) / fadeOutLength;

                if(thru >= 0 && thru <= 1) {
                    GPU_RectangleFilled(target, 0, 0, WIDTH, HEIGHT, {0, 0, 0, (uint8)(thru * 255)});
                } else {
                    GPU_RectangleFilled(target, 0, 0, WIDTH, HEIGHT, {0, 0, 0, 255});
                    fadeOutStart = 0;
                    fadeOutLength = 0;
                    fadeOutCallback();
                }
                EASY_END_BLOCK;
            }
            EASY_END_BLOCK;

            EASY_BLOCK("GPU_Flip", GPU_PROFILER_COLOR);
            GPU_Flip(target);
            EASY_END_BLOCK;
        }

        if(Time::millis() - now < 2) {
            Sleep(2 - (Time::millis() - now));
        }

        EASY_BLOCK("frameCounter");
        frames++;
        if(now - lastFPS >= 1000) {
            lastFPS = now;
            logInfo("{0:d} FPS", frames);
            if(networkMode == NetworkMode::SERVER) {
                logDebug("{0:d} peers connected.", server->server->connectedPeers);
            }
            fps = frames;
            dt_fps.w = -1;
            frames = 0;
        }

        for(int i = 1; i < frameTimeNum; i++) {
            frameTime[i - 1] = frameTime[i];
        }
        frameTime[frameTimeNum - 1] = (uint16_t)(Time::millis() - now);
        EASY_END_BLOCK; // frameCounter

        lastTime = now;

        EASY_END_BLOCK; // render
        #pragma endregion
    }

exit:
    EASY_END_BLOCK; // frame??

    // release resources & shutdown
    #pragma region
    delete objectDelete;

    running = false;

    if(networkMode != NetworkMode::SERVER) {
        TTF_Quit();

        SDL_DestroyWindow(window);
        SDL_Quit();

        audioEngine.Shutdown();

        #if BUILD_WITH_STEAM
        SteamAPI_Shutdown();
        #endif
    }
    #pragma endregion

    if(profiler::isEnabled()) {
        // dump profiler data
        time_t rawtime;
        struct tm * timeinfo;
        char buf[80];

        time(&rawtime);
        timeinfo = localtime(&rawtime);

        strftime(buf, sizeof(buf), getFileInGameDir("profiler/%Y-%m-%d_%H-%M-%S.prof").c_str(), timeinfo);
        std::string str(buf);

        profiler::dumpBlocksToFile(str.c_str());
        struct stat buffer;
        if(stat(str.c_str(), &buffer) == 0) {

            std::ifstream src(str.c_str(), std::ios::binary);
            std::ofstream dst(getFileInGameDir("profiler/latest.prof").c_str(), std::ios::binary);

            dst << src.rdbuf();
        }
    }

    return EXIT_SUCCESS;
}

void Game::updateFrameEarly() {
    EASY_FUNCTION(GAME_PROFILER_COLOR);

    // handle controls
    #pragma region
    EASY_BLOCK("controls");
    if(Controls::DEBUG_UI->get()) {
        debugUI->visible = !debugUI->visible;
        debugDrawUI->visible = !debugDrawUI->visible;
    }

    if(Controls::DEBUG_REFRESH->get()) {
        for(int x = 0; x < world->width; x++) {
            for(int y = 0; y < world->height; y++) {
                world->dirty[x + y * world->width] = true;
                world->layer2Dirty[x + y * world->width] = true;
            }
        }
    }

    if(Controls::DEBUG_RIGID->get()) {
        for(auto& cur : world->rigidBodies) {
            if(cur->body->IsEnabled()) {
                float s = sin(cur->body->GetAngle());
                float c = cos(cur->body->GetAngle());
                bool upd = false;

                for(int xx = 0; xx < cur->matWidth; xx++) {
                    for(int yy = 0; yy < cur->matHeight; yy++) {
                        int tx = xx * c - yy * s + cur->body->GetPosition().x;
                        int ty = xx * s + yy * c + cur->body->GetPosition().y;

                        MaterialInstance tt = cur->tiles[xx + yy * cur->matWidth];
                        if(tt.mat->id != Materials::GENERIC_AIR.id) {
                            if(world->tiles[tx + ty * world->width].mat->id == Materials::GENERIC_AIR.id) {
                                world->tiles[tx + ty * world->width] = tt;
                                world->dirty[tx + ty * world->width] = true;
                            } else if(world->tiles[(tx + 1) + ty * world->width].mat->id == Materials::GENERIC_AIR.id) {
                                world->tiles[(tx + 1) + ty * world->width] = tt;
                                world->dirty[(tx + 1) + ty * world->width] = true;
                            } else if(world->tiles[(tx - 1) + ty * world->width].mat->id == Materials::GENERIC_AIR.id) {
                                world->tiles[(tx - 1) + ty * world->width] = tt;
                                world->dirty[(tx - 1) + ty * world->width] = true;
                            } else if(world->tiles[tx + (ty + 1) * world->width].mat->id == Materials::GENERIC_AIR.id) {
                                world->tiles[tx + (ty + 1) * world->width] = tt;
                                world->dirty[tx + (ty + 1) * world->width] = true;
                            } else if(world->tiles[tx + (ty - 1) * world->width].mat->id == Materials::GENERIC_AIR.id) {
                                world->tiles[tx + (ty - 1) * world->width] = tt;
                                world->dirty[tx + (ty - 1) * world->width] = true;
                            } else {
                                world->tiles[tx + ty * world->width] = Tiles::createObsidian(tx, ty);
                                world->dirty[tx + ty * world->width] = true;
                            }
                        }
                    }
                }

                if(upd) {
                    GPU_FreeImage(cur->texture);
                    cur->texture = GPU_CopyImageFromSurface(cur->surface);
                    GPU_SetImageFilter(cur->texture, GPU_FILTER_NEAREST);
                    //world->updateRigidBodyHitbox(cur);
                    cur->needsUpdate = true;
                }
            }

            world->b2world->DestroyBody(cur->body);
        }
        world->rigidBodies.clear();
    }

    if(Controls::DEBUG_UPDATE_WORLD_MESH->get()) {
        world->updateWorldMesh();
    }

    if(Controls::DEBUG_EXPLODE->get()) {
        int x = (int)((mx - ofsX - camX) / scale);
        int y = (int)((my - ofsY - camY) / scale);
        world->explosion(x, y, 30);
    }

    if(Controls::DEBUG_CARVE->get()) {
        // carve square
        #pragma region
        int x = (int)((mx - ofsX - camX) / scale - 16);
        int y = (int)((my - ofsY - camY) / scale - 16);

        SDL_Surface* tex = SDL_CreateRGBSurfaceWithFormat(0, 32, 32, 32, SDL_PIXELFORMAT_ARGB8888);

        int n = 0;
        for(int xx = 0; xx < 32; xx++) {
            for(int yy = 0; yy < 32; yy++) {

                if(world->tiles[(x + xx) + (y + yy) * world->width].mat->physicsType == PhysicsType::SOLID) {
                    PIXEL(tex, xx, yy) = world->tiles[(x + xx) + (y + yy) * world->width].color;
                    world->tiles[(x + xx) + (y + yy) * world->width] = Tiles::NOTHING;
                    world->dirty[(x + xx) + (y + yy) * world->width] = true;
                    n++;
                }
            }
        }
        if(n > 0) {
            b2PolygonShape s;
            s.SetAsBox(1, 1);
            RigidBody* rb = world->makeRigidBody(b2_dynamicBody, (float)x, (float)y, 0, s, 1, (float)0.3, tex);
            for(int x = 0; x < tex->w; x++) {
                b2Filter bf = {};
                bf.categoryBits = 0x0002;
                bf.maskBits = 0x0001;
                rb->body->GetFixtureList()[0].SetFilterData(bf);
            }
            world->rigidBodies.push_back(rb);
            world->updateRigidBodyHitbox(rb);

            world->updateWorldMesh();
        }
        #pragma endregion
    }

    if(Controls::DEBUG_BRUSHSIZE_INC->get()) {
        brushSize = brushSize < 50 ? brushSize + 1 : brushSize;
        brushSizeLabel->text = std::to_string(brushSize);
        brushSizeLabel->updateTexture();
    }

    if(Controls::DEBUG_BRUSHSIZE_DEC->get()) {
        brushSize = brushSize > 1 ? brushSize - 1 : brushSize;
        brushSizeLabel->text = std::to_string(brushSize);
        brushSizeLabel->updateTexture();
    }

    if(Controls::DEBUG_TOGGLE_PLAYER->get()) {
        if(world->player) {
            freeCamX = world->player->x + world->player->hw / 2;
            freeCamY = world->player->y - world->player->hh / 2;
            world->entities.erase(std::remove(world->entities.begin(), world->entities.end(), world->player), world->entities.end());
            world->b2world->DestroyBody(world->player->rb->body);
            delete world->player;
            world->player = nullptr;
        } else {
            Player* e = new Player();
            e->x = freeCamX;
            e->y = freeCamY - 4;
            e->vx = 0;
            e->vy = 0;
            e->hw = 10;
            e->hh = 20;
            b2PolygonShape sh;
            sh.SetAsBox(e->hw / 2 + 1, e->hh / 2);
            e->rb = world->makeRigidBody(b2BodyType::b2_kinematicBody, e->x + e->hw / 2 - 0.5, e->y + e->hh / 2 - 0.5, 0, sh, 1, 1, NULL);
            e->rb->body->SetGravityScale(0);
            e->rb->body->SetLinearDamping(0);
            e->rb->body->SetAngularDamping(0);

            Item* i3 = new Item();
            i3->setFlag(ItemFlags::VACUUM);
            i3->vacuumParticles = {};
            i3->surface = Textures::loadTexture("assets/objects/chisel.png");
            i3->texture = GPU_CopyImageFromSurface(i3->surface);
            GPU_SetImageFilter(i3->texture, GPU_FILTER_NEAREST);
            e->setItemInHand(i3, world);

            b2Filter bf = {};
            bf.categoryBits = 0x0001;
            //bf.maskBits = 0x0000;
            e->rb->body->GetFixtureList()[0].SetFilterData(bf);

            world->entities.push_back(e);
            world->player = e;
        }
    }
    EASY_END_BLOCK;
    #pragma endregion

    EASY_BLOCK("audioEngine.Update");
    audioEngine.Update();
    EASY_END_BLOCK;

    if(state == LOADING) {

    } else {
        audioEngine.SetEventParameter("event:/Sand", "Sand", 0);
        if(world->player && world->player->heldItem != NULL && world->player->heldItem->getFlag(ItemFlags::FLUID_CONTAINER)) {
            if(Controls::lmouse && world->player->heldItem->carry.size() > 0) {
                // shoot fluid from container
                #pragma region
                int x = (int)(world->player->x + world->player->hw / 2 + world->loadZone.x + 10 * (float)cos((world->player->holdAngle + 180) * 3.1415f / 180.0f));
                int y = (int)(world->player->y + world->player->hh / 2 + world->loadZone.y + 10 * (float)sin((world->player->holdAngle + 180) * 3.1415f / 180.0f));

                MaterialInstance mat = world->player->heldItem->carry[world->player->heldItem->carry.size() - 1];
                world->player->heldItem->carry.pop_back();
                world->addParticle(new Particle(mat, (float)x, (float)y, (float)(world->player->vx / 2 + (rand() % 10 - 5) / 10.0f + 1.5f * (float)cos((world->player->holdAngle + 180) * 3.1415f / 180.0f)), (float)(world->player->vy / 2 + -(rand() % 5 + 5) / 10.0f + 1.5f * (float)sin((world->player->holdAngle + 180) * 3.1415f / 180.0f)), 0, (float)0.1));

                int i = world->player->heldItem->carry.size();
                i = (int)((i / (float)world->player->heldItem->capacity) * world->player->heldItem->fill.size());
                UInt16Point pt = world->player->heldItem->fill[i];
                PIXEL(world->player->heldItem->surface, pt.x, pt.y) = 0x00;

                world->player->heldItem->texture = GPU_CopyImageFromSurface(world->player->heldItem->surface);
                GPU_SetImageFilter(world->player->heldItem->texture, GPU_FILTER_NEAREST);

                audioEngine.SetEventParameter("event:/Sand", "Sand", 1);
                #pragma endregion
            } else {
                // pick up fluid into container
                #pragma region
                float breakSize = world->player->heldItem->breakSize;

                int x = (int)(world->player->x + world->player->hw / 2 + world->loadZone.x + 10 * (float)cos((world->player->holdAngle + 180) * 3.1415f / 180.0f) - breakSize / 2);
                int y = (int)(world->player->y + world->player->hh / 2 + world->loadZone.y + 10 * (float)sin((world->player->holdAngle + 180) * 3.1415f / 180.0f) - breakSize / 2);

                int n = 0;
                for(int xx = 0; xx < breakSize; xx++) {
                    for(int yy = 0; yy < breakSize; yy++) {
                        if(world->player->heldItem->capacity == 0 || (world->player->heldItem->carry.size() < world->player->heldItem->capacity)) {
                            float cx = (float)((xx / breakSize) - 0.5);
                            float cy = (float)((yy / breakSize) - 0.5);

                            if(cx * cx + cy * cy > 0.25f) continue;

                            if(world->tiles[(x + xx) + (y + yy) * world->width].mat->physicsType == PhysicsType::SAND || world->tiles[(x + xx) + (y + yy) * world->width].mat->physicsType == PhysicsType::SOUP) {
                                world->player->heldItem->carry.push_back(world->tiles[(x + xx) + (y + yy) * world->width]);

                                int i = world->player->heldItem->carry.size() - 1;
                                i = (int)((i / (float)world->player->heldItem->capacity) * world->player->heldItem->fill.size());
                                UInt16Point pt = world->player->heldItem->fill[i];
                                Uint32 c = world->tiles[(x + xx) + (y + yy) * world->width].color;
                                PIXEL(world->player->heldItem->surface, pt.x, pt.y) = (world->tiles[(x + xx) + (y + yy) * world->width].mat->alpha << 24) + c;

                                world->player->heldItem->texture = GPU_CopyImageFromSurface(world->player->heldItem->surface);
                                GPU_SetImageFilter(world->player->heldItem->texture, GPU_FILTER_NEAREST);

                                world->tiles[(x + xx) + (y + yy) * world->width] = Tiles::NOTHING;
                                world->dirty[(x + xx) + (y + yy) * world->width] = true;
                                n++;
                            }
                        }
                    }
                }

                if(n > 0) {
                    audioEngine.PlayEvent("event:/Impact");
                }
                #pragma endregion
            }
        }

        // rigidbody hover
        #pragma region

        int x = (int)((mx - ofsX - camX) / scale);
        int y = (int)((my - ofsY - camY) / scale);

        bool swapped = false;

        // this copies the vector
        vector<RigidBody*> rbs = world->rigidBodies;
        for(size_t i = 0; i < rbs.size(); i++) {
            RigidBody* cur = rbs[i];

            if(swapped) {
                cur->hover = (float)std::fmax(0, cur->hover - 0.06);
                continue;
            }

            bool connect = false;
            if(cur->body->IsEnabled()) {
                float s = sin(-cur->body->GetAngle());
                float c = cos(-cur->body->GetAngle());
                bool upd = false;
                for(float xx = -3; xx <= 3; xx += 0.5) {
                    for(float yy = -3; yy <= 3; yy += 0.5) {
                        if(abs(xx) + abs(yy) == 6) continue;
                        // rotate point

                        float tx = x + xx - cur->body->GetPosition().x;
                        float ty = y + yy - cur->body->GetPosition().y;

                        int ntx = (int)(tx * c - ty * s);
                        int nty = (int)(tx * s + ty * c);

                        if(ntx >= 0 && nty >= 0 && ntx < cur->surface->w && nty < cur->surface->h) {
                            if(((PIXEL(cur->surface, ntx, nty) >> 24) & 0xff) != 0x00) {
                                connect = true;
                            }
                        }
                    }
                }
            }

            if(connect) {
                swapped = true;
                cur->hover = (float)std::fmin(1, cur->hover + 0.06);
            } else {
                cur->hover = (float)std::fmax(0, cur->hover - 0.06);
            }
        }
        #pragma endregion

        // update world->tickZone
        #pragma region
        world->tickZone = {CHUNK_W, CHUNK_H, world->width - CHUNK_W * 2, world->height - CHUNK_H * 2};
        if(world->tickZone.x + world->tickZone.w > world->width) {
            world->tickZone.x = world->width - world->tickZone.w;
        }

        if(world->tickZone.y + world->tickZone.h > world->height) {
            world->tickZone.y = world->height - world->tickZone.h;
        }
        #pragma endregion

    }
}

void Game::tick() {
    EASY_FUNCTION(GAME_PROFILER_COLOR);

    //logDebug("{0:d} {0:d}", accLoadX, accLoadY);
    if(state == LOADING) {
        if(world) {
            // tick chunkloading
            world->frame();
            if(world->readyToMerge.size() == 0 && fadeOutStart == 0) {
                fadeOutStart = now;
                fadeOutLength = 250;
                fadeOutCallback = [&]() {
                    fadeInStart = now;
                    fadeInLength = 500;
                    fadeInWaitFrames = 4;
                    state = stateAfterLoad;
                };

                EASY_EVENT("world done loading", profiler::colors::Magenta);
            }
        }
    } else {
        // check chunk loading
        #pragma region
        EASY_BLOCK("chunk loading");
        int lastReadyToMergeSize = world->readyToMerge.size();

        // if need to load chunks
        if((abs(accLoadX) > CHUNK_W / 2 || abs(accLoadY) > CHUNK_H / 2)) {
            while(world->toLoad.size() > 0) {
                // tick chunkloading
                world->frame();
            }

            //iterate
            #pragma region
            EASY_BLOCK("iterate");
            for(int i = 0; i < world->width * world->height; i++) {
                const unsigned int offset = i * 4;

                #define UCH_SET_PIXEL(pix_ar, ofs, c_r, c_g, c_b, c_a) \
				pix_ar[ofs + 0] = c_b;\
				pix_ar[ofs + 1] = c_g;\
				pix_ar[ofs + 2] = c_r;\
				pix_ar[ofs + 3] = c_a;

                if(world->dirty[i]) {
                    if(world->tiles[i].mat->physicsType == PhysicsType::AIR) {
                        UCH_SET_PIXEL(pixels_ar, offset, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
                    } else {
                        Uint32 color = world->tiles[i].color;
                        UCH_SET_PIXEL(pixels_ar, offset, (color >> 0) & 0xff, (color >> 8) & 0xff, (color >> 16) & 0xff, world->tiles[i].mat->alpha);
                    }
                }

                if(world->layer2Dirty[i]) {
                    if(world->layer2[i].mat->physicsType == PhysicsType::AIR) {
                        if(Settings::draw_background_grid) {
                            Uint32 color = ((i) % 2) == 0 ? 0x888888 : 0x444444;
                            UCH_SET_PIXEL(pixelsLayer2_ar, offset, (color >> 0) & 0xff, (color >> 8) & 0xff, (color >> 16) & 0xff, SDL_ALPHA_OPAQUE);
                        } else {
                            UCH_SET_PIXEL(pixelsLayer2_ar, offset, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
                        }
                        continue;
                    }
                    Uint32 color = world->layer2[i].color;
                    UCH_SET_PIXEL(pixelsLayer2_ar, offset, (color >> 0) & 0xff, (color >> 8) & 0xff, (color >> 16) & 0xff, world->layer2[i].mat->alpha);
                }

                if(world->backgroundDirty[i]) {
                    Uint32 color = world->background[i];
                    UCH_SET_PIXEL(pixelsBackground_ar, offset, (color >> 0) & 0xff, (color >> 8) & 0xff, (color >> 16) & 0xff, (color >> 24) & 0xff);
                }
                #undef UCH_SET_PIXEL
            }
            EASY_END_BLOCK;
            #pragma endregion

            EASY_BLOCK("memset");
            memset(world->dirty, false, world->width * world->height);
            memset(world->layer2Dirty, false, world->width * world->height);
            memset(world->backgroundDirty, false, world->width * world->height);
            EASY_END_BLOCK;

            EASY_BLOCK("loop");
            while((abs(accLoadX) > CHUNK_W / 2 || abs(accLoadY) > CHUNK_H / 2)) {
                int subX = std::fmax(std::fmin(accLoadX, CHUNK_W / 2), -CHUNK_W / 2);
                if(abs(subX) < CHUNK_W / 2) subX = 0;
                int subY = std::fmax(std::fmin(accLoadY, CHUNK_H / 2), -CHUNK_H / 2);
                if(abs(subY) < CHUNK_H / 2) subY = 0;

                world->loadZone.x += subX;
                world->loadZone.y += subY;

                int delta = 4 * (subX + subY * world->width);
                EASY_BLOCK("rotate");
                std::vector<std::future<void>> results = {};
                if(delta > 0) {
                    results.push_back(updateDirtyPool->push([&](int id) {
                        rotate(&(pixels_ar[0]), &(pixels_ar[world->width * world->height * 4]) - delta, &(pixels_ar[world->width * world->height * 4]));
                        //rotate(pixels.begin(), pixels.end() - delta, pixels.end());
                    }));
                    results.push_back(updateDirtyPool->push([&](int id) {
                        rotate(&(pixelsLayer2_ar[0]), &(pixelsLayer2_ar[world->width * world->height * 4]) - delta, &(pixelsLayer2_ar[world->width * world->height * 4]));
                        //rotate(pixelsLayer2.begin(), pixelsLayer2.end() - delta, pixelsLayer2.end());
                    }));
                    results.push_back(updateDirtyPool->push([&](int id) {
                        rotate(&(pixelsBackground_ar[0]), &(pixelsBackground_ar[world->width * world->height * 4]) - delta, &(pixelsBackground_ar[world->width * world->height * 4]));
                        //rotate(pixelsBackground.begin(), pixelsBackground.end() - delta, pixelsBackground.end());
                    }));
                    results.push_back(updateDirtyPool->push([&](int id) {
                        rotate(&(pixelsFire_ar[0]), &(pixelsFire_ar[world->width * world->height * 4]) - delta, &(pixelsFire_ar[world->width * world->height * 4]));
                        //rotate(pixelsFire_ar.begin(), pixelsFire_ar.end() - delta, pixelsFire_ar.end());
                    }));
                } else if(delta < 0) {
                    results.push_back(updateDirtyPool->push([&](int id) {
                        rotate(&(pixels_ar[0]), &(pixels_ar[0]) - delta, &(pixels_ar[world->width * world->height * 4]));
                        //rotate(pixels.begin(), pixels.begin() - delta, pixels.end());
                    }));
                    results.push_back(updateDirtyPool->push([&](int id) {
                        rotate(&(pixelsLayer2_ar[0]), &(pixelsLayer2_ar[0]) - delta, &(pixelsLayer2_ar[world->width * world->height * 4]));
                        //rotate(pixelsLayer2.begin(), pixelsLayer2.begin() - delta, pixelsLayer2.end());
                    }));
                    results.push_back(updateDirtyPool->push([&](int id) {
                        rotate(&(pixelsBackground_ar[0]), &(pixelsBackground_ar[0]) - delta, &(pixelsBackground_ar[world->width * world->height * 4]));
                        //rotate(pixelsBackground.begin(), pixelsBackground.begin() - delta, pixelsBackground.end());
                    }));
                    results.push_back(updateDirtyPool->push([&](int id) {
                        rotate(&(pixelsFire_ar[0]), &(pixelsFire_ar[0]) - delta, &(pixelsFire_ar[world->width * world->height * 4]));
                        //rotate(pixelsFire_ar.begin(), pixelsFire_ar.begin() - delta, pixelsFire_ar.end());
                    }));
                }
                EASY_BLOCK("wait for threads", THREAD_WAIT_PROFILER_COLOR);
                for(auto& v : results) {
                    v.get();
                }
                EASY_END_BLOCK; // wait for threads

                EASY_END_BLOCK; // rotate

                #define CLEARPIXEL(pixels, ofs) \
pixels[ofs + 0] = pixels[ofs + 1] = pixels[ofs + 2] = 0xff; \
pixels[ofs + 3] = SDL_ALPHA_TRANSPARENT;

                EASY_BLOCK("CLEARPIXEL X");
                for(int x = 0; x < abs(subX); x++) {
                    for(int y = 0; y < world->height; y++) {
                        const unsigned int offset = (world->width * 4 * y) + x * 4;
                        if(offset < world->width * world->height * 4) {
                            CLEARPIXEL(pixels_ar, offset);
                            CLEARPIXEL(pixelsLayer2_ar, offset);
                            CLEARPIXEL(pixelsObjects_ar, offset);
                            CLEARPIXEL(pixelsBackground_ar, offset);
                            CLEARPIXEL(pixelsFire_ar, offset);
                        }
                    }
                }
                EASY_END_BLOCK;

                EASY_BLOCK("CLEARPIXEL Y");
                for(int y = 0; y < abs(subY); y++) {
                    for(int x = 0; x < world->width; x++) {
                        const unsigned int offset = (world->width * 4 * y) + x * 4;
                        if(offset < world->width * world->height * 4) {
                            CLEARPIXEL(pixels_ar, offset);
                            CLEARPIXEL(pixelsLayer2_ar, offset);
                            CLEARPIXEL(pixelsObjects_ar, offset);
                            CLEARPIXEL(pixelsBackground_ar, offset);
                            CLEARPIXEL(pixelsFire_ar, offset);
                        }
                    }
                }
                EASY_END_BLOCK;

                #undef CLEARPIXEL

                accLoadX -= subX;
                accLoadY -= subY;

                ofsX -= subX * scale;
                ofsY -= subY * scale;
            }

            EASY_END_BLOCK;

            if(Settings::draw_light_map) renderLightmap(world);
            world->tickChunks();
            world->updateWorldMesh();
            world->dirty[0] = true;
            world->layer2Dirty[0] = true;
            world->backgroundDirty[0] = true;

        } else {
            world->frame();
        }
        EASY_END_BLOCK;
        #pragma endregion

        if(world->needToTickGeneration) world->tickChunkGeneration();

        // clear objects
        GPU_Rect r = {0, 0, (float)(world->width), (float)(world->height)};
        GPU_SetShapeBlendMode(GPU_BLEND_NORMAL);
        GPU_Clear(textureObjects->target);

        GPU_SetShapeBlendMode(GPU_BLEND_NORMAL);
        GPU_Clear(textureObjectsBack->target);

        // render objects
        #pragma region
        EASY_BLOCK("memset objectDelete");
        memset(objectDelete, false, world->width * world->height * sizeof(bool));
        EASY_END_BLOCK;

        EASY_BLOCK("render rigidbodies");
        GPU_SetBlendMode(textureObjects, GPU_BLEND_NORMAL);
        GPU_SetBlendMode(textureObjectsBack, GPU_BLEND_NORMAL);

        for(size_t i = 0; i < world->rigidBodies.size(); i++) {
            RigidBody* cur = world->rigidBodies[i];
            if(cur == nullptr) continue;
            if(cur->surface == nullptr) continue;
            if(!cur->body->IsEnabled()) continue;

            float x = cur->body->GetPosition().x;
            float y = cur->body->GetPosition().y;

            // draw
            #pragma region
            GPU_Target* tgt = cur->back ? textureObjectsBack->target : textureObjects->target;

            GPU_Rect r = {x, y, (float)cur->surface->w, (float)cur->surface->h};

            if(cur->texNeedsUpdate) {
                cur->texture = GPU_CopyImageFromSurface(cur->surface);
                GPU_SetImageFilter(cur->texture, GPU_FILTER_NEAREST);
                cur->texNeedsUpdate = false;
            }

            GPU_BlitRectX(cur->texture, NULL, tgt, &r, cur->body->GetAngle() * 180 / (float)W_PI, 0, 0, GPU_FLIP_NONE);
            #pragma endregion

            // draw outline
            #pragma region
            Uint8 outlineAlpha = (Uint8)(cur->hover * 255);
            if(outlineAlpha > 0) {
                SDL_Color col = {0xff, 0xff, 0x80, outlineAlpha};
                GPU_SetShapeBlendMode(GPU_BLEND_NORMAL); // SDL_BLENDMODE_BLEND
                for(auto& l : cur->outline) {
                    b2Vec2* vec = new b2Vec2[l.GetNumPoints()];
                    for(int j = 0; j < l.GetNumPoints(); j++) {
                        vec[j] = {(float)l.GetPoint(j).x / scale, (float)l.GetPoint(j).y / scale};
                    }
                    Drawing::drawPolygon(tgt, col, vec, (int)x, (int)y, scale, l.GetNumPoints(), cur->body->GetAngle(), 0, 0);
                    delete[] vec;
                }
                GPU_SetShapeBlendMode(GPU_BLEND_NORMAL); // SDL_BLENDMODE_NONE
            }
            #pragma endregion

            // displace fluids
            #pragma region
            float s = sin(cur->body->GetAngle());
            float c = cos(cur->body->GetAngle());

            std::vector<std::pair<int, int>> checkDirs = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};

            for(int tx = 0; tx < cur->matWidth; tx++) {
                for(int ty = 0; ty < cur->matHeight; ty++) {
                    MaterialInstance rmat = cur->tiles[tx + ty * cur->matWidth];
                    if(rmat.mat->id == Materials::GENERIC_AIR.id) continue;

                    // rotate point
                    int wx = (int)(tx * c - ty * s + x);
                    int wy = (int)(tx * s + ty * c + y);

                    for(auto& dir : checkDirs) {
                        int wxd = wx + dir.first;
                        int wyd = wy + dir.second;

                        if(wxd < 0 || wyd < 0 || wxd >= world->width || wyd >= world->height) continue;
                        if(world->tiles[wxd + wyd * world->width].mat->physicsType == PhysicsType::AIR) {
                            world->tiles[wxd + wyd * world->width] = rmat;
                            world->dirty[wxd + wyd * world->width] = true;
                            //objectDelete[wxd + wyd * world->width] = true;
                            break;
                        } else if(world->tiles[wxd + wyd * world->width].mat->physicsType == PhysicsType::SAND) {
                            world->addParticle(new Particle(world->tiles[wxd + wyd * world->width], (float)wxd, (float)(wyd - 3), (float)((rand() % 10 - 5) / 10.0f), (float)(-(rand() % 5 + 5) / 10.0f), 0, (float)0.1));
                            world->tiles[wxd + wyd * world->width] = rmat;
                            //objectDelete[wxd + wyd * world->width] = true;
                            world->dirty[wxd + wyd * world->width] = true;
                            cur->body->SetLinearVelocity({cur->body->GetLinearVelocity().x * (float)0.99, cur->body->GetLinearVelocity().y * (float)0.99});
                            cur->body->SetAngularVelocity(cur->body->GetAngularVelocity() * (float)0.98);
                            break;
                        } else if(world->tiles[wxd + wyd * world->width].mat->physicsType == PhysicsType::SOUP) {
                            world->addParticle(new Particle(world->tiles[wxd + wyd * world->width], (float)wxd, (float)(wyd - 3), (float)((rand() % 10 - 5) / 10.0f), (float)(-(rand() % 5 + 5) / 10.0f), 0, (float)0.1));
                            world->tiles[wxd + wyd * world->width] = rmat;
                            //objectDelete[wxd + wyd * world->width] = true;
                            world->dirty[wxd + wyd * world->width] = true;
                            cur->body->SetLinearVelocity({cur->body->GetLinearVelocity().x * (float)0.998, cur->body->GetLinearVelocity().y * (float)0.998});
                            cur->body->SetAngularVelocity(cur->body->GetAngularVelocity() * (float)0.99);
                            break;
                        }
                    }
                }
            }
            #pragma endregion
        }
        EASY_END_BLOCK;
        #pragma endregion

        // render entities
        #pragma region
        EASY_BLOCK("render entities");
        if(lastReadyToMergeSize == 0) {
            world->tickEntities(textureEntities->target);

            if(world->player) {
                if(world->player->holdHammer) {
                    int x = (int)((mx - ofsX - camX) / scale);
                    int y = (int)((my - ofsY - camY) / scale);
                    GPU_Line(textureEntities->target, x, y, world->player->hammerX, world->player->hammerY, {0xff, 0xff, 0x00, 0xff});
                }
            }
        }
        GPU_SetShapeBlendMode(GPU_BLEND_NORMAL); // SDL_BLENDMODE_NONE
        EASY_END_BLOCK;
        #pragma endregion

        // entity fluid displacement & make solid
        #pragma region
        for(size_t i = 0; i < world->entities.size(); i++) {
            Entity cur = *world->entities[i];

            for(int tx = 0; tx < cur.hw; tx++) {
                for(int ty = 0; ty < cur.hh; ty++) {

                    int wx = (int)(tx + cur.x + world->loadZone.x);
                    int wy = (int)(ty + cur.y + world->loadZone.y);
                    if(wx < 0 || wy < 0 || wx >= world->width || wy >= world->height) continue;
                    if(world->tiles[wx + wy * world->width].mat->physicsType == PhysicsType::AIR) {
                        world->tiles[wx + wy * world->width] = Tiles::OBJECT;
                        objectDelete[wx + wy * world->width] = true;
                    } else if(world->tiles[wx + wy * world->width].mat->physicsType == PhysicsType::SAND || world->tiles[wx + wy * world->width].mat->physicsType == PhysicsType::SOUP) {
                        world->addParticle(new Particle(world->tiles[wx + wy * world->width], (float)(wx + rand() % 3 - 1 - cur.vx), (float)(wy - abs(cur.vy)), (float)(-cur.vx / 4 + (rand() % 10 - 5) / 5.0f), (float)(-cur.vy / 4 + -(rand() % 5 + 5) / 5.0f), 0, (float)0.1));
                        world->tiles[wx + wy * world->width] = Tiles::OBJECT;
                        objectDelete[wx + wy * world->width] = true;
                        world->dirty[wx + wy * world->width] = true;
                    }
                }
            }
        }
        #pragma endregion

        if(Settings::tick_world && world->readyToMerge.size() == 0) {
            world->tick();
        }

        if(Controls::DEBUG_TICK->get()) {
            world->tick();
        }

        // player movement
        #pragma region
        if(world->player) {
            if(Controls::PLAYER_UP->get() && !Controls::DEBUG_DRAW->get()) {
                if(world->player->ground) {
                    world->player->vy = -4;
                    audioEngine.PlayEvent("event:/Jump");
                }
            }

            world->player->vy += (float)(((Controls::PLAYER_UP->get() && !Controls::DEBUG_DRAW->get()) ? (world->player->vy > -1 ? -0.8 : -0.35) : 0) + (Controls::PLAYER_DOWN->get() ? 0.1 : 0));
            if(Controls::PLAYER_UP->get() && !Controls::DEBUG_DRAW->get()) {
                audioEngine.SetEventParameter("event:/Fly", "Intensity", 1);
                for(int i = 0; i < 4; i++) {
                    Particle* p = new Particle(Tiles::createLava(), (float)(world->player->x + world->loadZone.x + world->player->hw / 2 + rand() % 5 - 2 + world->player->vx), (float)(world->player->y + world->loadZone.y + world->player->hh + world->player->vy), (float)((rand() % 10 - 5) / 10.0f + world->player->vx / 2.0f), (float)((rand() % 10) / 10.0f + 1 + world->player->vy / 2.0f), 0, (float)0.025);
                    p->temporary = true;
                    p->lifetime = 120;
                    world->addParticle(p);
                }
            } else {
                audioEngine.SetEventParameter("event:/Fly", "Intensity", 0);
            }

            if(world->player->vy > 0) {
                audioEngine.SetEventParameter("event:/Wind", "Wind", (float)(world->player->vy / 12.0));
            } else {
                audioEngine.SetEventParameter("event:/Wind", "Wind", 0);
            }

            world->player->vx += (float)((Controls::PLAYER_LEFT->get() ? (world->player->vx > 0 ? -0.4 : -0.2) : 0) + (Controls::PLAYER_RIGHT->get() ? (world->player->vx < 0 ? 0.4 : 0.2) : 0));
            if(!Controls::PLAYER_LEFT->get() && !Controls::PLAYER_RIGHT->get()) world->player->vx *= (float)(world->player->ground ? 0.85 : 0.96);
            if(world->player->vx > 4.5) world->player->vx = 4.5;
            if(world->player->vx < -4.5) world->player->vx = -4.5;
        } else {
            if(state == INGAME) {
                freeCamX += (float)((Controls::PLAYER_LEFT->get() ? -5 : 0) + (Controls::PLAYER_RIGHT->get() ? 5 : 0));
                freeCamY += (float)((Controls::PLAYER_UP->get() ? -5 : 0) + (Controls::PLAYER_DOWN->get() ? 5 : 0));
            } else {

            }
        }
        #pragma endregion

        #pragma region
        if(world->player) {
            desCamX = (float)(-(mx - (WIDTH / 2)) / 4);
            desCamY = (float)(-(my - (HEIGHT / 2)) / 4);

            world->player->holdAngle = (float)(atan2(desCamY, desCamX) * 180 / (float)W_PI);

            desCamX = 0;
            desCamY = 0;
        } else {
            desCamX = 0;
            desCamY = 0;
        }
        #pragma endregion

        #pragma region
        if(world->player) {
            if(world->player->heldItem) {
                if(world->player->heldItem->getFlag(ItemFlags::VACUUM)) {
                    if(world->player->holdVacuum) {

                        int wcx = (int)((WIDTH / 2 - ofsX - camX) / scale);
                        int wcy = (int)((HEIGHT / 2 - ofsY - camY) / scale);

                        int wmx = (int)((mx - ofsX - camX) / scale);
                        int wmy = (int)((my - ofsY - camY) / scale);

                        int mdx = wmx - wcx;
                        int mdy = wmy - wcy;

                        int distSq = mdx * mdx + mdy * mdy;
                        if(distSq <= 256 * 256) {

                            int sind = -1;
                            bool inObject = true;
                            world->forLine(wcx, wcy, wmx, wmy, [&](int ind) {
                                if(world->tiles[ind].mat->physicsType == PhysicsType::OBJECT) {
                                    if(!inObject) {
                                        sind = ind;
                                        return true;
                                    }
                                } else {
                                    inObject = false;
                                }

                                if(world->tiles[ind].mat->physicsType == PhysicsType::SOLID || world->tiles[ind].mat->physicsType == PhysicsType::SAND || world->tiles[ind].mat->physicsType == PhysicsType::SOUP) {
                                    sind = ind;
                                    return true;
                                }
                                return false;
                            });

                            int x = sind == -1 ? wmx : sind % world->width;
                            int y = sind == -1 ? wmy : sind / world->width;

                            std::function<void(MaterialInstance, int, int)> makeParticle = [&](MaterialInstance tile, int xPos, int yPos) {
                                Particle* par = new Particle(tile, xPos, yPos, 0, 0, 0, (float)0.01f);
                                par->vx = (rand() % 10 - 5) / 5.0f * 1.0f;
                                par->vy = (rand() % 10 - 5) / 5.0f * 1.0f;
                                par->ax = -par->vx / 10.0f;
                                par->ay = -par->vy / 10.0f;
                                if(par->ay == 0 && par->ax == 0) par->ay = 0.01f;

                                //par->targetX = world->player->x + world->player->hw / 2 + world->loadZone.x;
                                //par->targetY = world->player->y + world->player->hh / 2 + world->loadZone.y;
                                //par->targetForce = 0.35f;

                                par->lifetime = 6;

                                par->phase = true;

                                world->player->heldItem->vacuumParticles.push_back(par);

                                par->killCallback = [&]() {
                                    auto v = world->player->heldItem->vacuumParticles;
                                    v.erase(std::remove(v.begin(), v.end(), par), v.end());
                                };

                                world->addParticle(par);
                            };

                            int rad = 5;
                            for(int xx = -rad; xx <= rad; xx++) {
                                for(int yy = -rad; yy <= rad; yy++) {
                                    if((yy == -rad || yy == rad) && (xx == -rad || x == rad)) continue;

                                    MaterialInstance tile = world->tiles[(x + xx) + (y + yy) * world->width];
                                    if(tile.mat->physicsType == PhysicsType::SOLID || tile.mat->physicsType == PhysicsType::SAND || tile.mat->physicsType == PhysicsType::SOUP) {
                                        makeParticle(tile, x + xx, y + yy);
                                        world->tiles[(x + xx) + (y + yy) * world->width] = Tiles::NOTHING;
                                        //world->tiles[(x + xx) + (y + yy) * world->width] = Tiles::createFire();
                                        world->dirty[(x + xx) + (y + yy) * world->width] = true;
                                    }


                                }
                            }

                            world->particles.erase(std::remove_if(world->particles.begin(), world->particles.end(), [&](Particle* cur) {
                                if(cur->targetForce == 0 && !cur->phase) {
                                    int rad = 5;
                                    for(int xx = -rad; xx <= rad; xx++) {
                                        for(int yy = -rad; yy <= rad; yy++) {
                                            if((yy == -rad || yy == rad) && (xx == -rad || x == rad)) continue;

                                            if(((int)(cur->x) == (x + xx)) && ((int)(cur->y) == (y + yy))) {

                                                cur->vx = (rand() % 10 - 5) / 5.0f * 1.0f;
                                                cur->vy = (rand() % 10 - 5) / 5.0f * 1.0f;
                                                cur->ax = -cur->vx / 10.0f;
                                                cur->ay = -cur->vy / 10.0f;
                                                if(cur->ay == 0 && cur->ax == 0) cur->ay = 0.01f;

                                                //par->targetX = world->player->x + world->player->hw / 2 + world->loadZone.x;
                                                //par->targetY = world->player->y + world->player->hh / 2 + world->loadZone.y;
                                                //par->targetForce = 0.35f;

                                                cur->lifetime = 6;

                                                cur->phase = true;

                                                world->player->heldItem->vacuumParticles.push_back(cur);

                                                cur->killCallback = [&]() {
                                                    auto v = world->player->heldItem->vacuumParticles;
                                                    v.erase(std::remove(v.begin(), v.end(), cur), v.end());
                                                };

                                                return false;
                                            }
                                        }
                                    }
                                }

                                return false;
                            }), world->particles.end());

                            vector<RigidBody*> rbs = world->rigidBodies;

                            for(size_t i = 0; i < rbs.size(); i++) {
                                RigidBody* cur = rbs[i];
                                if(cur->body->IsEnabled()) {
                                    float s = sin(-cur->body->GetAngle());
                                    float c = cos(-cur->body->GetAngle());
                                    bool upd = false;
                                    for(int xx = -rad; xx <= rad; xx++) {
                                        for(int yy = -rad; yy <= rad; yy++) {
                                            if((yy == -rad || yy == rad) && (xx == -rad || x == rad)) continue;
                                            // rotate point

                                            float tx = x + xx - cur->body->GetPosition().x;
                                            float ty = y + yy - cur->body->GetPosition().y;

                                            int ntx = (int)(tx * c - ty * s);
                                            int nty = (int)(tx * s + ty * c);

                                            if(ntx >= 0 && nty >= 0 && ntx < cur->surface->w && nty < cur->surface->h) {
                                                Uint32 pixel = PIXEL(cur->surface, ntx, nty);
                                                if(((pixel >> 24) & 0xff) != 0x00) {
                                                    PIXEL(cur->surface, ntx, nty) = 0x00000000;
                                                    upd = true;

                                                    makeParticle(MaterialInstance(&Materials::GENERIC_SOLID, pixel), (x + xx), (y + yy));
                                                }

                                            }
                                        }
                                    }

                                    if(upd) {
                                        GPU_FreeImage(cur->texture);
                                        cur->texture = GPU_CopyImageFromSurface(cur->surface);
                                        GPU_SetImageFilter(cur->texture, GPU_FILTER_NEAREST);
                                        //world->updateRigidBodyHitbox(cur);
                                        cur->needsUpdate = true;
                                    }
                                }
                            }

                        }
                    }

                    if(world->player->heldItem->vacuumParticles.size() > 0) {
                        world->player->heldItem->vacuumParticles.erase(std::remove_if(world->player->heldItem->vacuumParticles.begin(), world->player->heldItem->vacuumParticles.end(), [&](Particle* cur) {

                            if(cur->lifetime <= 0) {
                                cur->targetForce = 0.45f;
                                cur->targetX = world->player->x + world->player->hw / 2 + world->loadZone.x;
                                cur->targetY = world->player->y + world->player->hh / 2 + world->loadZone.y;
                                cur->ax = 0;
                                cur->ay = 0.01f;
                            }

                            float tdx = cur->targetX - cur->x;
                            float tdy = cur->targetY - cur->y;

                            if(tdx * tdx + tdy * tdy < 10 * 10) {
                                cur->temporary = true;
                                cur->lifetime = 0;
                                //logDebug("vacuum {}", cur->tile.mat->name.c_str());
                                return true;
                            }

                            return false;
                        }), world->player->heldItem->vacuumParticles.end());
                    }

                }
            }
        }
        #pragma endregion

        // update particles, tickObjects, update dirty
        // TODO: this is not entirely thread safe since tickParticles changes World::tiles and World::dirty
        #pragma region
        EASY_BLOCK("post World::tick");
        bool hadDirty = false;
        bool hadLayer2Dirty = false;
        bool hadBackgroundDirty = false;
        bool hadFire = false;

        int pitch;
        //void* vdpixels_ar = texture->data;
        //unsigned char* dpixels_ar = (unsigned char*)vdpixels_ar;
        unsigned char* dpixels_ar = pixels_ar;
        unsigned char* dpixelsFire_ar = pixelsFire_ar;

        for(size_t i = 0; i < world->rigidBodies.size(); i++) {
            RigidBody* cur = world->rigidBodies[i];
            if(cur == nullptr) continue;
            if(cur->surface == nullptr) continue;
            if(!cur->body->IsEnabled()) continue;

            float x = cur->body->GetPosition().x;
            float y = cur->body->GetPosition().y;

            float s = sin(cur->body->GetAngle());
            float c = cos(cur->body->GetAngle());

            std::vector<std::pair<int, int>> checkDirs = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for(int tx = 0; tx < cur->matWidth; tx++) {
                for(int ty = 0; ty < cur->matHeight; ty++) {
                    MaterialInstance rmat = cur->tiles[tx + ty * cur->matWidth];
                    if(rmat.mat->id == Materials::GENERIC_AIR.id) continue;

                    // rotate point
                    int wx = (int)(tx * c - ty * s + x);
                    int wy = (int)(tx * s + ty * c + y);

                    bool found = false;
                    for(auto& dir : checkDirs) {
                        int wxd = wx + dir.first;
                        int wyd = wy + dir.second;

                        if(wxd < 0 || wyd < 0 || wxd >= world->width || wyd >= world->height) continue;
                        if(world->tiles[wxd + wyd * world->width] == rmat) {
                            cur->tiles[tx + ty * cur->matWidth] = world->tiles[wxd + wyd * world->width];
                            world->tiles[wxd + wyd * world->width] = Tiles::NOTHING;
                            world->dirty[wxd + wyd * world->width] = true;
                            found = true;
                            break;
                        }
                    }

                    if(!found) {
                        if(world->tiles[wx + wy * world->width].mat->id == Materials::GENERIC_AIR.id) {
                            cur->tiles[tx + ty * cur->matWidth] = Tiles::NOTHING;
                        }
                    }
                }
            }

            for(int x = 0; x < cur->surface->w; x++) {
                for(int y = 0; y < cur->surface->h; y++) {
                    MaterialInstance mat = cur->tiles[x + y * cur->surface->w];
                    if(mat.mat->id == Materials::GENERIC_AIR.id) {
                        PIXEL(cur->surface, x, y) = 0x00000000;
                    } else {
                        PIXEL(cur->surface, x, y) = (mat.mat->alpha << 24) + mat.color;
                    }
                }
            }

            cur->texture = GPU_CopyImageFromSurface(cur->surface);
            GPU_SetImageFilter(cur->texture, GPU_FILTER_NEAREST);

            cur->needsUpdate = true;
        }

        std::vector<std::future<void>> results = {};

        results.push_back(updateDirtyPool->push([&](int id) {
            EASY_BLOCK("particles");
            //SDL_SetRenderTarget(renderer, textureParticles);
            void* particlePixels = pixelsParticles_ar;
            EASY_BLOCK("memset");
            memset(particlePixels, 0, world->width * world->height * 4);
            EASY_END_BLOCK; // memset
            world->renderParticles((unsigned char**)&particlePixels);
            world->tickParticles();

            //SDL_SetRenderTarget(renderer, NULL);
            EASY_END_BLOCK; // particles
        }));

        if(Settings::tick_box2d && world->readyToMerge.size() == 0) {
            results.push_back(updateDirtyPool->push([&](int id) {
                world->tickObjects();
            }));
        }

        EASY_BLOCK("wait for threads", THREAD_WAIT_PROFILER_COLOR);
        for(int i = 0; i < results.size(); i++) {
            results[i].get();
        }
        EASY_END_BLOCK;

        if(tickTime % 10 == 0) world->tickObjectsMesh();

        results.clear();
        results.push_back(updateDirtyPool->push([&](int id) {
            EASY_BLOCK("dirty");
            for(int i = 0; i < world->width * world->height; i++) {
                const unsigned int offset = i * 4;

                if(world->dirty[i]) {
                    hadDirty = true;
                    if(world->tiles[i].mat->physicsType == PhysicsType::AIR) {
                        dpixels_ar[offset + 0] = 0;        // b
                        dpixels_ar[offset + 1] = 0;        // g
                        dpixels_ar[offset + 2] = 0;        // r
                        dpixels_ar[offset + 3] = SDL_ALPHA_TRANSPARENT;    // a		

                        dpixelsFire_ar[offset + 0] = 0;        // b
                        dpixelsFire_ar[offset + 1] = 0;        // g
                        dpixelsFire_ar[offset + 2] = 0;        // r
                        dpixelsFire_ar[offset + 3] = SDL_ALPHA_TRANSPARENT;    // a		
                    } else {
                        Uint32 color = world->tiles[i].color;
                        //float br = world->light[i];
                        dpixels_ar[offset + 2] = ((color >> 0) & 0xff);        // b
                        dpixels_ar[offset + 1] = ((color >> 8) & 0xff);        // g
                        dpixels_ar[offset + 0] = ((color >> 16) & 0xff);        // r
                        dpixels_ar[offset + 3] = world->tiles[i].mat->alpha;    // a

                        if(world->tiles[i].mat->id == Materials::FIRE.id) {
                            dpixelsFire_ar[offset + 2] = ((color >> 0) & 0xff);        // b
                            dpixelsFire_ar[offset + 1] = ((color >> 8) & 0xff);        // g
                            dpixelsFire_ar[offset + 0] = ((color >> 16) & 0xff);        // r
                            dpixelsFire_ar[offset + 3] = world->tiles[i].mat->alpha;    // a
                            hadFire = true;
                        }
                    }
                }
            }
            EASY_END_BLOCK;
        }));

        //void* vdpixelsLayer2_ar = textureLayer2->data;
        //unsigned char* dpixelsLayer2_ar = (unsigned char*)vdpixelsLayer2_ar;
        unsigned char* dpixelsLayer2_ar = pixelsLayer2_ar;
        results.push_back(updateDirtyPool->push([&](int id) {
            EASY_BLOCK("layer2Dirty");
            for(int i = 0; i < world->width * world->height; i++) {
                /*for (int x = 0; x < world->width; x++) {
                    for (int y = 0; y < world->height; y++) {*/
                    //const unsigned int i = x + y * world->width;
                const unsigned int offset = i * 4;
                if(world->layer2Dirty[i]) {
                    hadLayer2Dirty = true;
                    if(world->layer2[i].mat->physicsType == PhysicsType::AIR) {
                        if(Settings::draw_background_grid) {
                            Uint32 color = ((i) % 2) == 0 ? 0x888888 : 0x444444;
                            dpixelsLayer2_ar[offset + 2] = (color >> 0) & 0xff;        // b
                            dpixelsLayer2_ar[offset + 1] = (color >> 8) & 0xff;        // g
                            dpixelsLayer2_ar[offset + 0] = (color >> 16) & 0xff;       // r
                            dpixelsLayer2_ar[offset + 3] = SDL_ALPHA_OPAQUE;			 // a
                            continue;
                        } else {
                            dpixelsLayer2_ar[offset + 0] = 0;        // b
                            dpixelsLayer2_ar[offset + 1] = 0;        // g
                            dpixelsLayer2_ar[offset + 2] = 0;        // r
                            dpixelsLayer2_ar[offset + 3] = SDL_ALPHA_TRANSPARENT;    // a
                            continue;
                        }
                    }
                    Uint32 color = world->layer2[i].color;
                    dpixelsLayer2_ar[offset + 2] = (color >> 0) & 0xff;        // b
                    dpixelsLayer2_ar[offset + 1] = (color >> 8) & 0xff;        // g
                    dpixelsLayer2_ar[offset + 0] = (color >> 16) & 0xff;        // r
                    dpixelsLayer2_ar[offset + 3] = world->layer2[i].mat->alpha;    // a
                }
            }
            EASY_END_BLOCK;
        }));

        //void* vdpixelsBackground_ar = textureBackground->data;
        //unsigned char* dpixelsBackground_ar = (unsigned char*)vdpixelsBackground_ar;
        unsigned char* dpixelsBackground_ar = pixelsBackground_ar;
        results.push_back(updateDirtyPool->push([&](int id) {
            EASY_BLOCK("backgroundDirty");
            for(int i = 0; i < world->width * world->height; i++) {
                /*for (int x = 0; x < world->width; x++) {
                    for (int y = 0; y < world->height; y++) {*/
                    //const unsigned int i = x + y * world->width;
                const unsigned int offset = i * 4;

                if(world->backgroundDirty[i]) {
                    hadBackgroundDirty = true;
                    Uint32 color = world->background[i];
                    dpixelsBackground_ar[offset + 2] = (color >> 0) & 0xff;        // b
                    dpixelsBackground_ar[offset + 1] = (color >> 8) & 0xff;        // g
                    dpixelsBackground_ar[offset + 0] = (color >> 16) & 0xff;       // r
                    dpixelsBackground_ar[offset + 3] = (color >> 24) & 0xff;       // a
                }

                //}
            }
            EASY_END_BLOCK;
        }));

        EASY_BLOCK("objectDelete");
        for(int i = 0; i < world->width * world->height; i++) {
            /*for (int x = 0; x < world->width; x++) {
                for (int y = 0; y < world->height; y++) {*/
                //const unsigned int i = x + y * world->width;
            const unsigned int offset = i * 4;

            if(objectDelete[i]) {
                world->tiles[i] = Tiles::NOTHING;
            }
        }
        EASY_END_BLOCK;

        //results.push_back(updateDirtyPool->push([&](int id) {

        //}));
        EASY_BLOCK("wait for threads", THREAD_WAIT_PROFILER_COLOR);
        for(int i = 0; i < results.size(); i++) {
            results[i].get();
        }
        EASY_END_BLOCK;

        EASY_BLOCK("particle GPU_UpdateImageBytes");
        GPU_UpdateImageBytes(
            textureParticles,
            NULL,
            &pixelsParticles_ar[0],
            world->width * 4
        );
        EASY_END_BLOCK; // GPU_UpdateImageBytes

        EASY_BLOCK("dirty memset");
        if(hadDirty)		   memset(world->dirty, false, world->width * world->height);
        if(hadLayer2Dirty)	   memset(world->layer2Dirty, false, world->width * world->height);
        if(hadBackgroundDirty) memset(world->backgroundDirty, false, world->width * world->height);
        EASY_END_BLOCK;

        EASY_END_BLOCK; // post World::tick
        #pragma endregion

        if(Settings::draw_light_map && tickTime % 4 == 0) renderLightmap(world);
        if(Settings::tick_temperature && tickTime % 4 == 2) {
            world->tickTemperature();
        }
        if(Settings::draw_temperature_map && tickTime % 4 == 0) {
            renderTemperatureMap(world);
        }

        EASY_BLOCK("GPU_UpdateImageBytes", GPU_PROFILER_COLOR);
        if(hadDirty) {
            GPU_UpdateImageBytes(
                texture,
                NULL,
                &pixels[0],
                world->width * 4
            );
        }

        if(hadLayer2Dirty) {
            GPU_UpdateImageBytes(
                textureLayer2,
                NULL,
                &pixelsLayer2[0],
                world->width * 4
            );
        }

        if(hadBackgroundDirty) {
            GPU_UpdateImageBytes(
                textureBackground,
                NULL,
                &pixelsBackground[0],
                world->width * 4
            );
        }

        if(hadFire) {
            GPU_UpdateImageBytes(
                textureFire,
                NULL,
                &pixelsFire[0],
                world->width * 4
            );
        }

        if(Settings::draw_temperature_map) {
            GPU_UpdateImageBytes(
                temperatureMap,
                NULL,
                &pixelsTemp[0],
                world->width * 4
            );
        }

        /*GPU_UpdateImageBytes(
            textureObjects,
            NULL,
            &pixelsObjects[0],
            world->width * 4
        );*/
        EASY_END_BLOCK;

        if(Settings::tick_box2d && tickTime % 4 == 0) world->updateWorldMesh();

        EASY_END_BLOCK;
    }
}

void Game::updateFrameLate() {
    EASY_FUNCTION(GAME_PROFILER_COLOR);
    if(state == LOADING) {

    } else {

        // update camera
        #pragma region
        int nofsX;
        int nofsY;

        if(world->player) {

            if(now - lastTick <= mspt) {
                float thruTick = (float)((now - lastTick) / (double)mspt);

                plPosX = world->player->x + (int)(world->player->vx * thruTick);
                plPosY = world->player->y + (int)(world->player->vy * thruTick);
            } else {
                //plPosX = world->player->x;
                //plPosY = world->player->y;
            }

            //plPosX = (float)(plPosX + (world->player->x - plPosX) / 25.0);
            //plPosY = (float)(plPosY + (world->player->y - plPosY) / 25.0);

            nofsX = (int)(-((int)plPosX + world->player->hw / 2 + world->loadZone.x) * scale + WIDTH / 2);
            nofsY = (int)(-((int)plPosY + world->player->hh / 2 + world->loadZone.y) * scale + HEIGHT / 2);
        } else {
            plPosX = (float)(plPosX + (freeCamX - plPosX) / 50.0f);
            plPosY = (float)(plPosY + (freeCamY - plPosY) / 50.0f);

            nofsX = (int)(-(plPosX + 0 + world->loadZone.x) * scale + WIDTH / 2);
            nofsY = (int)(-(plPosY + 0 + world->loadZone.y) * scale + HEIGHT / 2);
        }

        accLoadX += (nofsX - ofsX) / (float)scale;
        accLoadY += (nofsY - ofsY) / (float)scale;
        //logDebug("{0:f} {0:f}", plPosX, plPosY);
        //logDebug("a {0:d} {0:d}", nofsX, nofsY);
        //logDebug("{0:d} {0:d}", nofsX - ofsX, nofsY - ofsY);
        ofsX += (nofsX - ofsX);
        ofsY += (nofsY - ofsY);
        #pragma endregion

        camX = (float)(camX + (desCamX - camX) * (now - lastTime) / 250.0f);
        camY = (float)(camY + (desCamY - camY) * (now - lastTime) / 250.0f);
    }
}

void Game::renderEarly() {
    EASY_FUNCTION(GAME_PROFILER_COLOR);
    if(state == LOADING) {
        if(now - lastLoadingTick > 20) {
            // render loading screen
            #pragma region
            EASY_BLOCK("render loading screen");
            unsigned int* ldPixels = (unsigned int*)pixelsLoading_ar;
            bool anyFalse = false;
            //int drop  = (sin(now / 250.0) + 1) / 2 * loadingScreenW;
            //int drop2 = (-sin(now / 250.0) + 1) / 2 * loadingScreenW;
            for(int x = 0; x < loadingScreenW; x++) {
                for(int y = loadingScreenH - 1; y >= 0; y--) {
                    int i = (x + y * loadingScreenW);
                    bool state = ldPixels[i] == loadingOnColor;

                    if(!state) anyFalse = true;
                    bool newState = state;
                    //newState = rand() % 2;

                    if(!state && y == 0) {
                        if(rand() % 6 == 0) {
                            newState = true;
                        }
                        /*if (x >= drop - 1 && x <= drop + 1) {
                            newState = true;
                        }else if (x >= drop2 - 1 && x <= drop2 + 1) {
                            newState = true;
                        }*/
                    }

                    if(state && y < loadingScreenH - 1) {
                        if(ldPixels[(x + (y + 1) * loadingScreenW)] == loadingOffColor) {
                            ldPixels[(x + (y + 1) * loadingScreenW)] = loadingOnColor;
                            newState = false;
                        } else {
                            bool canLeft = x > 0 && ldPixels[((x - 1) + (y + 1) * loadingScreenW)] == loadingOffColor;
                            bool canRight = x < loadingScreenW - 1 && ldPixels[((x + 1) + (y + 1) * loadingScreenW)] == loadingOffColor;
                            if(canLeft && !(canRight && (rand() % 2 == 0))) {
                                ldPixels[((x - 1) + (y + 1) * loadingScreenW)] = loadingOnColor;
                                newState = false;
                            } else if(canRight) {
                                ldPixels[((x + 1) + (y + 1) * loadingScreenW)] = loadingOnColor;
                                newState = false;
                            }
                        }
                    }

                    ldPixels[(x + y * loadingScreenW)] = (newState ? loadingOnColor : loadingOffColor);
                    int sx = WIDTH / loadingScreenW;
                    int sy = HEIGHT / loadingScreenH;
                    //GPU_RectangleFilled(target, x * sx, y * sy, x * sx + sx, y * sy + sy, state ? SDL_Color{ 0xff, 0, 0, 0xff } : SDL_Color{ 0, 0xff, 0, 0xff });
                }
            }
            if(!anyFalse) {
                uint32 tmp = loadingOnColor;
                loadingOnColor = loadingOffColor;
                loadingOffColor = tmp;
            }

            GPU_UpdateImageBytes(
                loadingTexture,
                NULL,
                &pixelsLoading_ar[0],
                loadingScreenW * 4
            );
            EASY_END_BLOCK;
            #pragma endregion

            lastLoadingTick = now;
        } else {
            Sleep(5);
        }
        GPU_ActivateShaderProgram(0, NULL);
        GPU_BlitRect(loadingTexture, NULL, target, NULL);
        if(dt_loading.w == -1) {
            dt_loading = Drawing::drawTextParams(target, "Loading...", font64, WIDTH / 2, HEIGHT / 2 - 32, 255, 255, 255, ALIGN_CENTER);
        }
        Drawing::drawText(target, dt_loading, WIDTH / 2, HEIGHT / 2 - 32, ALIGN_CENTER);
    } else {
        // render entities with LERP
        #pragma region
        if(world->player) {
            if(now - lastTick <= mspt) {
                float thruTick = (float)((now - lastTick) / (double)mspt);
                EASY_BLOCK("render entities LERP");
                GPU_SetBlendMode(textureEntities, GPU_BLEND_ADD);
                GPU_Clear(textureEntities->target);
                for(auto& v : world->entities) {
                    v->render(textureEntities->target, world->loadZone.x + (int)(v->vx * thruTick), world->loadZone.y + (int)(v->vy * thruTick));
                }

                if(world->player && world->player->heldItem != NULL) {
                    if(world->player->heldItem->getFlag(ItemFlags::HAMMER)) {
                        if(world->player->holdHammer) {
                            int x = (int)((mx - ofsX - camX) / scale);
                            int y = (int)((my - ofsY - camY) / scale);

                            int dx = x - world->player->hammerX;
                            int dy = y - world->player->hammerY;
                            float len = sqrt(dx * dx + dy * dy);
                            if(len > 40) {
                                dx = dx / len * 40;
                                dy = dy / len * 40;
                            }

                            GPU_Line(textureEntities->target, world->player->hammerX + dx, world->player->hammerY + dy, world->player->hammerX, world->player->hammerY, {0xff, 0xff, 0x00, 0xff});
                        } else {
                            int startInd = getAimSolidSurface(64);

                            if(startInd != -1) {
                                int x = startInd % world->width;
                                int y = startInd / world->width;
                                GPU_Rectangle(textureEntities->target, x - 1, y - 1, x + 1, y + 1, {0xff, 0xff, 0x00, 0xE0});
                            }
                        }
                    }
                }
                GPU_SetBlendMode(textureEntities, GPU_BLEND_NORMAL);
                EASY_END_BLOCK;
            }
        }
        #pragma endregion
    }
}

void Game::renderLate() {
    EASY_FUNCTION(GAME_PROFILER_COLOR);

    target = backgroundImage->target;
    GPU_Clear(target);

    if(state == LOADING) {

    } else {
        // draw backgrounds
        #pragma region
        EASY_BLOCK("draw background", RENDER_PROFILER_COLOR);
        Background bg = Backgrounds::TEST_OVERWORLD;
        if(Settings::draw_background && scale <= bg.layers[0].surface.size() && world->loadZone.y > -5 * CHUNK_H) {
            GPU_SetShapeBlendMode(GPU_BLEND_SET);
            GPU_ClearColor(target, {(bg.solid >> 16) & 0xff, (bg.solid >> 8) & 0xff, (bg.solid >> 0) & 0xff, 0xff});

            GPU_Rect* dst = new GPU_Rect();
            GPU_Rect* src = new GPU_Rect();

            float arX = (float)WIDTH / (bg.layers[0].surface[0]->w);
            float arY = (float)HEIGHT / (bg.layers[0].surface[0]->h);

            double time = Time::millis() / 1000.0;

            GPU_SetShapeBlendMode(GPU_BLEND_NORMAL);

            for(size_t i = 0; i < bg.layers.size(); i++) {
                BackgroundLayer cur = bg.layers[i];

                SDL_Surface* texture = cur.surface[scale - 1];

                GPU_Image* tex = cur.texture[scale - 1];
                GPU_SetBlendMode(tex, GPU_BLEND_NORMAL);

                int tw = texture->w;
                int th = texture->h;

                int iter = (int)ceil((float)WIDTH / (tw)) + 1;
                for(int n = 0; n < iter; n++) {

                    src->x = 0;
                    src->y = 0;
                    src->w = tw;
                    src->h = th;

                    dst->x = (((ofsX + camX) + world->loadZone.x*scale) + n * tw / cur.parralaxX) * cur.parralaxX + world->width / 2 * scale - tw / 2;
                    dst->y = ((ofsY + camY) + world->loadZone.y*scale) * cur.parralaxY + world->height / 2 * scale - th / 2 - HEIGHT / 3 * (scale - 1);
                    dst->w = (float)tw;
                    dst->h = (float)th;

                    dst->x += (float)(scale * fmod(cur.moveX * time, tw));

                    //TODO: optimize
                    while(dst->x >= WIDTH - 10) dst->x -= (iter * tw);
                    while(dst->x + dst->w < 0) dst->x += (iter * tw - 1);

                    //TODO: optimize
                    if(dst->x < 0) {
                        dst->w += dst->x;
                        src->x -= (int)dst->x;
                        src->w += (int)dst->x;
                        dst->x = 0;
                    }

                    if(dst->y < 0) {
                        dst->h += dst->y;
                        src->y -= (int)dst->y;
                        src->h += (int)dst->y;
                        dst->y = 0;
                    }

                    if(dst->x + dst->w >= WIDTH) {
                        src->w -= (int)((dst->x + dst->w) - WIDTH);
                        dst->w += WIDTH - (dst->x + dst->w);
                    }

                    if(dst->y + dst->h >= HEIGHT) {
                        src->h -= (int)((dst->y + dst->h) - HEIGHT);
                        dst->h += HEIGHT - (dst->y + dst->h);
                    }

                    GPU_BlitRect(tex, src, target, dst);
                }
            }

            delete dst;
            delete src;
        }
        EASY_END_BLOCK; // draw backgrounds
        #pragma endregion

        EASY_BLOCK("draw world textures", RENDER_PROFILER_COLOR);
        GPU_Rect r1 = GPU_Rect {(float)(ofsX + camX), (float)(ofsY + camY), (float)(world->width * scale), (float)(world->height * scale)};
        GPU_SetBlendMode(textureBackground, GPU_BLEND_NORMAL);
        GPU_BlitRect(textureBackground, NULL, target, &r1);

        GPU_SetBlendMode(textureLayer2, GPU_BLEND_NORMAL);
        GPU_BlitRect(textureLayer2, NULL, target, &r1);

        GPU_SetBlendMode(textureObjectsBack, GPU_BLEND_NORMAL);
        GPU_BlitRect(textureObjectsBack, NULL, target, &r1);

        // shader
        EASY_BLOCK("water shader");
        GPU_ActivateShaderProgram(water_shader, &water_shader_block);
        float t = (now - startTime) / 1000.0;
        //Shaders::update_marching_ants_shader(marching_ants_shader, t, target->w * scale, target->h * scale, texture, r1.x, HEIGHT - r1.y, r1.w, r1.h, scale);

        target = realTarget;

        GPU_BlitRect(backgroundImage, NULL, target, NULL);

        GPU_SetBlendMode(texture, GPU_BLEND_NORMAL);
        GPU_ActivateShaderProgram(0, NULL);
        EASY_END_BLOCK; //water shader

        // done shader

        EASY_BLOCK("lighting shader");
        int msx = (int)((mx - ofsX - camX) / scale);
        int msy = (int)((my - ofsY - camY) / scale);

        GPU_Clear(worldTexture->target);

        GPU_BlitRect(texture, NULL, worldTexture->target, NULL);

        GPU_SetBlendMode(textureObjects, GPU_BLEND_NORMAL);
        GPU_BlitRect(textureObjects, NULL, worldTexture->target, NULL);

        GPU_SetBlendMode(textureParticles, GPU_BLEND_NORMAL);
        GPU_BlitRect(textureParticles, NULL, worldTexture->target, NULL);

        GPU_SetBlendMode(textureEntities, GPU_BLEND_NORMAL);
        GPU_BlitRect(textureEntities, NULL, worldTexture->target, NULL);

        //if (Settings::draw_shaders) raycastLightingShader->activate();
        //if (Settings::draw_shaders) raycastLightingShader->update(worldTexture, msx / (float)world->width, msy / (float)world->height);
        if(Settings::draw_shaders) simpleLightingShader->activate();
        if(Settings::draw_shaders) simpleLightingShader->update(worldTexture, msx / (float)world->width, msy / (float)world->height);

        GPU_Clear(lightingTexture->target);
        GPU_BlitRect(worldTexture, NULL, lightingTexture->target, NULL);
        //GPU_BlitRect(worldTexture, NULL, target, &r1);

        if(Settings::draw_shaders) simpleLighting2Shader->activate();
        if(Settings::draw_shaders) simpleLighting2Shader->update(worldTexture, lightingTexture, msx / (float)world->width, msy / (float)world->height);

        GPU_BlitRect(worldTexture, NULL, target, &r1);

        if(Settings::draw_shaders) GPU_ActivateShaderProgram(0, NULL);

        EASY_END_BLOCK; //lighting shader

        GPU_Clear(texture2Fire->target);
        EASY_BLOCK("fire shader 1");
        fireShader->activate();
        fireShader->update(textureFire);
        GPU_BlitRect(textureFire, NULL, texture2Fire->target, NULL);
        GPU_ActivateShaderProgram(0, NULL);
        EASY_END_BLOCK;

        EASY_BLOCK("fire shader 2");
        fire2Shader->activate();
        fire2Shader->update(texture2Fire);
        GPU_BlitRect(texture2Fire, NULL, target, &r1);
        GPU_ActivateShaderProgram(0, NULL);
        EASY_END_BLOCK;

        // done light



        GPU_Rect r2 = GPU_Rect {(float)(ofsX + camX + world->tickZone.x*scale), (float)(ofsY + camY + world->tickZone.y*scale), (float)(world->tickZone.w * scale), (float)(world->tickZone.h * scale)};
        if(Settings::draw_light_map) {
            GPU_SetBlendMode(lightMap, GPU_BLEND_MULTIPLY);
            GPU_BlitRect(lightMap, NULL, target, &r2);
        }

        if(Settings::draw_temperature_map) {
            GPU_SetBlendMode(temperatureMap, GPU_BLEND_NORMAL);
            GPU_BlitRect(temperatureMap, NULL, target, &r1);
        }
        EASY_END_BLOCK; // draw world textures

        if(Settings::draw_load_zones) {
            GPU_Rect r2m = GPU_Rect {(float)(ofsX + camX + world->meshZone.x*scale),
                (float)(ofsY + camY + world->meshZone.y*scale),
                (float)(world->meshZone.w * scale),
                (float)(world->meshZone.h * scale)};

            GPU_Rectangle2(target, r2m, {0x00, 0xff, 0xff, 0xff});
            GPU_Rectangle2(target, r2, {0xff, 0x00, 0x00, 0xff});
        }

        if(Settings::draw_load_zones) {
            EASY_BLOCK("draw load zones", RENDER_PROFILER_COLOR);
            SDL_Color col = {0xff, 0x00, 0x00, 0x20};
            GPU_SetShapeBlendMode(GPU_BLEND_NORMAL);

            GPU_Rect r3 = GPU_Rect {(float)(0), (float)(0), (float)((ofsX + camX + world->tickZone.x*scale)), (float)(HEIGHT)};
            GPU_Rectangle2(target, r3, col);

            GPU_Rect r4 = GPU_Rect {(float)(ofsX + camX + world->tickZone.x*scale + world->tickZone.w * scale), (float)(0), (float)((WIDTH)-(ofsX + camX + world->tickZone.x*scale + world->tickZone.w * scale)), (float)(HEIGHT)};
            GPU_Rectangle2(target, r3, col);

            GPU_Rect r5 = GPU_Rect {(float)(ofsX + camX + world->tickZone.x*scale), (float)(0), (float)(world->tickZone.w * scale), (float)(ofsY + camY + world->tickZone.y*scale)};
            GPU_Rectangle2(target, r3, col);

            GPU_Rect r6 = GPU_Rect {(float)(ofsX + camX + world->tickZone.x*scale), (float)(ofsY + camY + world->tickZone.y*scale + world->tickZone.h * scale), (float)(world->tickZone.w * scale), (float)(HEIGHT - (ofsY + camY + world->tickZone.y*scale + world->tickZone.h * scale))};
            GPU_Rectangle2(target, r6, col);

            col = {0x00, 0xff, 0x00, 0xff};
            GPU_Rect r7 = GPU_Rect {(float)(ofsX + camX + world->width / 2 * scale - (WIDTH / 3 * scale / 2)), (float)(ofsY + camY + world->height / 2 * scale - (HEIGHT / 3 * scale / 2)), (float)(WIDTH / 3 * scale), (float)(HEIGHT / 3 * scale)};
            GPU_Rectangle2(target, r7, col);

            EASY_END_BLOCK; // draw load zones
        }

        if(Settings::draw_physics_meshes) {
            EASY_BLOCK("draw physics meshes", RENDER_PROFILER_COLOR);
            for(size_t i = 0; i < world->rigidBodies.size(); i++) {
                RigidBody cur = *world->rigidBodies[i];

                float x = cur.body->GetPosition().x;
                float y = cur.body->GetPosition().y;
                x = ((x)* scale + ofsX + camX);
                y = ((y)* scale + ofsY + camY);

                /*SDL_Rect* r = new SDL_Rect{ (int)x, (int)y, cur.surface->w * scale, cur.surface->h * scale };
                SDL_RenderCopyEx(renderer, cur.texture, NULL, r, cur.body->GetAngle() * 180 / M_PI, new SDL_Point{ 0, 0 }, SDL_RendererFlip::SDL_FLIP_NONE);
                delete r;*/

                Uint32 color = 0x0000ff;

                SDL_Color col = {(color >> 16) & 0xff, (color >> 8) & 0xff, (color >> 0) & 0xff, 0xff};

                b2Fixture* fix = cur.body->GetFixtureList();
                while(fix) {
                    b2Shape* shape = fix->GetShape();

                    switch(shape->GetType()) {
                    case b2Shape::Type::e_polygon:
                        b2PolygonShape* poly = (b2PolygonShape*)shape;
                        b2Vec2* verts = poly->m_vertices;

                        Drawing::drawPolygon(target, col, verts, (int)x, (int)y, scale, poly->m_count, cur.body->GetAngle()/* + fmod((Time::millis() / 1000.0), 360)*/, 0, 0);

                        break;
                    }

                    fix = fix->GetNext();
                }
            }

            if(world->player) {
                RigidBody cur = *world->player->rb;

                float x = cur.body->GetPosition().x;
                float y = cur.body->GetPosition().y;
                x = ((x)* scale + ofsX + camX);
                y = ((y)* scale + ofsY + camY);

                Uint32 color = 0x0000ff;

                SDL_Color col = {(color >> 16) & 0xff, (color >> 8) & 0xff, (color >> 0) & 0xff, 0xff};

                b2Fixture* fix = cur.body->GetFixtureList();
                while(fix) {
                    b2Shape* shape = fix->GetShape();
                    switch(shape->GetType()) {
                    case b2Shape::Type::e_polygon:
                        b2PolygonShape* poly = (b2PolygonShape*)shape;
                        b2Vec2* verts = poly->m_vertices;

                        Drawing::drawPolygon(target, col, verts, (int)x, (int)y, scale, poly->m_count, cur.body->GetAngle()/* + fmod((Time::millis() / 1000.0), 360)*/, 0, 0);

                        break;
                    }

                    fix = fix->GetNext();
                }
            }

            for(size_t i = 0; i < world->worldRigidBodies.size(); i++) {
                RigidBody cur = *world->worldRigidBodies[i];

                float x = cur.body->GetPosition().x;
                float y = cur.body->GetPosition().y;
                x = ((x)* scale + ofsX + camX);
                y = ((y)* scale + ofsY + camY);

                Uint32 color = 0x00ff00;

                SDL_Color col = {(color >> 16) & 0xff, (color >> 8) & 0xff, (color >> 0) & 0xff, 0xff};

                b2Fixture* fix = cur.body->GetFixtureList();
                while(fix) {
                    b2Shape* shape = fix->GetShape();
                    switch(shape->GetType()) {
                    case b2Shape::Type::e_polygon:
                        b2PolygonShape* poly = (b2PolygonShape*)shape;
                        b2Vec2* verts = poly->m_vertices;

                        Drawing::drawPolygon(target, col, verts, (int)x, (int)y, scale, poly->m_count, cur.body->GetAngle()/* + fmod((Time::millis() / 1000.0), 360)*/, 0, 0);

                        break;
                    }

                    fix = fix->GetNext();
                }

                /*color = 0xff0000;
                SDL_SetRenderDrawColor(renderer, (color >> 16) & 0xff, (color >> 8) & 0xff, (color >> 0) & 0xff, 0xff);
                SDL_RenderDrawPoint(renderer, x, y);
                color = 0x00ff00;
                SDL_SetRenderDrawColor(renderer, (color >> 16) & 0xff, (color >> 8) & 0xff, (color >> 0) & 0xff, 0xff);
                SDL_RenderDrawPoint(renderer, cur.body->GetLocalCenter().x * scale + ofsX, cur.body->GetLocalCenter().y * scale + ofsY);*/
            }

            int minChX = (int)floor((world->meshZone.x - world->loadZone.x) / CHUNK_W);
            int minChY = (int)floor((world->meshZone.y - world->loadZone.y) / CHUNK_H);
            int maxChX = (int)ceil((world->meshZone.x + world->meshZone.w - world->loadZone.x) / CHUNK_W);
            int maxChY = (int)ceil((world->meshZone.y + world->meshZone.h - world->loadZone.y) / CHUNK_H);

            /*minChX = 0;
            minChY = 0;
            maxChX = 4;
            maxChY = 4;*/

            for(int cx = minChX; cx <= maxChX; cx++) {
                for(int cy = minChY; cy <= maxChY; cy++) {
                    Chunk* ch = world->getChunk(cx, cy);
                    SDL_Color col = {255, 0, 0, 255};

                    float x = ((ch->x * CHUNK_W + world->loadZone.x)* scale + ofsX + camX);
                    float y = ((ch->y * CHUNK_H + world->loadZone.y)* scale + ofsY + camY);

                    GPU_Rectangle(target, x, y, x + CHUNK_W * scale, y + CHUNK_H * scale, {0, 0, 0, 255});

                    for(int i = 0; i < ch->polys.size(); i++) {
                        Drawing::drawPolygon(target, col, ch->polys[i].m_vertices, (int)x, (int)y, scale, ch->polys[i].m_count, 0/* + fmod((Time::millis() / 1000.0), 360)*/, 0, 0);
                    }


                }
            }

            EASY_END_BLOCK; // draw physics meshes
        }

        if(Settings::draw_material_info) {
            EASY_BLOCK("draw material info", RENDER_PROFILER_COLOR);
            int msx = (int)((mx - ofsX - camX) / scale);
            int msy = (int)((my - ofsY - camY) / scale);

            if(msx >= 0 && msy >= 0 && msx < world->width && msy < world->height) {
                MaterialInstance tile = world->tiles[msx + msy * world->width];
                Drawing::drawText(target, tile.mat->name.c_str(), font16, 2, 2, 0xff, 0xff, 0xff, ALIGN_LEFT);
                int ln = 0;
                if(tile.mat->interact) {
                    for(size_t i = 0; i < Materials::MATERIALS.size(); i++) {
                        if(tile.mat->nInteractions[i] > 0) {
                            char buff2[40];
                            snprintf(buff2, sizeof(buff2), "    %s", Materials::MATERIALS[i]->name.c_str());
                            Drawing::drawText(target, buff2, font16, 2, 2 + 14 * ++ln, 0xff, 0xff, 0xff, ALIGN_LEFT);

                            for(int j = 0; j < tile.mat->nInteractions[i]; j++) {
                                MaterialInteraction inter = tile.mat->interactions[i][j];
                                char buff1[40];
                                if(inter.type == INTERACT_TRANSFORM_MATERIAL) {
                                    snprintf(buff1, sizeof(buff1), "        %s %s r=%d x=%d y=%d", "TRANSFORM", Materials::MATERIALS[inter.data1]->name.c_str(), inter.data2, inter.ofsX, inter.ofsY);
                                } else if(inter.type == INTERACT_SPAWN_MATERIAL) {
                                    snprintf(buff1, sizeof(buff1), "        %s %s r=%d x=%d y=%d", "SPAWN", Materials::MATERIALS[inter.data1]->name.c_str(), inter.data2, inter.ofsX, inter.ofsY);
                                }
                                Drawing::drawText(target, buff1, font16, 2, 2 + 14 * ++ln, 0xff, 0xff, 0xff, ALIGN_LEFT);
                            }
                        }
                    }
                }
            }

            EASY_END_BLOCK; // draw material info
        }

        EASY_BLOCK("draw fps", RENDER_PROFILER_COLOR);
        if(dt_fps.w == -1) {
            char buffFps[20];
            snprintf(buffFps, sizeof(buffFps), "%d FPS", fps);
            dt_fps = Drawing::drawTextParams(target, buffFps, font16, WIDTH - 4, 2, 0xff, 0xff, 0xff, ALIGN_RIGHT);
        }

        Drawing::drawText(target, dt_fps, WIDTH - 4, 2, ALIGN_RIGHT);
        EASY_END_BLOCK; // draw fps

        if(Settings::draw_chunk_state) {
            EASY_BLOCK("draw chunk state", RENDER_PROFILER_COLOR);
            GPU_Rect r = {0 , 0, 10, 10};
            for(auto& p : world->chunkCache) {
                if(p.first == INT_MIN) continue;
                int cx = p.first;
                for(auto& p2 : p.second) {
                    if(p2.first == INT_MIN) continue;
                    int cy = p2.first;
                    Chunk* m = p2.second;
                    r.x = WIDTH / 2 + m->x * 10;
                    r.y = HEIGHT / 2 + m->y * 10;
                    SDL_Color col;
                    if(m->generationPhase == -1) {
                        col = {0x60, 0x60, 0x60, 0xff};
                    } else if(m->generationPhase == 0) {
                        col = {0xff, 0x00, 0x00, 0xff};
                    } else if(m->generationPhase == 1) {
                        col = {0x00, 0xff, 0x00, 0xff};
                    } else if(m->generationPhase == 2) {
                        col = {0x00, 0x00, 0xff, 0xff};
                    } else if(m->generationPhase == 3) {
                        col = {0xff, 0xff, 0x00, 0xff};
                    } else if(m->generationPhase == 4) {
                        col = {0xff, 0x00, 0xff, 0xff};
                    } else if(m->generationPhase == 5) {
                        col = {0x00, 0xff, 0xff, 0xff};
                    } else {}
                    GPU_Rectangle2(target, r, col);
                }
            }
            EASY_END_BLOCK; // draw chunk state
        }

        if(Settings::draw_chunk_queue) {
            EASY_BLOCK("draw chunk queue", RENDER_PROFILER_COLOR);
            int dbgIndex = 1;

            char buff1[32];
            snprintf(buff1, sizeof(buff1), "world->readyToReadyToMerge (%d)", world->readyToReadyToMerge.size());
            std::string buffAsStdStr1 = buff1;
            Drawing::drawText(target, buffAsStdStr1.c_str(), font14, 2, 2 + (12 * dbgIndex++), 0xff, 0xff, 0xff, ALIGN_LEFT);
            for(size_t i = 0; i < world->readyToReadyToMerge.size(); i++) {
                char buff[10];
                snprintf(buff, sizeof(buff), "    #%d", i);
                std::string buffAsStdStr = buff;
                Drawing::drawText(target, buffAsStdStr.c_str(), font14, 2, 2 + (12 * dbgIndex++), 0xff, 0xff, 0xff, ALIGN_LEFT);
            }
            char buff2[30];
            snprintf(buff2, sizeof(buff2), "world->readyToMerge (%d)", world->readyToMerge.size());
            std::string buffAsStdStr2 = buff2;
            Drawing::drawText(target, buffAsStdStr2.c_str(), font14, 2, 2 + (12 * dbgIndex++), 0xff, 0xff, 0xff, ALIGN_LEFT);
            for(size_t i = 0; i < world->readyToMerge.size(); i++) {
                char buff[20];
                snprintf(buff, sizeof(buff), "    #%d (%d, %d)", i, world->readyToMerge[i]->x, world->readyToMerge[i]->y);
                std::string buffAsStdStr = buff;
                Drawing::drawText(target, buffAsStdStr.c_str(), font14, 2, 2 + (12 * dbgIndex++), 0xff, 0xff, 0xff, ALIGN_LEFT);
            }

            EASY_END_BLOCK; // draw chunk queue
        }

        if(Settings::draw_frame_graph) {
            EASY_BLOCK("draw frame graph", RENDER_PROFILER_COLOR);
            for(int i = 0; i <= 4; i++) {
                if(dt_frameGraph[i].w == -1) {
                    char buff[20];
                    snprintf(buff, sizeof(buff), "%d", i * 25);
                    std::string buffAsStdStr = buff;
                    dt_frameGraph[i] = Drawing::drawTextParams(target, buffAsStdStr.c_str(), font14, WIDTH - 20, HEIGHT - 15 - (i * 25) - 2, 0xff, 0xff, 0xff, ALIGN_LEFT);
                }

                Drawing::drawText(target, dt_frameGraph[i], WIDTH - 20, HEIGHT - 15 - (i * 25) - 2, ALIGN_LEFT);
                GPU_Line(target, WIDTH - 30 - frameTimeNum - 5, HEIGHT - 10 - (i * 25), WIDTH - 25, HEIGHT - 10 - (i * 25), {0xff, 0xff, 0xff, 0xff});
            }
            /*for (int i = 0; i <= 100; i += 25) {
                char buff[20];
                snprintf(buff, sizeof(buff), "%d", i);
                std::string buffAsStdStr = buff;
                Drawing::drawText(renderer, buffAsStdStr.c_str(), font14, WIDTH - 20, HEIGHT - 15 - i - 2, 0xff, 0xff, 0xff, ALIGN_LEFT);
                SDL_RenderDrawLine(renderer, WIDTH - 30 - frameTimeNum - 5, HEIGHT - 10 - i, WIDTH - 25, HEIGHT - 10 - i);
            }*/

            for(int i = 0; i < frameTimeNum; i++) {
                int h = frameTime[i];

                SDL_Color col;
                if(h <= (int)(1000 / 144.0)) {
                    col = {0x00, 0xff, 0x00, 0xff};
                } else if(h <= (int)(1000 / 60.0)) {
                    col = {0xa0, 0xe0, 0x00, 0xff};
                } else if(h <= (int)(1000 / 30.0)) {
                    col = {0xff, 0xff, 0x00, 0xff};
                } else if(h <= (int)(1000 / 15.0)) {
                    col = {0xff, 0x80, 0x00, 0xff};
                } else {
                    col = {0xff, 0x00, 0x00, 0xff};
                }

                GPU_Line(target, WIDTH - frameTimeNum - 30 + i, HEIGHT - 10 - h, WIDTH - frameTimeNum - 30 + i, HEIGHT - 10, col);
                //SDL_RenderDrawLine(renderer, WIDTH - frameTimeNum - 30 + i, HEIGHT - 10 - h, WIDTH - frameTimeNum - 30 + i, HEIGHT - 10);
            }

            GPU_Line(target, WIDTH - 30 - frameTimeNum - 5, HEIGHT - 10 - (int)(1000.0 / fps), WIDTH - 25, HEIGHT - 10 - (int)(1000.0 / fps), {0x00, 0xff, 0xff, 0xff});

            EASY_END_BLOCK; // draw frame graph
        }

        EASY_BLOCK("draw ui", RENDER_PROFILER_COLOR);
        GPU_SetShapeBlendMode(GPU_BLEND_NORMAL);

        for(auto& v : uis) {
            v->bounds->x = max(0, min(v->bounds->x, WIDTH - v->bounds->w));
            v->bounds->y = max(0, min(v->bounds->y, HEIGHT - v->bounds->h));
        }

        EASY_BLOCK("draw debugUI", RENDER_PROFILER_COLOR);
        debugUI->draw(target, 0, 0);
        EASY_END_BLOCK; // draw debugUI

        EASY_BLOCK("draw debugDrawUI", RENDER_PROFILER_COLOR);
        debugDrawUI->draw(target, 0, 0);
        EASY_END_BLOCK; // draw debugDrawUI

        EASY_BLOCK("draw chiselUI", RENDER_PROFILER_COLOR);
        if(chiselUI != NULL) {
            if(!chiselUI->visible) {
                uis.erase(std::remove(uis.begin(), uis.end(), chiselUI), uis.end());
                delete chiselUI;
                chiselUI = NULL;
            } else {
                chiselUI->draw(target, 0, 0);
            }
        }
        EASY_END_BLOCK; // draw mainMenuUI
        EASY_BLOCK("draw mainMenuUI", RENDER_PROFILER_COLOR);
        //if (state == MAIN_MENU) { // handled by setting mainMenuUI->visible
        mainMenuUI->draw(target, 0, 0);
        //}
        EASY_END_BLOCK;
        EASY_END_BLOCK; // draw ui

        EASY_BLOCK("draw version info", RENDER_PROFILER_COLOR);
        #ifdef DEVELOPMENT_BUILD
        if(dt_versionInfo1.w == -1) {
            char buffDevBuild[40];
            snprintf(buffDevBuild, sizeof(buffDevBuild), "Development Build");
            dt_versionInfo1 = Drawing::drawTextParams(target, buffDevBuild, font16, 4, HEIGHT - 32 - 13, 0xff, 0xff, 0xff, ALIGN_LEFT);

            char buffVersion[40];
            snprintf(buffVersion, sizeof(buffVersion), "Version %s - dev", VERSION);
            dt_versionInfo2 = Drawing::drawTextParams(target, buffVersion, font16, 4, HEIGHT - 32, 0xff, 0xff, 0xff, ALIGN_LEFT);

            char buffBuildDate[40];
            snprintf(buffBuildDate, sizeof(buffBuildDate), "%s : %s", __DATE__, __TIME__);
            dt_versionInfo3 = Drawing::drawTextParams(target, buffBuildDate, font16, 4, HEIGHT - 32 + 13, 0xff, 0xff, 0xff, ALIGN_LEFT);
        }

        Drawing::drawText(target, dt_versionInfo1, 4, HEIGHT - 32 - 13, ALIGN_LEFT);
        Drawing::drawText(target, dt_versionInfo2, 4, HEIGHT - 32, ALIGN_LEFT);
        Drawing::drawText(target, dt_versionInfo3, 4, HEIGHT - 32 + 13, ALIGN_LEFT);
        #elif defined ALPHA_BUILD
        char buffDevBuild[40];
        snprintf(buffDevBuild, sizeof(buffDevBuild), "Alpha Build");
        Drawing::drawText(target, buffDevBuild, font16, 4, HEIGHT - 32, 0xff, 0xff, 0xff, ALIGN_LEFT);

        char buffVersion[40];
        snprintf(buffVersion, sizeof(buffVersion), "Version %s - alpha", VERSION);
        Drawing::drawText(target, buffVersion, font16, 4, HEIGHT - 32 + 13, 0xff, 0xff, 0xff, ALIGN_LEFT);
        #else
        char buffVersion[40];
        snprintf(buffVersion, sizeof(buffVersion), "Version %s", VERSION);
        Drawing::drawText(target, buffVersion, font16, 4, HEIGHT - 32 + 13, 0xff, 0xff, 0xff, ALIGN_LEFT);
        #endif
        EASY_END_BLOCK; // draw version info

    }
}

void Game::renderLightmap(World* world) {
    EASY_FUNCTION(GAME_PROFILER_COLOR);

    GPU_Target* lightTarget = GPU_LoadTarget(lightMap);
    GPU_Rect r = GPU_Rect {0, 0, (float)world->tickZone.w, (float)world->tickZone.h};
    GPU_RectangleFilled2(lightTarget, r, {0, 0, 0, 0xff});

    GPU_SetBlendFunction(lightTex,
        GPU_BlendFuncEnum::GPU_FUNC_ONE,
        GPU_BlendFuncEnum::GPU_FUNC_ONE,
        GPU_BlendFuncEnum::GPU_FUNC_ONE,
        GPU_BlendFuncEnum::GPU_FUNC_ONE
    );

    GPU_SetBlendEquation(lightTex,
        GPU_BlendEqEnum::GPU_EQ_ADD,
        GPU_BlendEqEnum::GPU_EQ_ADD
    );

    for(int x = world->tickZone.x; x < world->tickZone.w + world->tickZone.x; x += 8) {
        for(int y = world->tickZone.y; y < world->tickZone.h + world->tickZone.y; y += 8) {
            if(world->tiles[x + y * world->width].mat->emit > 0) {
                GPU_Rect lr = {(float)(x - world->tickZone.x - world->tiles[x + y * world->width].mat->emit), (float)(y - world->tickZone.y - world->tiles[x + y * world->width].mat->emit), (float)(world->tiles[x + y * world->width].mat->emit * 2), (float)(world->tiles[x + y * world->width].mat->emit * 2)};
                GPU_SetColor(lightTex, {(world->tiles[x + y * world->width].mat->emitColor >> 16) & 0xff, (world->tiles[x + y * world->width].mat->emitColor >> 8) & 0xff, (world->tiles[x + y * world->width].mat->emitColor >> 0) & 0xff});
                GPU_BlitRect(lightTex, NULL, lightTarget, &lr);
            }
        }
    }

    GPU_FreeTarget(lightTarget);
}

void Game::renderTemperatureMap(World* world) {
    EASY_FUNCTION(GAME_PROFILER_COLOR);

    for(int x = 0; x < world->width; x++) {
        for(int y = 0; y < world->height; y++) {
            auto t = world->tiles[x + y * world->width];
            int32_t temp = t.temperature;
            Uint32 color = (Uint8)((temp + 1024) / 2048.0f * 255);

            const unsigned int offset = (world->width * 4 * y) + x * 4;
            pixelsTemp_ar[offset + 0] = color;        // b
            pixelsTemp_ar[offset + 1] = color;        // g
            pixelsTemp_ar[offset + 2] = color;        // r
            pixelsTemp_ar[offset + 3] = 0xf0;    // a
        }
    }

}

int Game::getAimSurface(int dist) {
    int dcx = this->mx - WIDTH / 2;
    int dcy = this->my - HEIGHT / 2;

    float len = sqrtf(dcx * dcx + dcy * dcy);
    float udx = dcx / len;
    float udy = dcy / len;

    int mmx = WIDTH / 2 + udx * dist;
    int mmy = HEIGHT / 2 + udy * dist;

    int wcx = (int)((WIDTH / 2 - ofsX - camX) / scale);
    int wcy = (int)((HEIGHT / 2 - ofsY - camY) / scale);

    int wmx = (int)((mmx - ofsX - camX) / scale);
    int wmy = (int)((mmy - ofsY - camY) / scale);

    int startInd = -1;
    world->forLine(wcx, wcy, wmx, wmy, [&](int ind) {
        if(world->tiles[ind].mat->physicsType == PhysicsType::SOLID || world->tiles[ind].mat->physicsType == PhysicsType::SAND || world->tiles[ind].mat->physicsType == PhysicsType::SOUP) {
            startInd = ind;
            return true;
        }
        return false;
    });

    return startInd;
}

string Game::getFileInGameDir(string filePathRel) {
    return this->gameDir + filePathRel;
}

string Game::getWorldDir(string worldName) {
    return this->getFileInGameDir("worlds/" + worldName);
}

int Game::getAimSolidSurface(int dist) {
    int dcx = this->mx - WIDTH / 2;
    int dcy = this->my - HEIGHT / 2;

    float len = sqrtf(dcx * dcx + dcy * dcy);
    float udx = dcx / len;
    float udy = dcy / len;

    int mmx = WIDTH / 2 + udx * dist;
    int mmy = HEIGHT / 2 + udy * dist;

    int wcx = (int)((WIDTH / 2 - ofsX - camX) / scale);
    int wcy = (int)((HEIGHT / 2 - ofsY - camY) / scale);

    int wmx = (int)((mmx - ofsX - camX) / scale);
    int wmy = (int)((mmy - ofsY - camY) / scale);

    int startInd = -1;
    world->forLine(wcx, wcy, wmx, wmy, [&](int ind) {
        if(world->tiles[ind].mat->physicsType == PhysicsType::SOLID) {
            startInd = ind;
            return true;
        }
        return false;
    });

    return startInd;
}

#if BUILD_WITH_STEAM
extern "C" void __cdecl SteamAPIDebugTextHook(int nSeverity, const char *pchDebugText) {
    ::OutputDebugString(pchDebugText);
    if(nSeverity >= 1) {
        logWarn("[STEAMAPI] {}", pchDebugText);
        // place to set a breakpoint for catching API errors
        int x = 3;
        x = x;
    } else {
        logInfo("[STEAMAPI] {}", pchDebugText);
    }
}
#endif

#if BUILD_WITH_STEAM
void Game::SteamHookMessages() {
    SteamUtils()->SetWarningMessageHook(&SteamAPIDebugTextHook);
}
#endif
