#include "game/scenes/mainmenu/menu_gameplay.h"
#include "game/scenes/mainmenu/menu_advanced.h"

#include "game/common_defines.h"
#include "game/gui/gui.h"
#include "game/utils/settings.h"

void menu_gameplay_done(component *c, void *u) {
    menu_finish(component_get_parent(c));
}

void menu_gameplay_speed_slide(component *c, void *userdata, int pos) {
    scene *sc = userdata;
    game_state_set_speed(sc->gs, pos + 5);
}

void menu_enter_advanced(component *c, void *userdata) {
    scene *s = userdata;
    menu_set_submenu(component_get_parent(c), menu_advanced_create(s));
}

// MASTER.DAT 0x6157C: the CPU difficulty can only be raised up to a cap. The cap starts at Champion (or the current
// difficulty if higher). Pressing right more than 20 times on the difficulty line raises it to Deadly; on handhelds
// (changed from the original O+M+F key combination) another 21 presses raise it to Ultimate.
static struct {
    int cap;
    int prev;
    int right_presses;
} difficulty_lock;

static void menu_gameplay_difficulty_toggle(component *c, void *userdata, int pos) {
    const int last = NUMBER_OF_AI_DIFFICULTY_TYPES - 1;
    int prev = difficulty_lock.prev;
    bool right = pos == prev + 1 || (prev == last && pos == 0);
    bool left = pos == prev - 1 || (prev == 0 && pos == last);
    if(right) {
        difficulty_lock.right_presses++;
        if(difficulty_lock.right_presses > 20 && difficulty_lock.cap < AI_DIFFICULTY_DEADLY) {
            difficulty_lock.cap = AI_DIFFICULTY_DEADLY;
        } else if(difficulty_lock.right_presses > 41 && difficulty_lock.cap < AI_DIFFICULTY_ULTIMATE) {
            difficulty_lock.cap = AI_DIFFICULTY_ULTIMATE;
        }
        if(pos > difficulty_lock.cap || (prev == last && pos == 0)) {
            pos = prev; // no wrap-around, stop at the cap
        }
    } else if(left && prev == 0) {
        pos = 0; // no wrap-around at the bottom either
    }
    if(pos != textselector_get_pos(c)) {
        textselector_set_pos(c, pos);
    }
    difficulty_lock.prev = pos;
}

component *menu_gameplay_create(scene *s) {
    int difficulty = settings_get()->gameplay.difficulty;
    difficulty_lock.cap = difficulty > AI_DIFFICULTY_CHAMPION ? difficulty : AI_DIFFICULTY_CHAMPION;
    difficulty_lock.prev = difficulty;
    difficulty_lock.right_presses = 0;

    const char *fightmode_opts[] = {"NORMAL", "HYPER"};
    const char *hazard_opts[] = {"OFF", "ON"};

    component *menu = menu_create();
    menu_attach(menu, label_create_title("GAMEPLAY"));
    menu_attach(menu,
                textslider_create_bind("SPEED", "Change the overall speed of the game. Press left and right to change.",
                                       10, false, menu_gameplay_speed_slide, s, &settings_get()->gameplay.speed));
    menu_attach(menu, textselector_create_bind_opts(
                          "FIGHT MODE",
                          "Fight mode can be either normal or hyper. Hyper mode will enhance your special moves. Check "
                          "the robot description section of help for more information.",
                          NULL, NULL, &settings_get()->gameplay.fight_mode, fightmode_opts, 2));
    menu_attach(menu, textslider_create_bind("POWER 1",
                                             "Change the power of player 1's hits and throws. This setting will take "
                                             "effect only in two player games. Press left and right to change.",
                                             7, 0, NULL, NULL, &settings_get()->gameplay.power1));
    menu_attach(menu, textslider_create_bind("POWER 2",
                                             "Change the power of player 1's hits and throws. This setting will take "
                                             "effect only in two player games. Press left and right to change.",
                                             7, 0, NULL, NULL, &settings_get()->gameplay.power2));
    menu_attach(menu, textselector_create_bind_opts("HAZARDS",
                                                    "Some arenas have dangerous environments: spikes, electrivity, "
                                                    "fighter planes, and the like. This option turns them on and off.",
                                                    NULL, NULL, &settings_get()->gameplay.hazards_on, hazard_opts, 2));
    menu_attach(menu,
                textselector_create_bind_opts("CPU:",
                                              "This determines how well the computer fights in a one player game. This "
                                              "has no effect on two player games. Press left and right to change.",
                                              menu_gameplay_difficulty_toggle, NULL, &settings_get()->gameplay.difficulty,
                                              ai_difficulty_names,
                                              NUMBER_OF_AI_DIFFICULTY_TYPES));
    const char *ai_opts[] = {"OPENOMF", "ORIGINAL"};
    menu_attach(menu, textselector_create_bind_opts("CPU AI",
                                                    "ORIGINAL follows the AI of the DOS game (MASTER.DAT), OPENOMF uses "
                                                    "the newer tactics AI. Takes effect from the next fight.",
                                                    NULL, NULL, &settings_get()->gameplay.original_ai, ai_opts, 2));
    menu_attach(menu, textselector_create_bind_opts("",
                                                    "This will set matches so they are one round, best two out of "
                                                    "three rounds, or best three out of five rounds.",
                                                    NULL, NULL, &settings_get()->gameplay.rounds, round_type_names,
                                                    NUMBER_OF_ROUND_TYPES));

    menu_attach(menu, button_create("ADVANCED OPTIONS", "Do I really have to tell you what this is?", false, false,
                                    menu_enter_advanced, s));
    menu_attach(menu, button_create("DONE", "Go back to the main menu.", false, false, menu_gameplay_done, NULL));
    return menu;
}
