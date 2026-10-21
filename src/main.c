#include "controller/game_controller_db.h"
#include "engine.h"
#include "game/game_state.h"
#include "game/utils/settings.h"
#include "game/utils/version.h"
#include "resources/resource_files.h"
#include "resources/resource_paths.h"
#include "resources/sgmanager.h"
#include "utils/allocator.h"
#include "utils/c_array_util.h"
#include "utils/c_string_util.h"
#include "utils/log.h"
#include "utils/msgbox.h"
#include "utils/random.h"

#include <SDL.h>
#include <argtable3.h>
#include <enet/enet.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef SHA1_HASH
static const char *git_sha1_hash = "";
#else
static const char *git_sha1_hash = SHA1_HASH;
#endif

void scan_game_controllers(void) {
    log_info("Found %d joysticks attached", SDL_NumJoysticks());
    SDL_Joystick *joy;
    char guid_str[33];
    for(int i = 0; i < SDL_NumJoysticks(); i++) {
        joy = SDL_JoystickOpen(i);
        if(joy) {
            SDL_JoystickGUID guid = SDL_JoystickGetGUID(joy);
            SDL_JoystickGetGUIDString(guid, guid_str, 33);
            log_info("Opened Joystick %d", i);
            log_info(" * Name:              %s", SDL_JoystickNameForIndex(i));
            log_info(" * Number of Axes:    %d", SDL_JoystickNumAxes(joy));
            log_info(" * Number of Buttons: %d", SDL_JoystickNumButtons(joy));
            log_info(" * Number of Balls:   %d", SDL_JoystickNumBalls(joy));
            log_info(" * Number of Hats:    %d", SDL_JoystickNumHats(joy));
            log_info(" * GUID          :    %s", guid_str);
        } else {
            log_info("Joystick %d is unsupported", i);
        }

        if(SDL_JoystickGetAttached(joy)) {
            SDL_JoystickClose(joy);
        }
    }
}

#ifdef __ANDROID__
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

// 앱에 넣은 게임 데이터(APK assets/omf/...)를 앱 전용 폴더로 한 번 복사하고, OpenOMF 경로 환경변수를 정한다.
// OpenOMF 는 일반 파일 경로로 리소스를 읽으므로 assets 를 그대로 쓸 수 없다.
// 복사할 파일 목록은 assets/omf/files.txt (APK 를 만들 때 생성), 버전이 바뀌면 다시 복사한다.
static void android_mkdirs(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for(char *p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

static bool android_copy_asset(const char *asset, const char *dst) {
    SDL_RWops *in = SDL_RWFromFile(asset, "rb");
    if(in == NULL) {
        return false;
    }
    char tmp[1100];
    snprintf(tmp, sizeof(tmp), "%s.tmp", dst);
    FILE *out = fopen(tmp, "wb");
    if(out == NULL) {
        SDL_RWclose(in);
        return false;
    }
    char buf[65536];
    size_t n;
    bool ok = true;
    while((n = SDL_RWread(in, buf, 1, sizeof(buf))) > 0) {
        if(fwrite(buf, 1, n, out) != n) {
            ok = false;
            break;
        }
    }
    SDL_RWclose(in);
    if(fclose(out) != 0) {
        ok = false;
    }
    return ok && rename(tmp, dst) == 0;
}

static void android_prepare_paths(void) {
    const char *base = SDL_AndroidGetInternalStoragePath();
    if(base == NULL) {
        return;
    }
    char root[1024], buf[1100];
    snprintf(root, sizeof(root), "%s/omf", base);
    android_mkdirs(root);

    // assets/omf/files.txt: 첫 줄 = 데이터 버전, 다음 줄부터 상대 경로
    SDL_RWops *list = SDL_RWFromFile("omf/files.txt", "rb");
    if(list != NULL) {
        Sint64 size = SDL_RWsize(list);
        char *text = SDL_calloc(1, (size_t)size + 1);
        SDL_RWread(list, text, 1, (size_t)size);
        SDL_RWclose(list);

        char *nl = strchr(text, '\n');
        char version[128] = "";
        if(nl != NULL) {
            snprintf(version, sizeof(version), "%.*s", (int)(nl - text), text);
        }
        char stamp_path[1100], stamp[128] = "";
        snprintf(stamp_path, sizeof(stamp_path), "%s/.assets_version", root);
        FILE *f = fopen(stamp_path, "rb");
        if(f != NULL) {
            size_t r = fread(stamp, 1, sizeof(stamp) - 1, f);
            stamp[r] = 0;
            fclose(f);
        }
        if(nl != NULL && strcmp(stamp, version) != 0) {
            bool all_ok = true;
            for(char *line = nl + 1; *line;) {
                char *end = strchr(line, '\n');
                if(end != NULL) {
                    *end = 0;
                }
                if(*line) {
                    char asset[1100], dst[1100];
                    snprintf(asset, sizeof(asset), "omf/%s", line);
                    snprintf(dst, sizeof(dst), "%s/%s", root, line);
                    char *slash = strrchr(dst, '/');
                    if(slash != NULL) {
                        *slash = 0;
                        android_mkdirs(dst);
                        *slash = '/';
                    }
                    if(!android_copy_asset(asset, dst)) {
                        all_ok = false;
                        SDL_Log("OMF: failed to copy asset %s", asset);
                    }
                }
                if(end == NULL) {
                    break;
                }
                line = end + 1;
            }
            if(all_ok && (f = fopen(stamp_path, "wb")) != NULL) {
                fputs(version, f);
                fclose(f);
            }
        }
        SDL_free(text);
    }

    snprintf(buf, sizeof(buf), "%s/", root);
    setenv("OPENOMF_RESOURCE_PATH", buf, 1);
    // 로그·세이브·설정은 사용자가 파일 관리자/PC 로 볼 수 있는 앱 외부 폴더
    // (Android/data/com.omf2097.android/files). 쓸 수 없으면 내부 폴더
    const char *user = NULL;
    if(SDL_AndroidGetExternalStorageState() & SDL_ANDROID_EXTERNAL_STORAGE_WRITE) {
        user = SDL_AndroidGetExternalStoragePath();
    }
    if(user == NULL) {
        user = base;
    }
    snprintf(buf, sizeof(buf), "%s/state/", user);
    android_mkdirs(buf);
    setenv("OPENOMF_STATE_PATH", buf, 1);
    snprintf(buf, sizeof(buf), "%s/config/", user);
    android_mkdirs(buf);
    setenv("OPENOMF_CONFIG_PATH", buf, 1);
}
#endif

int main(int argc, char *argv[]) {
#ifdef __ANDROID__
    android_prepare_paths();
#endif
    // Set up initial state for misc things
    char *ip = NULL;
    char *lobbyaddr = NULL;
    char *oldlobbyaddr = NULL;
    char *trace_file = NULL;
    int retval = 1;
    unsigned short connect_port = 0;
    unsigned short listen_port = 0;
    engine_init_flags init_flags;
    memset(&init_flags, 0, sizeof(init_flags));

    struct arg_lit *help = arg_lit0("h", "help", "print this help and exit");
    struct arg_lit *vers = arg_lit0("v", "version", "print version information and exit");
    struct arg_lit *listen = arg_lit0("l", "listen", "Start a network game server");
    struct arg_lit *lobby = arg_lit0(NULL, "lobby", "Enter network game lobby");
    struct arg_str *lobbyarg =
        arg_str0(NULL, "lobby-addr", "<lobby server>", "Use <lobby server> as the lobby address to connect to");
    struct arg_str *connect = arg_str0("c", "connect", "<host>", "Connect to a remote game");
    struct arg_str *force_audio_backend =
        arg_str0(NULL, "force-audio-backend", "<force-audio-backend>", "Force an audio backend to use");
    struct arg_str *force_renderer = arg_str0(NULL, "force-renderer", "<force-renderer>", "Force a renderer to use");
    struct arg_str *trace = arg_str0("t", "trace", "<file>", "Trace netplay events to file");
    struct arg_int *port = arg_int0("p", "port", "<port>", "Port to connect or listen (default: 2097)");
    struct arg_file *play = arg_file0("P", "play", "<file>", "Play an existing recfile");
    struct arg_file *rec = arg_file0("R", "rec", "<file>", "Record a new recfile");
    struct arg_lit *warp = arg_lit0(NULL, "warp", "run the game at warp speed");
    struct arg_int *speed = arg_int0(NULL, "speed", "<speed>", "game speed to use: 1-10");
    struct arg_str *log_level = arg_str0(NULL, "log-level", "<level>", "Log level (DEBUG, INFO, WARN, ERROR)");
    struct arg_end *end = arg_end(30);
    void *argtable[] = {help,  vers, listen, lobby, lobbyarg, connect, force_audio_backend, force_renderer,
                        trace, port, play,   rec,   warp,     speed,   log_level,           end};
    const char *progname = "openomf";

    // Make sure everything got allocated
    if(arg_nullcheck(argtable) != 0) {
        fprintf(stderr, "Error: insufficient memory\n");
        err_msgbox("Error: insufficient memory");
        return 1;
    }

    // Parse arguments
    int nerrors = arg_parse(argc, argv, argtable);

    // Handle help & version
    if(help->count > 0) {
        fprintf(stderr, "Usage: %s", progname);
        arg_print_syntax(stderr, argtable, "\n");
        fprintf(stderr, "\nArguments:\n");
        arg_print_glossary(stderr, argtable, "%-25s %s\n");
        goto exit_0;
    }
    if(vers->count > 0) {
        printf("OpenOMF v%s\n", get_version_string());
        printf("Source available at https://github.com/omf2097/ under MIT License\n");
        printf("(C) 2097 Tuomas Virtanen, Andrew Thompson, Hunter and others\n");
        goto exit_0;
    }

    // Handle errors
    if(nerrors > 0) {
        arg_print_errors(stderr, end, progname);
        fprintf(stderr, "Try '%s --help' for more information.\n", progname);
        goto exit_0;
    }

    if(lobbyarg->count > 0) {
        lobbyaddr = omf_strdup(lobbyarg->sval[0]);
    }

    // Check other flags
    if(connect->count > 0) {
        init_flags.net_mode = NET_MODE_CLIENT;
        connect_port = 2097;
        ip = omf_strdup(connect->sval[0]);
        if(port->count > 0) {
            connect_port = port->ival[0] & 0xFFFF;
        }
    } else if(listen->count > 0) {
        init_flags.net_mode = NET_MODE_SERVER;
        listen_port = 2097;
    } else if(lobby->count > 0) {
        init_flags.net_mode = NET_MODE_LOBBY;
    } else if(play->count > 0) {
        init_flags.playback = 1;
        path_from_c(&init_flags.rec_file, play->filename[0]);
    } else if(rec->count > 0) {
        init_flags.record = 1;
        path_from_c(&init_flags.rec_file, rec->filename[0]);
    }

    if(warp->count > 0) {
        init_flags.warpspeed = 1;
    } else {
        init_flags.warpspeed = 0;
    }

    if(speed->count > 0) {
        init_flags.speed = speed->ival[0];
    } else {
        init_flags.speed = -1;
    }

    if(force_renderer->count > 0) {
        strncpy_or_truncate(init_flags.force_renderer, force_renderer->sval[0], sizeof(init_flags.force_renderer));
    }
    if(force_audio_backend->count > 0) {
        strncpy_or_truncate(init_flags.force_audio_backend, force_audio_backend->sval[0],
                            sizeof(init_flags.force_audio_backend));
    }

    if(port->count > 0) {
        listen_port = port->ival[0] & 0xFFFF;
    }

    if(trace->count > 0) {
        trace_file = omf_strdup(trace->sval[0]);
    }

    // Init log
    log_init();
#if defined(USE_COLORS)
    log_set_colors(true);
#else
    log_set_colors(false);
#endif
#if defined(DEBUGMODE)
    log_add_stderr(LOG_DEBUG, true);
    log_set_level(LOG_DEBUG);
#else
    log_set_level(LOG_INFO); // In release mode, drop debugs.
#endif
    if(log_level->count > 0) {
        if(!is_log_level(log_level->sval[0])) {
            fprintf(stderr, "Invalid loging level value %s\n", log_level->sval[0]);
            goto exit_0;
        }
        log_set_level(log_level_text_to_enum(log_level->sval[0], LOG_INFO));
    }

    // Load file paths
    if(!resource_path_init()) {
        goto exit_0;
    }
    resource_path_create_dirs();

    // Initialize logfile writing now that we have the directories.
    const path log_filename = get_log_filename();
    log_add_file(path_c(&log_filename), LOG_INFO);

    // Simple header
    log_info("Starting OpenOMF v%s", get_version_string());
    if(strlen(git_sha1_hash) > 0) {
        log_info("Git SHA1 hash: %s", git_sha1_hash);
    }

    // Random seed
    rand_seed(time(NULL));

    // Init config
    const path settings_filename = get_config_filename();
    if(settings_init(path_c(&settings_filename))) {
        err_msgbox("Failed to initialize settings file");
        log_error("Failed to initialize settings file");
        goto exit_1;
    }
    settings_load();

    // Network game override stuff
    if(ip) {
        log_debug("Connect IP overridden to %s", ip);
        omf_free(settings_get()->net.net_connect_ip);
        settings_get()->net.net_connect_ip = ip;
        // Set ip to NULL here since it will be freed by the settings.
        ip = NULL;
    }
    if(lobbyaddr) {
        log_debug("Lobby address overridden to %s", lobbyaddr);
        oldlobbyaddr = settings_get()->net.net_lobby_address;
        settings_get()->net.net_lobby_address = lobbyaddr;
    }

    if(trace_file) {
        omf_free(settings_get()->net.trace_file);
        settings_get()->net.trace_file = trace_file;
        trace_file = NULL;
    }
    if(connect_port > 0 && connect_port < 0xFFFF) {
        log_debug("Connect Port overridden to %u", connect_port & 0xFFFF);
        settings_get()->net.net_connect_port = connect_port;
    }
    if(listen_port > 0 && listen_port < 0xFFFF) {
        log_debug("Listen Port overridden to %u", listen_port & 0xFFFF);
        settings_get()->net.net_listen_port_start = listen_port;
    }

    // Init SDL2
    if(SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO)) {
        err_msgbox("SDL2 Initialization failed: %s", SDL_GetError());
        goto exit_2;
    }
    SDL_version sdl_linked;
    SDL_GetVersion(&sdl_linked);
    log_info("Found SDL v%d.%d.%d", sdl_linked.major, sdl_linked.minor, sdl_linked.patch);
    log_info("Running on platform: %s", SDL_GetPlatform());

    if(SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER)) {
        err_msgbox("Joystick initialization failed: %s", SDL_GetError());
        goto exit_2;
    }

    // Just warn if haptic subsystem does not get initialized.
    if(SDL_InitSubSystem(SDL_INIT_HAPTIC)) {
        log_warn("Haptic subsystem failed to initialize -- rumble will be disabled.");
    }

    // Load game controller support
    joystick_load_external_mappings();
    scan_game_controllers();

    // Init enet
    if(enet_initialize() != 0) {
        err_msgbox("Failed to initialize enet");
        goto exit_3;
    }

    // Initialize engine
    if(engine_init(&init_flags)) {
        err_msgbox("Failed to initialize game engine: %s", log_last_error());
        goto exit_4;
    }

    // Run
    engine_run(&init_flags);
    retval = 0;

    // Close everything
    engine_close();
exit_4:
    enet_deinitialize();
exit_3:
    SDL_Quit();
exit_2:
    if(oldlobbyaddr) {
        // we probably don't want to save this override
        settings_get()->net.net_lobby_address = oldlobbyaddr;
    }
    settings_save();
    settings_free();
exit_1:
    log_info("Exit.");
    log_close();
exit_0:
    if(ip) {
        omf_free(ip);
    }
    if(lobbyaddr) {
        omf_free(lobbyaddr);
    }
    if(trace_file) {
        omf_free(trace_file);
    }
    arg_freetable(argtable, N_ELEMENTS(argtable));
    return retval;
}
