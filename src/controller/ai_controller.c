#include "controller/ai_controller.h"
#include "controller/controller.h"
#include "formats/pilot.h"
#include "game/game_state.h"
#include "game/objects/har.h"
#include "game/objects/projectile.h"
#include "game/objects/scrap.h"
#include "game/scenes/arena.h"
#include "resources/af_loader.h"
#include "resources/ids.h"
#include "utils/allocator.h"
#include "utils/c_array_util.h"
#include "utils/log.h"
#include "utils/random.h"
#include "utils/vec.h"
#include "game/utils/settings.h"

#include "utils/miscmath.h"

bool is_move_chain_allowed(object *obj, af_move *move);
struct ai_t;
#include <math.h>

/* times thrown before we AI learns its lesson */
#define MAX_TIMES_THROWN 3
/* times shot before we AI learns its lesson */
#define MAX_TIMES_SHOT 4
/* base likelihood to change movement action (lower is more likely) */
#define BASE_ACT_CHANCE 5
/* base timer before we can consider changing movement action */
#define BASE_ACT_TIMER 28
/* base likelihood to jump while moving forwards (lower is more likely) */
#define BASE_FWD_JUMP_CHANCE 5
/* base likelihood to jump while moving backwards (lower is more likely) */
#define BASE_BACK_JUMP_CHANCE 5
/* base likelihood to jump while standing still (lower is more likely) */
#define BASE_STILL_JUMP_CHANCE 40
/* number of move ticks before bailing on tactic */
#define TACTIC_MOVE_TIMER_MAX 30
/* number of attack attempt ticks before bailing on tactic */
#define TACTIC_ATTACK_TIMER_MAX 3
/* number of jump attack attempt ticks before bailing on tactic */
#define TACTIC_JUMP_ATTACK_TIMER_MAX 12
/* likelihood of attempting a random attack/tactic (lower is more likely) */
#define RANDOM_ATTACK_CHANCE 10

#define BACK (o->direction == OBJECT_FACE_RIGHT ? ACT_LEFT : ACT_RIGHT)
#define DOWNBACK (o->direction == OBJECT_FACE_RIGHT ? ACT_LEFT : ACT_RIGHT) | ACT_DOWN
#define UPBACK (o->direction == OBJECT_FACE_RIGHT ? ACT_LEFT : ACT_RIGHT) | ACT_UP

#define FORWARD (o->direction == OBJECT_FACE_RIGHT ? ACT_RIGHT : ACT_LEFT)
#define DOWNFORWARD (o->direction == OBJECT_FACE_RIGHT ? ACT_RIGHT : ACT_LEFT) | ACT_DOWN
#define UPFORWARD (o->direction == OBJECT_FACE_RIGHT ? ACT_RIGHT : ACT_LEFT) | ACT_UP

typedef struct {
    int max_hit_dist;
    int min_hit_dist;
    int value;
    int attempts;
    int consecutive;
    int last_dist;
} move_stat;

typedef struct {
    int tactic_type;
    int last_tactic;
    int move_type;
    int move_timer;
    int attack_type;
    int attack_id;
    int attack_timer;
    int attack_on;
    int chain_hit_on;
    int chain_hit_tactic;
} tactic_state;

typedef struct {
    int difficulty;
    int act_timer;
    int cur_act;
    int input_lag; // number of ticks to wait per input
    int input_lag_timer;

    // move stats
    af_move *selected_move;
    int last_move_id;
    int move_str_pos;
    move_stat move_stats[70];
    int blocked;
    int thrown; // times thrown by enemy
    int shot;   // times shot by enemy

    // tactical state
    tactic_state *tactic;

    sd_pilot *pilot;

    // all projectiles currently on screen (vector of projectile object*)
    vector active_projectiles;

    // MASTER.DAT AI state (see ai_original_poll)
    bool orig_mode;
    int orig_learn[70];  ///< per move learnt weight (move +0x228)
    int orig_air;        ///< HAR+0x8E8
    int orig_b;          ///< HAR+0x8EC
    int orig_c;          ///< HAR+0x8F0
    int orig_advance;    ///< HAR+0x945 bit 1
    int orig_retreat;    ///< HAR+0x945 bit 2
    int orig_forget_tick;
    int orig_offense, orig_defense;
    bool orig_ready;
    const void *orig_af; ///< AF the state was built for (FIRE/ICE swap the HAR between fights)
    uint32_t orig_har_obj; ///< HAR object the state was built for (a new fight gets a new HAR object)
} ai;

static void ai_original_learn(ai *a, har_event event, int own_move);

enum
{
    TACTIC_ESCAPE = 1, // escape from enemy
    TACTIC_TURTLE,     // block attacks
    TACTIC_GRAB,       // charge and grab enemy
    TACTIC_SPAM,       // spam the same attack
    TACTIC_SHOOT,      // shoot a projectile
    TACTIC_TRIP,       // trip enemy
    TACTIC_QUICK,      // quick attack
    TACTIC_CLOSE,      // close with the enemy
    TACTIC_FLY,        // fly towards the enemy
    TACTIC_PUSH,       // spam power moves to push them back
    TACTIC_COUNTER     // block then attack
};

enum
{
    MOVE_CLOSE = 1, // close distance
    MOVE_AVOID,     // gain distance
    MOVE_JUMP,      // jump towards
    MOVE_HIGH_JUMP, // high-jump towards
    MOVE_BLOCK      // hold block
};

enum
{
    ATTACK_ID = 1, // attack by id
    ATTACK_TRIP,   // trip attack
    ATTACK_GRAB,   // grab/throw attack
    ATTACK_LIGHT,  // light/quick attack
    ATTACK_HEAVY,  // heavy/power attack
    ATTACK_JUMP,   // jumping attack
    ATTACK_RANGED, // ranged attack
    ATTACK_CHARGE, // charge attack
    ATTACK_PUSH,   // push attack
    ATTACK_RANDOM, // random attack
};

enum
{
    RANGE_CRAMPED = 0,
    RANGE_CLOSE,
    RANGE_MID,
    RANGE_FAR
};

enum
{
    MOVE_DIR_STILL,
    MOVE_DIR_FWD,
    MOVE_DIR_BACK
};

int char_to_act(str *ch, int direction, int *position) {
    int action = 0;
    switch(str_at(ch, *position)) {
        case '8':
            action = ACT_UP;
            break;
        case '2':
            action = ACT_DOWN;
            break;
        case '6':
            if(direction == OBJECT_FACE_LEFT) {
                action = ACT_LEFT;
            } else {
                action = ACT_RIGHT;
            }
            break;
        case '4':
            if(direction == OBJECT_FACE_LEFT) {
                action = ACT_RIGHT;
            } else {
                action = ACT_LEFT;
            }
            break;
        case '7':
            if(direction == OBJECT_FACE_LEFT) {
                action = ACT_UP | ACT_RIGHT;
            } else {
                action = ACT_UP | ACT_LEFT;
            }
            break;
        case '9':
            if(direction == OBJECT_FACE_LEFT) {
                action = ACT_UP | ACT_LEFT;
            } else {
                action = ACT_UP | ACT_RIGHT;
            }
            break;
        case '1':
            if(direction == OBJECT_FACE_LEFT) {
                action = ACT_DOWN | ACT_RIGHT;
            } else {
                action = ACT_DOWN | ACT_LEFT;
            }
            break;
        case '3':
            if(direction == OBJECT_FACE_LEFT) {
                action = ACT_DOWN | ACT_LEFT;
            } else {
                action = ACT_DOWN | ACT_RIGHT;
            }
            break;
        case 'K':
            action = ACT_KICK;
            break;
        case 'P':
            action = ACT_PUNCH;
            break;
        case '5':
            return ACT_STOP;
        default:
            break;
    }

    // it's possible there's a kick/punch, or both, chained on here, so check the next 2 characters
    for(int i = 0; i < 2 && (*position) > 0; i++) {
        // this is a lookahead, so use - 1 to look at the previous character in the string (move strings are in reverse
        // order)
        switch(str_at(ch, (*position) - 1)) {
            case 'K':
                log_debug("adding in extra kick to %d at string %s position %d", action, str_c(ch), *position - 1);
                action |= ACT_KICK;
                // decrement our string position!
                (*position)--;
                break;
            case 'P':
                log_debug("adding in extra punch to %d at string %s position %d", action, str_c(ch), *position - 1);
                action |= ACT_PUNCH;
                // decrement our string position!
                (*position)--;
                break;
            default:
                // don't keep going
                return action;
        }
    }

    return action;
}

/**
 * \brief Chain an array of controller commands in sequence.
 *
 * \param ctrl Controller instance.
 * \param commands An array of controller commands to chain.
 * \param n_commands Number of elements in commands array.
 * \param ev The current controller event.
 *
 * \return Void.
 */
void chain_controller_cmd(controller *ctrl, int commands[], size_t n_commands, ctrl_event **ev) {
    for(size_t i = 0; i < n_commands; i++) {
        controller_cmd(ctrl, commands[i], ev);
    }
}

/**
 * \brief Convenience method to roll '1 in x' chance.
 *
 * \param roll_x An integer indicating number of numbers in roll.
 *
 * \return A boolean indicating whether the roll passed.
 */
bool roll_chance(int roll_x) {
    return roll_x <= 1 ? true : rand_int(roll_x) == 1;
}

/**
 * \brief Roll chance for pilot preference.
 *
 * \param pref_val The value of the pilot preference (-100 to 100)
 *
 * \return A boolean indicating whether the preference is confirmed.
 */
bool roll_pref(int pref_val) {
    int rand_roll = rand_int(200);
    int pref_thresh = pref_val + 100;
    return rand_roll <= pref_thresh;
}

/**
 * \brief Determine whether the AI is smart enough to usually go ahead with an action.
 *
 * \param a The AI instance.
 *
 * \return A boolean indicating whether the AI is smart enough.
 */
bool smart_usually(const ai *a) {
    if(a->difficulty >= 6) {
        // at highest difficulty 92% chance to be smart
        return !roll_chance(12);
    } else if(a->difficulty >= 3) {
        return roll_chance(7 - a->difficulty);
    } else {
        return false;
    }
}

/**
 * \brief Determine whether the AI is dumb enough to usually go ahead with an action.
 *
 * \param a The AI instance.
 *
 * \return A boolean indicating whether the AI is dumb enough.
 */
bool dumb_usually(const ai *a) {
    if(a->difficulty == 1) {
        // at lowest difficulty 92% chance to be dumb
        return !roll_chance(12);
    }
    if(a->difficulty <= 2) {
        return roll_chance(a->difficulty + 1);
    } else {
        return false;
    }
}

/**
 * \brief Determine whether the AI is smart enough to sometimes go ahead with an action.
 *
 * \param a The AI instance.
 *
 * \return A boolean indicating whether the AI is smart enough.
 */
bool smart_sometimes(const ai *a) {
    if(a->difficulty >= 2) {
        return roll_chance(10 - a->difficulty);
    } else {
        return false;
    }
}

/**
 * \brief Determine whether the AI is dumb enough to sometimes go ahead with an action.
 *
 * \param a The AI instance.
 *
 * \return A boolean indicating whether the AI is dumb enough.
 */
bool dumb_sometimes(const ai *a) {
    if(a->difficulty <= 2) {
        return roll_chance(a->difficulty + 2);
    } else {
        return false;
    }
}

/**
 * \brief Determine whether AI will proceed with an action using exponentially scaling roll.
 *
 * \param a The AI instance.
 *
 * \return A boolean indicating whether the AI should proceed with an action.
 */
bool diff_scale(const ai *a) {
    int roll = rand_int(36);
    return roll <= (a->difficulty * a->difficulty);
}

/**
 * \brief Determine whether AI will learn from this moment.
 *
 * \param a The AI instance.
 *
 * \return A boolean indicating whether the AI should learn.
 */
bool learning_moment(const ai *a) {
    float roll = (float)rand_int(diff_scale(a) ? 8 : 15);
    return roll <= a->pilot->learning;
}

/**
 * \brief Determine whether AI will forget something.
 *
 * \param a The AI instance.
 *
 * \return A boolean indicating whether the AI should forget.
 */
bool forgetful(const ai *a) {
    float roll = (float)rand_int(diff_scale(a) ? 3 : 2);
    return roll <= a->pilot->forget;
}

/**
 * \brief Determine the current range classification.
 *
 * \param ctrl The AI controller instance.
 *
 * \return A boolean indicating range classification.
 */
int get_enemy_range(const controller *ctrl) {
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);
    object *o_enemy =
        game_state_find_object(ctrl->gs, game_state_get_player(ctrl->gs, h->player_id == 1 ? 0 : 1)->har_obj_id);

    int range_units = fabsf(o_enemy->pos.x - o->pos.x) / 30;
    switch(range_units) {
        case 0:
        case 1:
            return RANGE_CRAMPED;
        case 2:
            return RANGE_CLOSE;
        case 3:
        case 4:
            return RANGE_MID;
        default:
            return RANGE_FAR;
    }
}

bool enemy_is_stunned_or_stasis(const controller *ctrl) {
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);
    object *o_enemy =
        game_state_find_object(ctrl->gs, game_state_get_player(ctrl->gs, h->player_id == 1 ? 0 : 1)->har_obj_id);

    har *h_enemy = object_get_userdata(o_enemy);

    return h_enemy->state == STATE_STUNNED || h_enemy->in_stasis_ticks;
}

/**
 * \brief Convenience method to check whether the provided move is a special move.
 *
 * \param move The move instance.
 *
 * \return A boolean indicating whether the move is a special move.
 */
bool is_special_move(const af_move *move) {
    if(str_equal_c(&move->move_string, "K") || str_equal_c(&move->move_string, "K1") ||
       str_equal_c(&move->move_string, "K2") || str_equal_c(&move->move_string, "K3") ||
       str_equal_c(&move->move_string, "K4") || str_equal_c(&move->move_string, "K6") ||
       str_equal_c(&move->move_string, "P") || str_equal_c(&move->move_string, "P1") ||
       str_equal_c(&move->move_string, "P2") || str_equal_c(&move->move_string, "P3") ||
       str_equal_c(&move->move_string, "P4") || str_equal_c(&move->move_string, "P6")) {
        return false;
    }
    return true;
}

/**
 * \brief Convenience method to check whether a HAR has projectiles.
 *
 * \param har_id An integer identifying the HAR.
 *
 * \return Boolean indicating whether HAR supports projectiles.
 */
bool har_has_projectiles(int har_id) {
    switch(har_id) {
        case HAR_JAGUAR:
        case HAR_SHADOW:
        case HAR_ELECTRA:
        case HAR_SHREDDER:
        case HAR_CHRONOS:
        case HAR_NOVA:
            return true;
    }

    return false;
}

/**
 * \brief Convenience method to check whether a HAR has a charge attack.
 *
 * \param har_id An integer identifying the HAR.
 *
 * \return Boolean indicating whether HAR has a charge attack.
 */
bool har_has_charge(int har_id) {
    switch(har_id) {
        case HAR_JAGUAR:
        case HAR_SHADOW:
        case HAR_KATANA:
        case HAR_FLAIL:
        case HAR_THORN:
        case HAR_PYROS:
        case HAR_ELECTRA:
        case HAR_SHREDDER:
        case HAR_CHRONOS:
        case HAR_GARGOYLE:
            return true;
    }
    return false;
}

/**
 * \brief Convenience method to check whether a HAR has a push attack.
 *
 * \param har_id An integer identifying the HAR.
 *
 * \return Boolean indicating whether HAR has a push attack.
 */
bool har_has_push(int har_id) {
    switch(har_id) {
        case HAR_JAGUAR:
        case HAR_KATANA:
        case HAR_FLAIL:
        case HAR_THORN:
        case HAR_PYROS:
        case HAR_ELECTRA:
        case HAR_NOVA:
            return true;
    }
    return false;
}

/**
 * \brief Determine whether the AI would like to use the specified tactic.
 *
 * \param ctrl Controller instance.
 * \param tactic_type An integer identifying the tactic.
 *
 * \return Boolean indicating whether the AI would like to use the tactic..
 */
bool likes_tactic(const controller *ctrl, int tactic_type) {
    ai *a = ctrl->data;

    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);
    sd_pilot *pilot = a->pilot;

    if((a->tactic->last_tactic == tactic_type && roll_chance(2)) || h->state == STATE_JUMPING) {
        return false;
    }

    bool enemy_close = h->close;
    int enemy_range = get_enemy_range(ctrl);
    bool wall_close = h->is_wallhugging;

    switch(tactic_type) {
        case TACTIC_SHOOT:
            if(har_has_projectiles(h->id) && roll_pref(pilot->att_sniper) && enemy_range > RANGE_CRAMPED &&
               (h->id != HAR_SHREDDER || ((enemy_range <= RANGE_MID && smart_usually(a)) ||
                                          dumb_sometimes(a)) // shredder prefers to be close-mid range
                )) {
                return true;
            }
            break;
        case TACTIC_CLOSE:
            if(enemy_range > RANGE_CRAMPED && (har_has_charge(h->id) || roll_chance(4)) &&
               roll_pref(pilot->att_hyper)) {
                return true;
            }
            break;
        case TACTIC_QUICK:
            if(enemy_range > RANGE_CRAMPED && enemy_range < RANGE_FAR &&
               ((roll_pref(pilot->att_sniper) && roll_chance(3)) || (roll_pref(pilot->att_hyper) && roll_chance(6)) ||
                (roll_pref(pilot->att_normal) && roll_chance(8)))) {
                return true;
            }
            break;
        case TACTIC_GRAB:
            if((a->thrown <= MAX_TIMES_THROWN || roll_chance(2)) &&
               ((roll_pref(pilot->att_hyper) && roll_chance(3)) ||
                ((h->id == HAR_FLAIL || h->id == HAR_THORN) && roll_chance(3)))) {
                return true;
            }
            break;
        case TACTIC_TURTLE:
            if(a->thrown <= MAX_TIMES_THROWN && ((roll_pref(pilot->att_def) && roll_chance(3)))) {
                return true;
            }
            break;
        case TACTIC_COUNTER:
            if(a->thrown < MAX_TIMES_THROWN && roll_pref(pilot->att_def) && roll_chance(3)) {
                return true;
            }
            break;
        case TACTIC_ESCAPE:
            if((roll_pref(pilot->att_jump) && roll_chance(3)) || (roll_pref(pilot->att_def) && roll_chance(5))) {
                return true;
            }
            break;
        case TACTIC_FLY:
            if((roll_pref(a->pilot->att_jump) || (a->shot > MAX_TIMES_SHOT && learning_moment(a)) ||
                (h->id == HAR_GARGOYLE || h->id == HAR_PYROS)) &&
               ((wall_close && roll_chance(2)) || roll_chance(4))) {
                return true;
            }
            break;
        case TACTIC_PUSH:
            if((enemy_range <= RANGE_CLOSE ||
                ((h->id == HAR_THORN || h->id == HAR_KATANA) && enemy_range <= RANGE_MID)) &&
               ((har_has_push(h->id) && smart_usually(a)) &&
                ((roll_pref(pilot->att_hyper) && roll_chance(2)) || (roll_pref(pilot->att_def) && roll_chance(4)) ||
                 (wall_close && roll_chance(5))))) {
                return true;
            }
            break;
        case TACTIC_TRIP:
            if(enemy_range <= RANGE_MID &&
               ((roll_pref(pilot->att_def) && roll_chance(4)) || (roll_pref(pilot->att_sniper) && roll_chance(6)))) {
                return true;
            }
            break;
        case TACTIC_SPAM:
            if((enemy_close || dumb_usually(a)) && (wall_close || roll_chance(6)) && roll_pref(pilot->att_normal)) {
                return true;
            }
            break;
    }

    return false;
}

/**
 * \brief Queue the specified tactic in AI tactical state object.
 *
 * \param ctrl Controller instance.
 * \param tactic_type An integer identifying the tactic.
 *
 * \return Void.
 */
void queue_tactic(controller *ctrl, int tactic_type) {
    ai *a = ctrl->data;
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);

    a->tactic->last_tactic = a->tactic->tactic_type > 0 ? a->tactic->tactic_type : 0;
    a->tactic->tactic_type = tactic_type;

    // log when we queue a tactic
    switch(tactic_type) {
        case TACTIC_GRAB:
            log_debug("HAR %d queued tactic: GRAB", h->id);
            break;
        case TACTIC_TRIP:
            log_debug("HAR %d queued tactic: TRIP", h->id);
            break;
        case TACTIC_QUICK:
            log_debug("HAR %d queued tactic: QUICK", h->id);
            break;
        case TACTIC_CLOSE:
            log_debug("HAR %d queued tactic: CLOSE", h->id);
            break;
        case TACTIC_FLY:
            log_debug("HAR %d queued tactic: FLY", h->id);
            break;
        case TACTIC_SHOOT:
            log_debug("HAR %d queued tactic: SHOOT", h->id);
            break;
        case TACTIC_PUSH:
            log_debug("HAR %d queued tactic: PUSH", h->id);
            break;
        case TACTIC_SPAM:
            log_debug("HAR %d queued tactic: SPAM", h->id);
            break;
        case TACTIC_ESCAPE:
            log_debug("HAR %d queued tactic: ESCAPE", h->id);
            break;
        case TACTIC_TURTLE:
            log_debug("HAR %d queued tactic: TURTLE", h->id);
            break;
        case TACTIC_COUNTER:
            log_debug("HAR %d queued tactic: COUNTER", h->id);
            break;
    }

    bool enemy_close = h->close;
    bool wall_close = h->is_wallhugging;
    int enemy_range = get_enemy_range(ctrl);

    bool do_charge = false;

    // set move tactic
    switch(tactic_type) {
        // aggressive tactics
        case TACTIC_GRAB:
        case TACTIC_TRIP:
        case TACTIC_QUICK:
        case TACTIC_CLOSE:
            if(enemy_close) {
                a->tactic->move_type = 0;
            } else if((tactic_type == TACTIC_CLOSE || (tactic_type == TACTIC_QUICK && roll_chance(3))) &&
                      smart_usually(a) && har_has_charge(h->id)) {
                // smart AI will try to use charge attacks
                a->tactic->move_type = 0;
                do_charge = true;
            } else if(smart_usually(a) && roll_pref(a->pilot->pref_jump)) {
                // smart AI that likes to jump will close via jump
                a->tactic->move_type = MOVE_JUMP;
            } else {
                a->tactic->move_type = MOVE_CLOSE;
            }
            break;
        // jumping tactics
        case TACTIC_FLY:
            a->tactic->move_type = MOVE_HIGH_JUMP;
            break;
        // ranged tactics
        case TACTIC_SHOOT:
            a->tactic->move_type = (enemy_range == RANGE_CRAMPED && !wall_close) ? MOVE_AVOID : 0;
            break;
        // stalling tactics
        case TACTIC_PUSH:
        case TACTIC_SPAM:
            a->tactic->move_type = 0;
            break;
        // evasive tactics
        case TACTIC_ESCAPE:
            a->tactic->move_type = wall_close ? MOVE_JUMP : MOVE_AVOID;
            break;
        // goading tactics
        case TACTIC_TURTLE:
            if(enemy_range == RANGE_CRAMPED) {
                // at this range they might grab/throw so we need to use escape tactic
                a->tactic->move_type = wall_close ? MOVE_JUMP : MOVE_AVOID;
            } else {
                a->tactic->move_type = MOVE_BLOCK;
            }
            break;
        case TACTIC_COUNTER:
            a->tactic->move_type = enemy_range > RANGE_CRAMPED ? MOVE_BLOCK : 0;
            break;
    }

    // set tactic move timer
    if(a->tactic->move_type > 0) {
        a->tactic->move_timer = TACTIC_MOVE_TIMER_MAX;
    }

    if(do_charge) {
        // set charge attack
        a->tactic->attack_type = ATTACK_CHARGE;
        a->tactic->attack_id = 0;
    } else {
        // set attack tactics
        switch(tactic_type) {
            // aggressive tactics
            case TACTIC_GRAB:
                a->tactic->attack_type = ATTACK_GRAB;
                a->tactic->attack_id = 0;
                break;
            case TACTIC_TRIP:
                a->tactic->attack_type = ATTACK_TRIP;
                a->tactic->attack_id = 0;
                // if we are jumping we wait for land to trip
                if(a->tactic->move_type == MOVE_JUMP) {
                    a->tactic->attack_on = HAR_EVENT_LAND;
                }
                break;
            case TACTIC_QUICK:
                a->tactic->attack_type = ATTACK_LIGHT;
                a->tactic->attack_id = 0;
                break;
            case TACTIC_FLY:
                // smart AI will try for a jumping attack
                a->tactic->attack_type = smart_usually(a) ? ATTACK_JUMP : 0;
                a->tactic->attack_id = 0;
                break;
            case TACTIC_SHOOT:
                a->tactic->attack_type = ATTACK_RANGED;
                a->tactic->attack_id = 0;
                break;
            case TACTIC_PUSH:
                if(har_has_push(h->id)) { // && smart_sometimes(a)
                    a->tactic->attack_type = ATTACK_PUSH;
                } else {
                    a->tactic->attack_type = ATTACK_HEAVY;
                }
                a->tactic->attack_id = 0;
                break;
            case TACTIC_SPAM:
                if(a->last_move_id > 0) {
                    a->tactic->attack_type = ATTACK_ID;
                    a->tactic->attack_id = a->last_move_id;
                } else {
                    a->tactic->attack_type = ATTACK_LIGHT;
                    a->tactic->attack_id = 0;
                }
                break;
            case TACTIC_COUNTER:
                a->tactic->attack_type = roll_chance(3) ? ATTACK_TRIP : ATTACK_HEAVY;
                // we only wait for block if they're not in range to grab/throw
                if(enemy_range > RANGE_CRAMPED) {
                    a->tactic->attack_on = HAR_EVENT_BLOCK;
                }
                break;
            case TACTIC_CLOSE:
                a->tactic->attack_type = ATTACK_RANDOM;
                a->tactic->attack_id = 0;
                break;
            case TACTIC_ESCAPE:
            case TACTIC_TURTLE:
                a->tactic->attack_type = 0;
                a->tactic->attack_id = 0;
        }
    }

    // set tactic attack timer
    if(a->tactic->attack_type > 0) {
        if(a->tactic->move_type == MOVE_JUMP || a->tactic->move_type == MOVE_HIGH_JUMP) {
            a->tactic->attack_timer = TACTIC_JUMP_ATTACK_TIMER_MAX;
        } else {
            a->tactic->attack_timer = TACTIC_ATTACK_TIMER_MAX;
        }
    }
}

/**
 * \brief Consider each of the provided tactics in order and queue one we like.
 *
 * \param ctrl Controller instance.
 * \param tactics An array of tactics to consider.
 *
 * \return Void.
 */
void chain_consider_tactics(controller *ctrl, int tactics[], size_t n_tactics) {
    for(size_t i = 0; i < n_tactics; i++) {
        if(likes_tactic(ctrl, tactics[i])) {
            queue_tactic(ctrl, tactics[i]);
            return;
        }
    }
}

void reset_tactic_state(ai *a) {
    a->tactic->last_tactic = a->tactic->tactic_type ? a->tactic->tactic_type : 0;
    a->tactic->tactic_type = 0;
    a->tactic->move_type = 0;
    a->tactic->move_timer = 0;
    a->tactic->attack_type = 0;
    a->tactic->attack_id = 0;
    a->tactic->attack_timer = 0;
    a->tactic->attack_on = 0;
    a->tactic->chain_hit_on = 0;
    a->tactic->chain_hit_tactic = 0;
}

/**
 * \brief Reset pilot personality. This is currently hard-coded.
 *
 * \param pilot The pilot details.
 *
 * \return Void.
 */
void reset_pilot_personality(sd_pilot *pilot) {
    // Values taken from MASTER.DAT 0x494BC (one player pilot table)
    switch(pilot->pilot_id) {
        case 0:
            // crystal
            pilot->att_normal = 30;
            pilot->att_hyper = 10;
            pilot->att_jump = 10;
            pilot->att_def = 40;
            pilot->att_sniper = 20;
            pilot->ap_close = 0;
            pilot->ap_throw = 100;
            pilot->ap_special = 75;
            pilot->ap_jump = -30;
            pilot->ap_high = -50;
            pilot->ap_low = -50;
            pilot->ap_middle = -50;
            pilot->pref_jump = -50;
            pilot->pref_fwd = 30;
            pilot->pref_back = 10;
            pilot->learning = 1.5f;
            pilot->forget = 0.25f;
            break;
        case 1:
            // steffan
            pilot->att_normal = 40;
            pilot->att_hyper = 60;
            pilot->att_jump = 30;
            pilot->att_def = 0;
            pilot->att_sniper = 0;
            pilot->ap_close = 0;
            pilot->ap_throw = 25;
            pilot->ap_special = 20;
            pilot->ap_jump = 0;
            pilot->ap_high = -75;
            pilot->ap_low = 75;
            pilot->ap_middle = 50;
            pilot->pref_jump = 30;
            pilot->pref_fwd = 20;
            pilot->pref_back = -9;
            pilot->learning = 1.0f;
            pilot->forget = 0.4f;
            break;
        case 2:
            // milano
            pilot->att_normal = 20;
            pilot->att_hyper = 30;
            pilot->att_jump = 40;
            pilot->att_def = 0;
            pilot->att_sniper = 0;
            pilot->ap_close = 0;
            pilot->ap_throw = -50;
            pilot->ap_special = -50;
            pilot->ap_jump = -50;
            pilot->ap_high = 50;
            pilot->ap_low = 50;
            pilot->ap_middle = 50;
            pilot->pref_jump = 40;
            pilot->pref_fwd = 30;
            pilot->pref_back = -3;
            pilot->learning = 0.9f;
            pilot->forget = 0.1f;
            break;
        case 3:
            // christian
            pilot->att_normal = 20;
            pilot->att_hyper = 15;
            pilot->att_jump = 0;
            pilot->att_def = 30;
            pilot->att_sniper = 10;
            pilot->ap_close = 0;
            pilot->ap_throw = 35;
            pilot->ap_special = 25;
            pilot->ap_jump = 30;
            pilot->ap_high = 0;
            pilot->ap_low = -25;
            pilot->ap_middle = 20;
            pilot->pref_jump = -10;
            pilot->pref_fwd = 10;
            pilot->pref_back = 10;
            pilot->learning = 2.4f;
            pilot->forget = 0.35f;
            break;
        case 4:
            // shirro
            pilot->att_normal = 15;
            pilot->att_hyper = 5;
            pilot->att_jump = 5;
            pilot->att_def = 20;
            pilot->att_sniper = 4;
            pilot->ap_close = 0;
            pilot->ap_throw = 75;
            pilot->ap_special = 50;
            pilot->ap_jump = -50;
            pilot->ap_high = -50;
            pilot->ap_low = -50;
            pilot->ap_middle = -50;
            pilot->pref_jump = -100;
            pilot->pref_fwd = 10;
            pilot->pref_back = 10;
            pilot->learning = 2.0f;
            pilot->forget = 0.2f;
            break;
        case 5:
            // jean-paul
            pilot->att_normal = 20;
            pilot->att_hyper = 10;
            pilot->att_jump = 20;
            pilot->att_def = 30;
            pilot->att_sniper = 45;
            pilot->ap_close = 0;
            pilot->ap_throw = -50;
            pilot->ap_special = 75;
            pilot->ap_jump = 100;
            pilot->ap_high = -50;
            pilot->ap_low = 100;
            pilot->ap_middle = -50;
            pilot->pref_jump = 0;
            pilot->pref_fwd = 10;
            pilot->pref_back = 0;
            pilot->learning = 1.2f;
            pilot->forget = 0.07f;
            break;
        case 6:
            // ibrahim
            pilot->att_normal = 40;
            pilot->att_hyper = 5;
            pilot->att_jump = 5;
            pilot->att_def = 50;
            pilot->att_sniper = 7;
            pilot->ap_close = 0;
            pilot->ap_throw = 0;
            pilot->ap_special = 50;
            pilot->ap_jump = -50;
            pilot->ap_high = 50;
            pilot->ap_low = 50;
            pilot->ap_middle = 50;
            pilot->pref_jump = 10;
            pilot->pref_fwd = 10;
            pilot->pref_back = -10;
            pilot->learning = 2.5f;
            pilot->forget = 0.05f;
            break;
        case 7:
            // angel
            pilot->att_normal = 50;
            pilot->att_hyper = 5;
            pilot->att_jump = 50;
            pilot->att_def = 10;
            pilot->att_sniper = 30;
            pilot->ap_close = 0;
            pilot->ap_throw = -75;
            pilot->ap_special = 25;
            pilot->ap_jump = 75;
            pilot->ap_high = 50;
            pilot->ap_low = 50;
            pilot->ap_middle = 50;
            pilot->pref_jump = 100;
            pilot->pref_fwd = 30;
            pilot->pref_back = 5;
            pilot->learning = 1.1f;
            pilot->forget = 0.4f;
            break;
        case 8:
            // cossette
            pilot->att_normal = 50;
            pilot->att_hyper = 5;
            pilot->att_jump = 5;
            pilot->att_def = 5;
            pilot->att_sniper = 5;
            pilot->ap_close = 0;
            pilot->ap_throw = 25;
            pilot->ap_special = -50;
            pilot->ap_jump = 50;
            pilot->ap_high = -25;
            pilot->ap_low = -50;
            pilot->ap_middle = -25;
            pilot->pref_jump = -50;
            pilot->pref_fwd = 0;
            pilot->pref_back = 10;
            pilot->learning = 0.7f;
            pilot->forget = 0.2f;
            break;
        case 9:
            // raven
            pilot->att_normal = 30;
            pilot->att_hyper = 40;
            pilot->att_jump = 0;
            pilot->att_def = 0;
            pilot->att_sniper = 0;
            pilot->ap_close = 0;
            pilot->ap_throw = 100;
            pilot->ap_special = 100;
            pilot->ap_jump = 50;
            pilot->ap_high = 50;
            pilot->ap_low = 50;
            pilot->ap_middle = 50;
            pilot->pref_jump = 35;
            pilot->pref_fwd = 30;
            pilot->pref_back = -7;
            pilot->learning = 3.0f;
            pilot->forget = 0.5f;
            break;
        case 10:
            // kreissack
            pilot->att_normal = 40;
            pilot->att_hyper = 60;
            pilot->att_jump = 30;
            pilot->att_def = 55;
            pilot->att_sniper = 90;
            pilot->ap_close = 0;
            pilot->ap_throw = -6;
            pilot->ap_special = -6;
            pilot->ap_jump = 50;
            pilot->ap_high = 50;
            pilot->ap_low = 50;
            pilot->ap_middle = 50;
            pilot->pref_jump = 50;
            pilot->pref_fwd = 70;
            pilot->pref_back = -50;
            pilot->learning = 4.0f;
            pilot->forget = 0.1f;
            break;
    }
}

/**
 * \brief Reset the base movement act timer.
 *
 * \param a The AI instance.
 *
 * \return Void.
 */
void reset_act_timer(ai *a) {
    a->act_timer = BASE_ACT_TIMER - (a->difficulty * 2) - rand_int(3);
}

/**
 * \brief Determine whether a pilot dislikes a move. Used for random attacks.
 *
 * \param a The AI instance.
 * \param selected_move The move instance.
 *
 * \return A boolean indicating whether move was disliked.
 */
bool dislikes_move(const ai *a, const af_move *move) {
    // check for non-projectile special moves
    if(is_special_move(move)) {
        // pilots with bad special ability dislike special moves
        return !roll_pref(a->pilot->ap_special);
    }

    switch(move->category) {
        case CAT_BASIC:
            // smart AI dislike basic moves
            return !roll_pref(a->pilot->att_normal) && smart_usually(a);
        case CAT_LOW:
            // pilots with bad low ability dislike low moves
            return !roll_pref(a->pilot->att_normal) && !roll_pref(a->pilot->ap_low);
        case CAT_MEDIUM:
            // pilots with bad middle ability dislike middle moves
            return !roll_pref(a->pilot->att_normal) && !roll_pref(a->pilot->ap_middle);
        case CAT_HIGH:
            // pilots with bad high ability dislike high moves
            return !roll_pref(a->pilot->att_normal) && !roll_pref(a->pilot->ap_high);
        case CAT_CLOSE:
            // non-hyper pilots with bad throw ability dislike throw moves
            return !roll_pref(a->pilot->att_hyper) && !roll_pref(a->pilot->ap_throw);
        case CAT_JUMPING:
            // non-jumper pilots with bad jump ability dislike jump moves
            return !roll_pref(a->pilot->att_jump) && !roll_pref(a->pilot->ap_jump);
        case CAT_PROJECTILE:
            // non-sniper pilots with bad special ability dislike projectile moves
            return !roll_pref(a->pilot->att_sniper) && !roll_pref(a->pilot->ap_special);
    }

    return false;
}

/**
 * \brief Determine whether a move is too powerful for AI difficutly.
 *
 * \param a The AI instance.
 * \param selected_move The move instance.
 *
 * \return A boolean indicating whether move is considered too powerful.
 */
bool move_too_powerful(const ai *a, const af_move *move) {
    return is_special_move(move) && dumb_usually(a);
}

int ai_har_event(controller *ctrl, har_event event) {
    ai *a = ctrl->data;
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);
    sd_pilot *pilot = a->pilot;
    move_stat *ms;

    if(a->orig_mode) {
        // the MASTER.DAT AI keeps no tactic bookkeeping; only its learning reacts to hits
        int own_move = (o && h && h->executing_move && o->cur_animation) ? o->cur_animation->id : -1;
        ai_original_learn(a, event, own_move);
        return 0;
    }

    bool has_queued_tactic = a->tactic->tactic_type > 0;

    if(has_queued_tactic) {
        switch(event.type) {
            case HAR_EVENT_BLOCK:
            case HAR_EVENT_BLOCK_PROJECTILE:
                if(a->tactic->tactic_type != TACTIC_COUNTER && a->tactic->tactic_type != TACTIC_TURTLE &&
                   a->tactic->tactic_type != TACTIC_TRIP && a->tactic->tactic_type != TACTIC_PUSH &&
                   a->tactic->tactic_type != TACTIC_SPAM && a->tactic->tactic_type != TACTIC_FLY &&
                   (a->tactic->tactic_type != TACTIC_GRAB || roll_chance(2)) &&
                   (a->tactic->chain_hit_on == 0 || a->tactic->chain_hit_on != event.move->category)) {
                    reset_tactic_state(a);
                    has_queued_tactic = false;
                    log_debug("\033[90mReset tactic queue: EVENT_BLOCK");
                }
                break;
            case HAR_EVENT_TAKE_HIT:
                if(a->tactic->tactic_type == TACTIC_CLOSE || a->tactic->tactic_type == TACTIC_FLY ||
                   a->tactic->tactic_type == TACTIC_COUNTER ||
                   (a->tactic->tactic_type == TACTIC_TURTLE && !pilot->att_def)) {
                    reset_tactic_state(a);
                    has_queued_tactic = false;
                    log_debug("\033[90mReset tactic queue: EVENT_TAKE_HIT");
                }
                break;
            case HAR_EVENT_ENEMY_STUN: {
                if(a->tactic->tactic_type == TACTIC_GRAB || a->tactic->tactic_type == TACTIC_CLOSE ||
                   a->tactic->tactic_type == TACTIC_TRIP) {
                    log_debug("Extend tactic move timer to capitalize on stun");
                    a->tactic->move_timer = TACTIC_MOVE_TIMER_MAX;
                } else if(a->tactic->tactic_type != TACTIC_SHOOT) {
                    reset_tactic_state(a);
                    has_queued_tactic = false;
                    log_debug("\033[90mReset tactic queue: EVENT_ENEMY_STUN");
                }
            } break;
        }
    }

    switch(event.type) {
        case HAR_EVENT_ATTACK:
        case HAR_EVENT_ENEMY_BLOCK:
        case HAR_EVENT_ENEMY_BLOCK_PROJECTILE:
        case HAR_EVENT_LAND_HIT:
        case HAR_EVENT_LAND_HIT_PROJECTILE:
            a->selected_move = NULL;
            break;
    }

    switch(event.type) {
        case HAR_EVENT_LAND_HIT:
        case HAR_EVENT_LAND_HIT_PROJECTILE: {
            ms = &a->move_stats[event.move->id];

            // in the heat of the moment they might forget what they have learnt
            if(roll_chance(2) && forgetful(a) && !pilot->photo) {
                reset_pilot_personality(pilot);
                a->blocked = 0;
                a->thrown = 0;
                a->shot = 0;
                // log_debug("HAR %d forgot their learning.", h->id);
            }

            if(ms->max_hit_dist == -1 || ms->last_dist > ms->max_hit_dist) {
                ms->max_hit_dist = ms->last_dist;
            }

            if(ms->min_hit_dist == -1 || ms->last_dist < ms->min_hit_dist) {
                ms->min_hit_dist = ms->last_dist;
            }

            ms->value++;
            if(ms->value > 10) {
                ms->value = 10;
            }

            a->last_move_id = event.move->id;

            if(a->tactic->chain_hit_on == event.move->category) {
                log_debug("Queueing chained tactic");
                queue_tactic(ctrl, a->tactic->chain_hit_tactic);
                break;
            }

            if(has_queued_tactic || !smart_usually(a)) {
                break;
            }

            if(event.type == HAR_EVENT_LAND_HIT_PROJECTILE) {
                // we hit with a projectile
                int tacs[] = {TACTIC_FLY, TACTIC_TURTLE, TACTIC_CLOSE, TACTIC_SHOOT};
                chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
            } else {
                // we hit with a HAR attack
                int tacs[] = {TACTIC_QUICK, TACTIC_TRIP,  TACTIC_GRAB,   TACTIC_PUSH,
                              TACTIC_CLOSE, TACTIC_SHOOT, TACTIC_TURTLE, TACTIC_SPAM};
                chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
            }
        } break;

        case HAR_EVENT_ENEMY_BLOCK:
        case HAR_EVENT_ENEMY_BLOCK_PROJECTILE: {
            ms = &a->move_stats[event.move->id];
            if(!a->blocked) {
                a->blocked = 1;
                ms->value--;

                a->last_move_id = event.move->id;

                if(has_queued_tactic || !smart_usually(a)) {
                    break;
                }

                if(event.type == HAR_EVENT_ENEMY_BLOCK_PROJECTILE) {
                    // enemy blocked our projectile
                    int tacs[] = {TACTIC_FLY, TACTIC_ESCAPE, TACTIC_TURTLE, TACTIC_CLOSE, TACTIC_SHOOT};
                    chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
                } else {
                    // enemy blocked our HAR attack
                    int tacs[] = {TACTIC_GRAB,   TACTIC_TRIP, TACTIC_PUSH,  TACTIC_COUNTER, TACTIC_TURTLE,
                                  TACTIC_ESCAPE, TACTIC_FLY,  TACTIC_QUICK, TACTIC_SPAM};
                    chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
                }
            }

        } break;

        case HAR_EVENT_BLOCK:
        case HAR_EVENT_BLOCK_PROJECTILE: {

            if(has_queued_tactic && a->tactic->attack_on == HAR_EVENT_BLOCK) {
                // do the attack now
                log_debug("\033[94mAttempting counter move");
                a->tactic->move_timer = 0;
                break;
            }

            if(has_queued_tactic || !smart_usually(a)) {
                break;
            }

            if(event.type == HAR_EVENT_BLOCK_PROJECTILE) {
                // count this as being shot to respond to spam quicker
                a->shot++;
                // we blocked a projectile
                int tacs[] = {TACTIC_FLY, TACTIC_SHOOT, TACTIC_CLOSE, TACTIC_TURTLE};
                chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
            } else {
                // we blocked a HAR attack
                int tacs[] = {TACTIC_TRIP,   TACTIC_PUSH,  TACTIC_TURTLE, TACTIC_GRAB,
                              TACTIC_ESCAPE, TACTIC_QUICK, TACTIC_SPAM};
                chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
            }

        } break;

        case HAR_EVENT_LAND: {

            if(has_queued_tactic && a->tactic->attack_on == HAR_EVENT_LAND && h->state == STATE_STANDING) {
                // do the attack now
                log_debug("\033[94mAttempting landing move");
                a->tactic->move_timer = 0;
                a->tactic->attack_on = 0;
                break;
            } else {
                a->act_timer = 0;

                if(!has_queued_tactic && smart_usually(a)) {
                    int tacs[] = {TACTIC_TRIP,  TACTIC_QUICK,   TACTIC_PUSH,   TACTIC_GRAB,
                                  TACTIC_SHOOT, TACTIC_COUNTER, TACTIC_TURTLE, TACTIC_CLOSE};
                    chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
                }
            }

        } break;

        case HAR_EVENT_ATTACK:
            a->tactic->move_timer = 0;
            break;

        case HAR_EVENT_HIT_WALL: {
            if(has_queued_tactic || !smart_usually(a)) {
                break;
            }

            int tacs[] = {TACTIC_SHOOT, TACTIC_PUSH,   TACTIC_TURTLE,  TACTIC_TRIP,
                          TACTIC_FLY,   TACTIC_ESCAPE, TACTIC_COUNTER, TACTIC_CLOSE};
            chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
        } break;
        case HAR_EVENT_TAKE_HIT:
        case HAR_EVENT_TAKE_HIT_PROJECTILE: {

            // if enemy is cheesing the AI will try to adjust
            if(event.move->category == CAT_CLOSE) {
                // keep track of how many times we have been thrown
                a->thrown++;
                if(learning_moment(a) && a->thrown >= MAX_TIMES_THROWN) {
                    log_debug("AI adjusting in response to repeated throws.");
                    // avoid defensive tactics
                    if(pilot->att_def > 90) {
                        pilot->att_def = 10;
                    }
                    // favor sniper tactics
                    if(pilot->att_sniper < 90) {
                        pilot->att_sniper += 10;
                    }
                    // favor jumping tactics
                    if(pilot->att_jump < 90) {
                        pilot->att_jump += 10;
                    }
                    // favor jumping movement
                    if(pilot->pref_jump < 90) {
                        pilot->pref_jump += 10;
                    }
                    // favor backwards movement
                    if(pilot->pref_back < 90) {
                        pilot->pref_back += 10;
                    }
                    if(pilot->pref_fwd > 90) {
                        pilot->pref_fwd -= 10;
                    }
                }
            } else if(event.type == HAR_EVENT_TAKE_HIT_PROJECTILE) {
                // keep track of how many times we have been shot
                a->shot++;
                if(learning_moment(a) && a->shot >= MAX_TIMES_SHOT) {
                    log_debug("AI adjusting in response to repeated projectiles.");
                    // avoid defensive tactics
                    if(pilot->att_def > 90) {
                        pilot->att_def = 10;
                    }
                    // favor shooting tactics
                    if(pilot->att_sniper < 90) {
                        pilot->att_sniper = 10;
                    }
                    // favor aggressive tactics
                    if(pilot->att_hyper < 90) {
                        pilot->att_hyper = 10;
                    }
                    // favor jumping tactics
                    if(pilot->att_jump < 20) {
                        pilot->att_jump += 20;
                    }
                    if(pilot->pref_jump < 90) {
                        pilot->pref_jump += 10;
                    }
                    // favor forwards movement
                    if(pilot->pref_fwd < 90) {
                        pilot->pref_fwd += 10;
                    }
                    if(pilot->pref_back > 90) {
                        pilot->pref_back -= 10;
                    }
                }
            }

            if(has_queued_tactic || !smart_usually(a)) {
                break;
            }

            if(event.move->category == CAT_CLOSE) {
                // distance gaining tactics
                int tacs[] = {TACTIC_ESCAPE, TACTIC_PUSH, TACTIC_FLY};
                chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
            } else if(event.type == HAR_EVENT_TAKE_HIT_PROJECTILE) {
                // aggressive tactics
                int tacs[] = {TACTIC_CLOSE, TACTIC_FLY, TACTIC_SHOOT, TACTIC_GRAB};
                chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
            } else {
                // defensive tactics
                int tacs[] = {TACTIC_COUNTER, TACTIC_TURTLE, TACTIC_ESCAPE, TACTIC_PUSH,
                              TACTIC_TRIP,    TACTIC_QUICK,  TACTIC_SPAM};
                chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
            }

        } break;
        case HAR_EVENT_RECOVER: {

            if(has_queued_tactic || !smart_usually(a)) {
                break;
            }

            int tacs[] = {TACTIC_SHOOT, TACTIC_COUNTER, TACTIC_TURTLE, TACTIC_ESCAPE};
            chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
        } break;
        case HAR_EVENT_ENEMY_HAZARD_HIT: {
            if(has_queued_tactic || !smart_usually(a)) {
                break;
            }

            log_debug("HAR capitalize on hazard: %d", h->id);

            int tacs[] = {TACTIC_GRAB, TACTIC_TRIP, TACTIC_QUICK, TACTIC_CLOSE, TACTIC_SHOOT};
            chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
        } break;
        case HAR_EVENT_ENEMY_STUN: {
            if(has_queued_tactic || !smart_usually(a)) {
                break;
            }

            log_debug("HAR capitalize on stun: %d", h->id);

            int tacs[] = {TACTIC_GRAB, TACTIC_CLOSE, TACTIC_TRIP, TACTIC_SHOOT};
            chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
        } break;
        default:
            break;
    }

    return 0;
}

void ai_controller_free(controller *ctrl) {
    ai *a = ctrl->data;
    vector_free(&a->active_projectiles);
    omf_free(a->tactic);
    omf_free(a);
}

/**
 * \brief Check whether a move is valid and can be initiated.
 *
 * \param move The move instance.
 * \param h The HAR instance.
 *
 * \return A boolean indicating whether the move is valid
 */
bool is_valid_move(const af_move *move, const har *h, bool force_allow_projectile) {
    // If category is any of these, and bot is not close, then
    // do not try to execute any of them. This attempts
    // to make the HARs close up instead of standing in place
    // wawing their hands towards each other. Not a perfect solution.
    switch(move->category) {
        case CAT_CLOSE:
        case CAT_LOW:
        case CAT_MEDIUM:
        case CAT_HIGH:
            // Only allow handwaving if close or jumping
            if(!h->close && h->state != STATE_JUMPING) {
                return false;
            }
    }
    if(move->category == CAT_JUMPING && h->state != STATE_JUMPING) {
        // not jumping but trying to execute a jumping move
        return false;
    }
    if(move->category != CAT_JUMPING && h->state == STATE_JUMPING) {
        // jumping but this move is not a jumping move
        return false;
    }
    if(move->category == CAT_SCRAP && h->state != STATE_VICTORY) {
        return false;
    }
    if(move->category == CAT_DESTRUCTION && h->state != STATE_SCRAP) {
        return false;
    }
    if(move->category == CAT_VICTORY) {
        return false;
    }

    // XXX check for chaining?

    int move_str_len = str_size(&move->move_string);
    char tmp;
    for(int i = 0; i < move_str_len; i++) {
        tmp = str_at(&move->move_string, i);
        if(!((tmp >= '1' && tmp <= '9') || tmp == 'K' || tmp == 'P')) {
            if(force_allow_projectile && move->category == CAT_PROJECTILE) {
                return true; // projectile is always true
            }
            return false;
        }
    }

    if((move->damage > 0 || move->category == CAT_PROJECTILE || move->category == CAT_SCRAP ||
        move->category == CAT_DESTRUCTION) &&
       move_str_len > 0) {
        return true;
    }

    return false;
}

/**
 * \brief Sets the selected move.
 *
 * \param ctrl Controller instance.
 * \param selected_move The move instance.
 *
 * \return Void.
 */
void set_selected_move(controller *ctrl, af_move *selected_move) {
    ai *a = ctrl->data;
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);

    a->move_stats[selected_move->id].attempts++;
    a->move_stats[selected_move->id].consecutive++;

    // do the move
    a->selected_move = selected_move;
    a->move_str_pos = str_size(&selected_move->move_string) - 1;
    object *o_enemy =
        game_state_find_object(ctrl->gs, game_state_get_player(ctrl->gs, h->player_id == 1 ? 0 : 1)->har_obj_id);
    a->move_stats[a->selected_move->id].last_dist = fabsf(o->pos.x - o_enemy->pos.x);
    a->blocked = 0;
    // log_debug("AI selected move %s", str_c(&selected_move->move_string));
}

/**
 * \brief Assigns a move by category identifier.
 *
 * \param ctrl Controller instance.
 * \param category An integer identifying the desired category of move.
 * \param highest_damage A boolean indicating whether to pick the highest damage move of category.
 *
 * \return A boolean indicating whether move was assigned.
 */
bool assign_move_by_cat(controller *ctrl, int category, bool highest_damage) {
    ai *a = ctrl->data;
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);

    af_move *selected_move = NULL;
    int top_value = 0;

    // Attack
    for(int i = 0; i < 70; i++) {
        af_move *move = NULL;
        if((move = af_get_move(h->af_data, i))) {
            // category filter
            if(category != move->category) {
                continue;
            }

            move_stat *ms = &a->move_stats[i];
            if(is_valid_move(move, h, true)) {
                int value;
                if(highest_damage) {
                    // evaluate the move based purely on damage
                    value = (int)move->damage * 10;
                } else {
                    // evaluate the move based on learning reinforcement
                    value = ms->value + rand_int(10);
                    if(learning_moment(a) && ms->min_hit_dist != -1) {
                        if(ms->last_dist < ms->max_hit_dist + 5 && ms->last_dist > ms->min_hit_dist + 5) {
                            value += 2;
                        } else if(ms->last_dist > ms->max_hit_dist + 10) {
                            value -= 3;
                        }
                    }

                    // smart AI will slightly favor high damage moves
                    if(smart_usually(a)) {
                        value += ((int)move->damage / 3);
                    }

                    value -= ms->attempts / 2;
                    value -= ms->consecutive * 2;
                }

                if(selected_move == NULL) {
                    selected_move = move;
                    top_value = value;
                } else if(value > top_value) {
                    selected_move = move;
                    top_value = value;
                }
            }
        }
    }

    if(selected_move) {
        for(int i = 0; i < 70; i++) {
            a->move_stats[i].consecutive /= 2;
        }

        set_selected_move(ctrl, selected_move);
        return true;
    }

    return false;
}

/**
 * \brief Assigns a move by move_id.
 *
 * \param ctrl Controller instance.
 * \param move_id An integer identifying the desired move.
 *
 * \return A boolean indicating whether move was assigned.
 */
bool assign_move_by_id(controller *ctrl, int move_id) {
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);

    for(int i = 0; i < 70; i++) {
        af_move *move = NULL;
        if((move = af_get_move(h->af_data, i))) {
            if(is_valid_move(move, h, true)) {
                // move_id filter
                if(move_id != move->id) {
                    continue;
                }

                // log_debug("=== assign_move_by_id === id %d", move_id);
                set_selected_move(ctrl, move);
                return true;
            }
        }
    }

    return false;
}

/**
 * \brief Make AI attempt to block attack.
 *
 * \param ctrl Controller instance.
 * \param ev The current controller event.
 *
 * \return A boolean indicating whether the attack was blocked.
 */
int ai_block_har(controller *ctrl, ctrl_event **ev) {
    ai *a = ctrl->data;
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);
    object *o_enemy =
        game_state_find_object(ctrl->gs, game_state_get_player(ctrl->gs, h->player_id == 1 ? 0 : 1)->har_obj_id);
    har *h_enemy = object_get_userdata(o_enemy);

    // XXX TODO get maximum move distance from the animation object
    if(fabsf(o_enemy->pos.x - o->pos.x) < 100 && h_enemy->executing_move && smart_usually(a)) {
        if(har_is_crouching(h_enemy)) {
            a->cur_act = DOWNBACK;
            controller_cmd(ctrl, a->cur_act, ev);
        } else {
            a->cur_act = BACK;
            controller_cmd(ctrl, a->cur_act, ev);
        }
        return 1;
    }
    return 0;
}

/**
 * \brief Make AI attempt to block projectile.
 *
 * \param ctrl Controller instance.
 * \param ev The current controller event.
 *
 * \return A boolean indicating whether the projectile was blocked.
 */
int ai_block_projectile(controller *ctrl, ctrl_event **ev) {
    ai *a = ctrl->data;
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);

    bool remember_shooting = (learning_moment(a) && a->shot >= MAX_TIMES_SHOT);

    iterator it;
    object **o_tmp;
    vector_iter_begin(&a->active_projectiles, &it);
    foreach(it, o_tmp) {
        object *o_prj = *o_tmp;
        if(projectile_get_owner(o_prj) == har_player_id(o)) {
            continue;
        }
        if(o_prj->cur_sprite_id >= 0 && (smart_usually(a) || remember_shooting)) {
            sprite *cur_sprite = animation_get_sprite(o_prj->cur_animation, o_prj->cur_sprite_id);
            if(cur_sprite) {
                vec2i pos_prj = vec2i_add(object_get_pos(o_prj), cur_sprite->pos);
                vec2i size_prj = object_get_size(o_prj);
                if(object_get_direction(o_prj) == OBJECT_FACE_LEFT) {
                    pos_prj.x = object_get_pos(o_prj).x + ((cur_sprite->pos.x * -1) - size_prj.x);
                }
                if(fabsf(pos_prj.x - o->pos.x) < 120) {
                    a->cur_act = DOWNBACK;
                    controller_cmd(ctrl, a->cur_act, ev);
                    return 1;
                }
            }
        }
    }

    return 0;
}

/**
 * \brief Process the current selected move.
 *
 * \param ctrl Controller instance.
 * \param ev The current controller event.
 *
 * \return Void.
 */
void process_selected_move(controller *ctrl, ctrl_event **ev) {
    ai *a = ctrl->data;
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);

    if(a->input_lag_timer > 0) {
        a->input_lag_timer--;
    } else {
        a->move_str_pos--;
        if(a->move_str_pos <= 0) {
            a->move_str_pos = 0;
        }
        a->input_lag_timer = a->input_lag;
    }

    controller_cmd(ctrl, char_to_act(&a->selected_move->move_string, o->direction, &a->move_str_pos), ev);

    if(a->move_str_pos == 0) {
        a->selected_move = NULL;
    }
}

/**
 * \brief Handle the AI's movement.
 *
 * \param ctrl Controller instance.
 * \param ev The current controller event.
 *
 * \return Void.
 */
void handle_movement(controller *ctrl, ctrl_event **ev) {
    ai *a = ctrl->data;
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);

    // default mid-action jump chance
    int jump_chance = 100;
    if(roll_pref(a->pilot->pref_jump)) {
        jump_chance -= 10;
    }
    if(diff_scale(a)) {
        jump_chance -= 10;
    }

    // Change action after act_timer runs out
    if(a->act_timer <= 0 && (roll_chance(BASE_ACT_CHANCE) || diff_scale(a))) {
        int enemy_range = get_enemy_range(ctrl);

        int move_dir = MOVE_DIR_STILL;
        if(!h->is_wallhugging && enemy_range == RANGE_CRAMPED) {
            // we are face-hugging already so no need to go forward
            move_dir = roll_pref(a->pilot->pref_back) ? MOVE_DIR_BACK : MOVE_DIR_STILL;
        } else if(roll_pref(a->pilot->pref_fwd)) {
            // pilot prefers forward
            move_dir = MOVE_DIR_FWD;
        } else if(!h->is_wallhugging && roll_pref(a->pilot->pref_back)) {
            // pilot prefers backward
            move_dir = MOVE_DIR_BACK;
        } else if((h->id == HAR_FLAIL || h->id == HAR_THORN || h->id == HAR_NOVA) && smart_usually(a)) {
            // brawlers are more likely to face-hug
            move_dir = MOVE_DIR_FWD;
        }

        switch(move_dir) {
            case MOVE_DIR_FWD:
                // walk forward
                a->cur_act = FORWARD;
                jump_chance = BASE_FWD_JUMP_CHANCE;
                if(diff_scale(a)) {
                    jump_chance -= 2;
                }
                break;
            case MOVE_DIR_BACK:
                // walk backward
                a->cur_act = BACK;
                jump_chance = BASE_BACK_JUMP_CHANCE;
                if(diff_scale(a)) {
                    jump_chance -= 2;
                }
                break;
            case MOVE_DIR_STILL:
            default:
                if(smart_usually(a) || roll_pref(a->pilot->att_def)) {
                    // crouch and block
                    a->cur_act = DOWNBACK;
                    jump_chance = 0;
                } else {
                    // do nothing
                    a->cur_act = ACT_STOP;
                    jump_chance = BASE_STILL_JUMP_CHANCE;
                    if(diff_scale(a)) {
                        jump_chance -= 5;
                    }
                }
                break;
        }

        reset_act_timer(a);
        controller_cmd(ctrl, a->cur_act, ev);
    }

    // Jump once in a while if they like to jump
    if(jump_chance > 0 && roll_chance(jump_chance) && roll_pref(a->pilot->pref_jump)) {
        // log_debug("Jump chance %d", jump_chance);
        if(smart_usually(a) && roll_pref(a->pilot->att_jump)) {
            // double jump
            controller_cmd(ctrl, ACT_DOWN, ev);
        }
        if(o->vel.x < 0) {
            controller_cmd(ctrl, ACT_UP | ACT_LEFT, ev);
        } else if(o->vel.x > 0) {
            controller_cmd(ctrl, ACT_UP | ACT_RIGHT, ev);
        } else {
            controller_cmd(ctrl, ACT_UP, ev);
        }
        // release the jump button
        controller_cmd(ctrl, ACT_STOP, ev);
    }
}

/**
 * \brief Attempt to select a random attack.
 *
 * \param ctrl Controller instance.
 * \param highest_damage A boolean indicating whether to pick the highest damage move that is valid.
 *
 * \return Boolean indicating whether an attack was selected.
 */
bool attempt_attack(controller *ctrl, bool highest_damage) {
    ai *a = ctrl->data;
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);

    int enemy_range = get_enemy_range(ctrl);
    bool in_attempt_range = (enemy_range <= RANGE_CLOSE || (enemy_range == RANGE_MID && dumb_sometimes(a)));

    af_move *selected_move = NULL;
    int top_value = 0;

    // Attack
    for(int i = 0; i < 70; i++) {
        af_move *move = NULL;
        if((move = af_get_move(h->af_data, i))) {
            move_stat *ms = &a->move_stats[i];
            if(is_valid_move(move, h, false)) {
                // smart AI will bail out unless close enough to hit
                if(!in_attempt_range && (move->category == CAT_BASIC || move->category == CAT_LOW ||
                                         move->category == CAT_MEDIUM || move->category == CAT_HIGH)) {
                    continue;
                }

                int value;
                if(highest_damage) {
                    // evaluate the move based purely on damage
                    value = (int)move->damage * 10;
                } else {
                    // evaluate the move based on learning reinforcement
                    value = ms->value + rand_int(10);
                    if(learning_moment(a) && ms->min_hit_dist != -1) {
                        if(ms->last_dist < ms->max_hit_dist + 5 && ms->last_dist > ms->min_hit_dist + 5) {
                            value += 2;
                        } else if(ms->last_dist > ms->max_hit_dist + 10) {
                            value -= 3;
                        }
                    }

                    // AI is less likely to use exact same move as last attack
                    if(a->last_move_id > 0 && a->last_move_id == move->id) {
                        value -= rand_int(10);
                    }

                    // smart AI will slightly favor high damage moves
                    if(smart_usually(a)) {
                        value += ((int)move->damage / 4);
                    }

                    // AI is less likely to use disliked moves
                    if(dislikes_move(a, move)) {
                        value -= rand_int(10);
                    }

                    value -= ms->attempts / 2;
                    value -= ms->consecutive * 2;

                    // sometimes skip move if it is too powerful for difficulty
                    if(move_too_powerful(a, move)) {
                        log_debug("skipping move %s because of difficulty", str_c(&move->move_string));
                        continue;
                    }
                }

                if(selected_move == NULL) {
                    selected_move = move;
                    top_value = value;
                } else if(value > top_value) {
                    selected_move = move;
                    top_value = value;
                }
            }
        }
    }

    if(selected_move) {
        for(int i = 0; i < 70; i++) {
            a->move_stats[i].consecutive /= 2;
        }

        // log_debug("Random attack %d", selected_move->id);
        set_selected_move(ctrl, selected_move);
        return true;
    }

    return false;
}

/**
 * \brief Attempt to initiate a charge atack using direct keyboard combinations.
 *
 * \param ctrl Controller instance.
 * \param ev The current controller event.
 *
 * \return Boolean indicating whether an attack was initiated.
 */
bool attempt_charge_attack(controller *ctrl, ctrl_event **ev) {
    ai *a = ctrl->data;
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);

    int enemy_range = get_enemy_range(ctrl);

    switch(h->state) {
        case STATE_WALKTO:
        case STATE_WALKFROM:
        case STATE_CROUCHBLOCK:
        case STATE_CROUCHING:
            controller_cmd(ctrl, ACT_STOP, ev);
            break;
        case STATE_STANDING:
            break;
        default:
            return false;
    }

    // log_debug("HAR attempting charge: %d", h->id);
    switch(h->id) {
        case HAR_JAGUAR: {
            // log_debug("Jaguar move: Leap");
            if(enemy_range >= RANGE_MID && roll_pref(a->pilot->ap_special) && diff_scale(a)) {
                // Shadow Leap : B,D,F+P
                int cmds[] = {BACK, DOWNBACK};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            }
            // Jaguar Leap : D,F+P
            int cmds[] = {ACT_DOWN, DOWNFORWARD, FORWARD | ACT_PUNCH};
            chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
        } break;
        case HAR_SHADOW: {
            // log_debug("Shadow move: Shadow Grab");
            // Shadow Grab       : D,D+P
            int cmds[] = {ACT_DOWN, ACT_STOP, ACT_DOWN | ACT_PUNCH};
            chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
        } break;
        case HAR_KATANA: {
            if(roll_chance(2) && roll_pref(a->pilot->ap_low)) {
                // log_debug("Katana move: Trip-slide");
                // Trip-Slide attack : D+B+K
                int cmds[] = {DOWNBACK | ACT_KICK};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            } else {
                if(enemy_range >= RANGE_MID && roll_chance(2)) {
                    // log_debug("Katana move: Foward Razor Spin");
                    // Foward Razor Spin : D,F+K
                    int cmds[] = {ACT_DOWN, DOWNFORWARD, FORWARD | ACT_KICK};
                    chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
                } else {
                    // log_debug("Katana move: Rising Blade ");
                    if(enemy_range >= RANGE_CLOSE && roll_pref(a->pilot->ap_special) && diff_scale(a)) {
                        // Triple Blade : B,D,F+P
                        int cmds[] = {BACK, DOWNBACK};
                        chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
                    }
                    // Rising Blade : D,F+P
                    int cmds[] = {ACT_DOWN, DOWNFORWARD, FORWARD | ACT_PUNCH};
                    chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
                }
            }
        } break;
        case HAR_FLAIL: {
            // log_debug("Flail move: Charging Punch");
            if(enemy_range > RANGE_MID && roll_pref(a->pilot->ap_special) && diff_scale(a)) {
                // Shadow Punch : D,B,B,P
                int cmds[] = {ACT_DOWN, DOWNBACK, BACK, ACT_STOP, BACK | ACT_PUNCH};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            } else {
                // Charging Punch : B,B,P
                int cmds[] = {BACK, ACT_STOP, BACK | ACT_PUNCH};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            }
        } break;
        case HAR_THORN: {
            // log_debug("Thorn move: Spike-charge");
            // Spike-Charge : F,F+P
            int cmds[] = {FORWARD, ACT_STOP, FORWARD | ACT_PUNCH};
            chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
        } break;
        case HAR_PYROS: {
            // log_debug("Pyros move: Thrust");
            if(enemy_range > RANGE_MID && roll_pref(a->pilot->ap_special) && diff_scale(a)) {
                // Shadow Thrust : F,F,F+P
                int cmds[] = {FORWARD, ACT_STOP};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            }
            // Super Thrust : F,F+P
            int cmds[] = {FORWARD, ACT_STOP, FORWARD | ACT_PUNCH};
            chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
        } break;
        case HAR_ELECTRA: {
            // log_debug("Electra move: Rolling Thunder");
            if(enemy_range >= RANGE_MID && roll_pref(a->pilot->ap_special) && diff_scale(a)) {
                // Super R.T. : B,D,F,F+P
                int cmds[] = {ACT_DOWN, DOWNFORWARD};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            }
            // Rolling Thunder : F,F+P
            int cmds[] = {FORWARD, ACT_STOP, FORWARD | ACT_PUNCH};
            chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
        } break;
        case HAR_CHRONOS: {
            if(enemy_range >= RANGE_MID && roll_pref(a->pilot->ap_special) && diff_scale(a)) {
                // log_debug("Chronos move: Teleport");
                // Teleportation : D,P
                int cmds[] = {ACT_DOWN, ACT_PUNCH};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
                // follow up with a close-range tactic
                int tacs[] = {TACTIC_GRAB, TACTIC_PUSH, TACTIC_SHOOT, TACTIC_SPAM, TACTIC_TRIP};
                chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
            } else {
                // log_debug("Chronos move: Trip-slide");
                // Trip-Slide attack : D,B+K
                int cmds[] = {DOWNBACK | ACT_KICK};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            }
        } break;
        case HAR_SHREDDER: {
            if(enemy_range > RANGE_MID && roll_pref(a->pilot->att_jump) && diff_scale(a)) {
                // log_debug("Shredder move: Flip-kick");
                // Flip Kick : D,D+K
                int cmds[] = {ACT_DOWN, ACT_STOP, ACT_DOWN | ACT_KICK};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            } else {
                // log_debug("Shredder move: Head-butt");
                if(enemy_range >= RANGE_MID && roll_pref(a->pilot->ap_special) && diff_scale(a)) {
                    // Shadow Head-Butt : B,D,F+P
                    int cmds[] = {BACK, DOWNBACK};
                    chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
                }
                // Head-Butt : D,F+P
                int cmds[] = {ACT_DOWN, DOWNFORWARD, FORWARD | ACT_PUNCH};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            }
        } break;
        case HAR_GARGOYLE: {
            if(enemy_range > RANGE_MID && roll_pref(a->pilot->att_jump) && diff_scale(a)) {
                // log_debug("Gargoyle move: Wing-charge");
                // Wing Charge : F,F,P
                int cmds[] = {FORWARD, ACT_STOP, FORWARD, ACT_PUNCH};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            } else {
                // log_debug("Gargoyle move: Talon");
                if(enemy_range >= RANGE_MID && roll_pref(a->pilot->ap_special) && diff_scale(a)) {
                    // Shadow Talon : B,D,F,P
                    int cmds[] = {BACK, DOWNBACK};
                    chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
                }
                // Flying Talon : D,F,P
                int cmds2[] = {ACT_DOWN, DOWNFORWARD, FORWARD, ACT_PUNCH};
                chain_controller_cmd(ctrl, cmds2, N_ELEMENTS(cmds2), ev);
            }
        } break;
        default:
            return false;
    }

    return true;
}

/**
 * \brief Attempt to initiate a push atack using direct keyboard combinations.
 *
 * \param ctrl Controller instance.
 * \param ev The current controller event.
 *
 * \return Boolean indicating whether an attack was initiated.
 */
bool attempt_push_attack(controller *ctrl, ctrl_event **ev) {
    ai *a = ctrl->data;
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);

    int enemy_range = get_enemy_range(ctrl);

    switch(h->state) {
        case STATE_WALKTO:
        case STATE_WALKFROM:
        case STATE_CROUCHBLOCK:
        case STATE_CROUCHING:
            controller_cmd(ctrl, ACT_STOP, ev);
            break;
        case STATE_STANDING:
            break;
        default:
            return false;
    }

    // log_debug("HAR attempting push: %d", h->id);
    switch(h->id) {
        case HAR_JAGUAR: {
            // log_debug("Jaguar move: High Kick");
            // High Kick : B+K
            int cmds[] = {BACK | ACT_KICK};
            chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
        } break;
        case HAR_KATANA: {
            // log_debug("Katana move: Rising Blade");
            if(enemy_range >= RANGE_CLOSE && roll_pref(a->pilot->ap_special) && diff_scale(a)) {
                // Triple Blade : B,D,F+P
                int cmds[] = {BACK, DOWNBACK};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            }
            // Rising Blade : D,F+P
            int cmds[] = {ACT_DOWN, DOWNFORWARD, FORWARD | ACT_PUNCH};
            chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
        } break;
        case HAR_FLAIL: {
            if(roll_chance(3)) {
                // log_debug("Flail move: Slow Swing Chains");
                // Slow Swing Chain : D,K
                int cmds[] = {ACT_DOWN, ACT_KICK};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            } else {
                // log_debug("Flail move: Swinging Chains");
                // Swinging Chains : D,P
                int cmds[] = {ACT_DOWN, ACT_PUNCH};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            }
        } break;
        case HAR_THORN: {
            // log_debug("Thorn move: Speed Kick");
            if(enemy_range >= RANGE_CLOSE && roll_pref(a->pilot->ap_special) && diff_scale(a)) {
                // Shadow Kick : B,D,F+K
                int cmds[] = {BACK, DOWNBACK};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            }
            // Speed Kick : D,F+K
            int cmds[] = {ACT_DOWN, DOWNFORWARD, FORWARD | ACT_KICK};
            chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
        } break;
        case HAR_PYROS: {
            // log_debug("Pyros move: Fire Spin");
            // Fire Spin : D,P
            int cmds[] = {ACT_DOWN, ACT_PUNCH};
            chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
        } break;
        case HAR_ELECTRA: {
            // log_debug("Electra move: Electric Shards");
            // Electric Shards : D,F+P
            int cmds[] = {ACT_DOWN, DOWNFORWARD, FORWARD | ACT_PUNCH};
            chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
        } break;
        case HAR_NOVA: {
            if(diff_scale(a)) {
                // log_debug("Nova move: Earthquake Slam");
                // Earthquake Slam : D,D+P
                int cmds[] = {ACT_DOWN, ACT_STOP, ACT_DOWN | ACT_PUNCH};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            } else {
                // log_debug("Nova move: Heavy Kick");
                // Heavy Kick : B+K
                int cmds[] = {BACK | ACT_KICK};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            }
        } break;
        default:
            return false;
    }

    return true;
}

/**
 * \brief Attempt to initiate a push atack using direct keyboard combinations.
 *
 * \param ctrl Controller instance.
 * \param ev The current controller event.
 *
 * \return Boolean indicating whether an attack was initiated.
 */
bool attempt_trip_attack(controller *ctrl, ctrl_event **ev) {
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);

    switch(h->state) {
        case STATE_WALKTO:
        case STATE_WALKFROM:
        case STATE_CROUCHBLOCK:
        case STATE_CROUCHING:
            controller_cmd(ctrl, ACT_STOP, ev);
            break;
        case STATE_STANDING:
            break;
        default:
            return false;
    }

    // log_debug("Har move: Trip");
    // Standard Trip : D+B+K
    int cmds[] = {DOWNBACK | ACT_KICK};
    chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);

    return true;
}

/**
 * \brief Attempt to initiate a projectile attack using direct keyboard combinations.
 *
 * \param ctrl Controller instance.
 * \param ev The current controller event.
 *
 * \return Boolean indicating whether an attack was initiated.
 */
bool attempt_projectile_attack(controller *ctrl, ctrl_event **ev) {
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);
    int enemy_range = get_enemy_range(ctrl);

    if(h->state == STATE_WALKTO || h->state == STATE_WALKFROM || h->state == STATE_CROUCHBLOCK) {
        controller_cmd(ctrl, ACT_STOP, ev);
    }

    // log_debug("HAR attempting projectile: %d", h->id);
    switch(h->id) {
        case HAR_JAGUAR:     // Concussion Cannon : D, B+P
        case HAR_ELECTRA:    // Ball Lightning : D, B+P
        case HAR_SHREDDER: { // Flying Hands : D, B+P
            if(h->id == HAR_SHREDDER && enemy_range > RANGE_MID) {
                return false;
            }
            int cmds[] = {ACT_DOWN, DOWNBACK, BACK | ACT_PUNCH};
            chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            return true;
        } break;
        case HAR_SHADOW: {
            int cmds[] = {ACT_DOWN, DOWNBACK, BACK};
            chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            if(roll_chance(2)) {
                // Shadow Punch : D,B+P
                int cmds2[] = {ACT_PUNCH};
                chain_controller_cmd(ctrl, cmds2, N_ELEMENTS(cmds2), ev);
            } else {
                // Shadow Kick : D,B+K
                int cmds2[] = {ACT_KICK};
                chain_controller_cmd(ctrl, cmds2, N_ELEMENTS(cmds2), ev);
            }
            return true;
        } break;
        case HAR_CHRONOS: {
            if(enemy_range < RANGE_MID || enemy_is_stunned_or_stasis(ctrl)) {
                // stasis does no damage, so don't use it on a stunned or already frozen enemy
                return false;
            }
            // Stasis : D, B, P
            int cmds[] = {ACT_DOWN, DOWNBACK, BACK | ACT_PUNCH};
            chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            return true;
        } break;
        case HAR_NOVA: {
            controller_cmd(ctrl, ACT_DOWN, ev);
            if(roll_chance(3) && enemy_range >= RANGE_MID) {
                // Mini-Grenade : D, B, P
                int cmds[] = {ACT_DOWN, DOWNBACK, BACK | ACT_PUNCH};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            } else {
                // Missile : D, F, P
                int cmds[] = {ACT_DOWN, DOWNFORWARD, FORWARD | ACT_PUNCH};
                chain_controller_cmd(ctrl, cmds, N_ELEMENTS(cmds), ev);
            }
            return true;
        } break;
    }

    return false;
}

/**
 * \brief Handle the next phase of the currently queued tactic.
 *
 * \param ctrl Controller instance.
 * \param ev The current controller event.
 *
 * \return Boolean indicating whether AI moved or attacked.
 */
bool handle_queued_tactic(controller *ctrl, ctrl_event **ev) {
    ai *a = ctrl->data;
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    har *h = object_get_userdata(o);
    tactic_state *tactic = a->tactic;
    int enemy_close = h->close;
    int enemy_range = get_enemy_range(ctrl);
    bool wall_close = h->is_wallhugging;

    bool acted = false;
    if(tactic->move_type > 0 && tactic->move_timer > 0) {
        acted = true;
        // handle movement phase of tactic
        switch(tactic->move_type) {
            case MOVE_CLOSE:
                if(!enemy_close) {
                    // take a step closer
                    a->cur_act = FORWARD;
                    controller_cmd(ctrl, a->cur_act, ev);
                    tactic->move_timer--;
                } else {
                    tactic->move_timer = 0;
                    // log_debug("Movement close success: %d", h->id);
                    if(tactic->attack_type == 0 && smart_usually(a)) {
                        queue_tactic(ctrl, TACTIC_GRAB);
                    }
                }
                break;
            case MOVE_AVOID:
                if(enemy_range == RANGE_FAR) {
                    tactic->move_timer = 0;
                    acted = false;
                } else {
                    if(enemy_range == RANGE_CRAMPED || !roll_pref(a->pilot->pref_jump)) {
                        // take a step away
                        a->cur_act = BACK;
                    } else {
                        if(smart_usually(a)) {
                            // do super jump
                            controller_cmd(ctrl, ACT_DOWN, ev);
                        }
                        // jump away
                        a->cur_act = UPBACK;
                    }

                    controller_cmd(ctrl, a->cur_act, ev);
                    controller_cmd(ctrl, ACT_STOP, ev);
                    tactic->move_timer--;
                }

                // if (tactic->move_timer == 0) log_debug("Movement avoid finished: %d", h->id);
                break;
            case MOVE_JUMP:
            case MOVE_HIGH_JUMP:
                if(enemy_range > RANGE_CRAMPED) {
                    if((enemy_range == RANGE_FAR && smart_usually(a)) || tactic->move_type == MOVE_HIGH_JUMP) {
                        // do high jump
                        controller_cmd(ctrl, ACT_DOWN, ev);
                    }
                    // jump closer
                    a->cur_act = UPFORWARD;
                    controller_cmd(ctrl, a->cur_act, ev);
                    controller_cmd(ctrl, ACT_STOP, ev);
                    if(roll_pref(a->pilot->pref_jump)) {
                        tactic->move_timer--;
                    } else {
                        tactic->move_timer = 0;
                    }
                } else if(tactic->tactic_type == TACTIC_FLY) {
                    if(roll_pref(a->pilot->att_jump) && smart_sometimes(a)) {
                        // do high jump
                        controller_cmd(ctrl, ACT_DOWN, ev);
                    }
                    // jump over enemy
                    a->cur_act = UPFORWARD;
                    controller_cmd(ctrl, a->cur_act, ev);
                    controller_cmd(ctrl, ACT_STOP, ev);
                    tactic->move_timer = 0;
                } else {
                    tactic->move_timer = 0;
                }

                // if (tactic->move_timer == 0) log_debug("Movement jump finished: %d", h->id);
                break;
            case MOVE_BLOCK:
                if(wall_close || har_is_crouching(h)) {
                    // crouch & block
                    a->cur_act = DOWNBACK;
                } else {
                    // retreat & block
                    a->cur_act = BACK;
                }

                controller_cmd(ctrl, a->cur_act, ev);
                tactic->move_timer--;

                // if (tactic->move_timer == 0) log_debug("Movement block finished: %d", h->id);
                break;
            default:
                // log_debug("Flushing invalid move type: %d", h->id);
                tactic->move_type = 0;
                tactic->move_timer = 0;
                acted = false;
        }
    } else if(tactic->attack_type > 0 && tactic->attack_timer > 0) {
        // handle attack phase of tactic
        bool in_attempt_range = (enemy_range <= RANGE_CLOSE || (enemy_range <= RANGE_MID && dumb_sometimes(a)));
        acted = true;
        tactic->attack_timer--;
        if(tactic->attack_on == 0) {
            int attack_cat = 0;
            switch(tactic->attack_type) {
                case ATTACK_ID: {
                    if(!in_attempt_range) {
                        break;
                    }

                    if(assign_move_by_id(ctrl, tactic->attack_id)) {
                        reset_tactic_state(a);
                        // log_debug("Specific attack success: %d", h->id);
                    }
                } break;
                case ATTACK_TRIP: {
                    if(attempt_trip_attack(ctrl, ev)) {
                        reset_tactic_state(a);

                        // chain another tactic
                        int tacs[] = {TACTIC_QUICK, TACTIC_GRAB, TACTIC_ESCAPE, TACTIC_SHOOT};
                        chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
                        if(a->tactic->tactic_type > 0) {
                            a->tactic->chain_hit_on = CAT_LOW;
                        }
                    }
                } break;
                case ATTACK_GRAB: {
                    if(!enemy_close) {
                        break;
                    }

                    if(assign_move_by_cat(ctrl, CAT_CLOSE, true)) {
                        attack_cat = CAT_CLOSE;
                    }

                    if(attack_cat > 0) {
                        reset_tactic_state(a);
                        // log_debug("Grab attack success: %d", h->id);

                        // chain another tactic
                        if(smart_sometimes(a)) {
                            if(likes_tactic(ctrl, TACTIC_PUSH)) {
                                // set chain tactic to push if attack hits
                                a->tactic->chain_hit_on = attack_cat;
                                a->tactic->chain_hit_tactic = TACTIC_PUSH;
                            } else if(likes_tactic(ctrl, TACTIC_FLY)) {
                                // set chain tactic to fly if attack hits
                                a->tactic->chain_hit_on = attack_cat;
                                a->tactic->chain_hit_tactic = TACTIC_FLY;
                            } else if(likes_tactic(ctrl, TACTIC_COUNTER)) {
                                // set chain tactic to counter if attack hits
                                a->tactic->chain_hit_on = attack_cat;
                                a->tactic->chain_hit_tactic = TACTIC_COUNTER;
                            } else if(likes_tactic(ctrl, TACTIC_SHOOT)) {
                                // set chain tactic to shoot if attack hits
                                a->tactic->chain_hit_on = attack_cat;
                                a->tactic->chain_hit_tactic = TACTIC_SHOOT;
                            }
                        }
                    }
                } break;
                case ATTACK_LIGHT: {
                    if(!in_attempt_range) {
                        break;
                    }

                    int light_cat = roll_chance(2) ? CAT_BASIC : CAT_MEDIUM;
                    if(assign_move_by_cat(ctrl, light_cat, false)) {
                        reset_tactic_state(a);
                        // log_debug("Light attack success: %d", h->id);

                        // chain another tactic
                        if(smart_sometimes(a)) {
                            if(likes_tactic(ctrl, TACTIC_PUSH)) {
                                // set chain tactic to push if attack hits
                                a->tactic->chain_hit_on = light_cat;
                                a->tactic->chain_hit_tactic = TACTIC_PUSH;
                            } else if(likes_tactic(ctrl, TACTIC_TRIP)) {
                                // set chain tactic to trip if attack hits
                                a->tactic->chain_hit_on = light_cat;
                                a->tactic->chain_hit_tactic = TACTIC_TRIP;
                            } else if(likes_tactic(ctrl, TACTIC_FLY)) {
                                // set chain tactic to fly if attack hits
                                a->tactic->chain_hit_on = light_cat;
                                a->tactic->chain_hit_tactic = TACTIC_FLY;
                            }
                        }
                    }
                } break;
                case ATTACK_HEAVY: {
                    if(!in_attempt_range) {
                        break;
                    }

                    int heavy_cat = roll_chance(2) ? CAT_MEDIUM : CAT_HIGH;
                    if(assign_move_by_cat(ctrl, heavy_cat, true)) {
                        reset_tactic_state(a);
                        // log_debug("Heavy attack success: %d", h->id);

                        // chain another tactic
                        if(smart_sometimes(a)) {
                            if(likes_tactic(ctrl, TACTIC_TRIP)) {
                                // set chain tactic to trip if attack hits
                                a->tactic->chain_hit_on = heavy_cat;
                                a->tactic->chain_hit_tactic = TACTIC_TRIP;
                            } else if(likes_tactic(ctrl, TACTIC_COUNTER)) {
                                // set chain tactic to counter if attack hits
                                a->tactic->chain_hit_on = heavy_cat;
                                a->tactic->chain_hit_tactic = TACTIC_COUNTER;
                            } else if(likes_tactic(ctrl, TACTIC_QUICK)) {
                                // set chain tactic to quick if attack hits
                                a->tactic->chain_hit_on = heavy_cat;
                                a->tactic->chain_hit_tactic = TACTIC_QUICK;
                            }
                        }
                    }
                } break;
                case ATTACK_JUMP: {
                    if(!in_attempt_range && a->tactic->attack_timer > 0) {
                        // log_debug("Waiting for jump attack range");
                        // when not in range we wait until last tick of attack timer
                        // that way the attack won't fizzle out before we reach them
                        return acted;
                    }

                    if(attempt_attack(ctrl, false)) {
                        reset_tactic_state(a);
                        // log_debug("Jump attack success: %d", h->id);

                        // chain another tactic
                        if(smart_usually(a)) {
                            if(likes_tactic(ctrl, TACTIC_TRIP)) {
                                // set chain tactic to counter if jumping attack hits
                                a->tactic->chain_hit_on = a->selected_move->category;
                                a->tactic->chain_hit_tactic = TACTIC_TRIP;
                            } else if(likes_tactic(ctrl, TACTIC_GRAB)) {
                                // set chain tactic to counter if jumping attack hits
                                a->tactic->chain_hit_on = a->selected_move->category;
                                a->tactic->chain_hit_tactic = TACTIC_GRAB;
                            } else if(likes_tactic(ctrl, TACTIC_PUSH)) {
                                // set chain tactic to counter if jumping attack hits
                                a->tactic->chain_hit_on = a->selected_move->category;
                                a->tactic->chain_hit_tactic = TACTIC_PUSH;
                            }
                        }
                    }
                } break;
                case ATTACK_RANGED: {
                    if(attempt_projectile_attack(ctrl, ev)) {
                        reset_tactic_state(a);

                        // chain another tactic
                        if(smart_sometimes(a)) {
                            if(a->pilot->att_sniper && likes_tactic(ctrl, TACTIC_SHOOT)) {
                                // set chain tactic to shoot if projectile hits
                                a->tactic->chain_hit_on = CAT_PROJECTILE;
                                a->tactic->chain_hit_tactic = TACTIC_SHOOT;
                            } else if(likes_tactic(ctrl, TACTIC_FLY)) {
                                // set chain tactic to fly if projectile hits
                                a->tactic->chain_hit_on = CAT_PROJECTILE;
                                a->tactic->chain_hit_tactic = TACTIC_FLY;
                            } else if(likes_tactic(ctrl, TACTIC_COUNTER)) {
                                // set chain tactic to counter if projectile hits
                                a->tactic->chain_hit_on = CAT_PROJECTILE;
                                a->tactic->chain_hit_tactic = TACTIC_COUNTER;
                            }
                        }
                    }
                } break;
                case ATTACK_CHARGE: {
                    if(attempt_charge_attack(ctrl, ev)) {
                        reset_tactic_state(a);

                        if(h->id == HAR_SHADOW) {
                            // shadow charge is long range
                            // use this free time to consider a new tactic
                            int tacs[] = {TACTIC_SHOOT, TACTIC_GRAB, TACTIC_FLY};
                            chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
                        }
                    }
                } break;
                case ATTACK_PUSH: {
                    if(attempt_push_attack(ctrl, ev)) {
                        reset_tactic_state(a);
                    }
                } break;
                case ATTACK_RANDOM: {
                    if(attempt_attack(ctrl, false)) {
                        reset_tactic_state(a);
                        // log_debug("Random attack success: %d", h->id);

                        // chain another tactic
                        if(smart_usually(a)) {
                            if(likes_tactic(ctrl, TACTIC_TRIP)) {
                                // set chain tactic to counter if jumping attack hits
                                a->tactic->chain_hit_on = a->selected_move->category;
                                a->tactic->chain_hit_tactic = TACTIC_TRIP;
                            } else if(likes_tactic(ctrl, TACTIC_GRAB)) {
                                // set chain tactic to counter if jumping attack hits
                                a->tactic->chain_hit_on = a->selected_move->category;
                                a->tactic->chain_hit_tactic = TACTIC_GRAB;
                            } else if(likes_tactic(ctrl, TACTIC_PUSH)) {
                                // set chain tactic to counter if jumping attack hits
                                a->tactic->chain_hit_on = a->selected_move->category;
                                a->tactic->chain_hit_tactic = TACTIC_PUSH;
                            }
                        }
                    }
                } break;
                default:
                    log_debug("Flushing invalid attack type: %d", h->id);
                    tactic->attack_type = 0;
                    tactic->attack_timer = 0;
            }
        }
    } else {
        // reset queued tactic
        reset_tactic_state(a);
        // log_debug("Flushing failed tactic queue: %d", h->id);
    }

    return acted;
}

/*
 * Computer AI modelled on MASTER.DAT (controller 0x514F0, reaction picker 0x534A0, learning 0x18DE4/0x1AAE0).
 * Probabilities are "value / 10001 per tick" like the original.
 * The original types a whole move string in one tick; here the string is fed through process_selected_move().
 */

// MASTER.DAT 0x494BC result for computer opponents (first fight), [pilot][difficulty]
static const int16_t orig_sp_offense[11][7] = {
    {1, 6, 23, 35, 72, 140, 259},    {1, 14, 39, 57, 112, 206, 373}, {1, 12, 35, 51, 102, 188, 343},
    {1, 14, 41, 59, 117, 212, 385},  {1, 13, 38, 55, 109, 200, 364}, {1, 4, 19, 29, 61, 122, 229},
    {1, 5, 20, 31, 64, 128, 238},    {1, 16, 44, 63, 124, 224, 406}, {1, 9, 29, 43, 87, 164, 301},
    {1, 15, 42, 61, 120, 218, 394},  {3, 24, 65, 93, 181, 318, 571}};
static const int16_t orig_sp_defense[11][7] = {
    {1, 13, 40, 58, 117, 214, 388},  {1, 9, 30, 46, 93, 174, 322},   {1, 10, 33, 50, 100, 186, 343},
    {1, 8, 27, 42, 85, 162, 301},    {1, 8, 27, 42, 85, 162, 301},   {1, 16, 46, 66, 132, 238, 430},
    {1, 14, 43, 62, 124, 226, 409},  {1, 9, 30, 46, 93, 174, 322},   {1, 15, 45, 65, 129, 234, 424},
    {1, 8, 29, 44, 90, 170, 313},    {2, 24, 66, 95, 186, 328, 589}};

// MASTER.DAT 0x512B0
static int orig_learn_scale(int x, int w) {
    if(w > 0) {
        return (x * 64) / (w + 64);
    }
    return (x * 64 - x * w) >> 6;
}

static bool orig_roll(int value) {
    return (int)rand_int(10001) < value;
}

// category -> pilot preference used as the resting point of the learnt weight (MASTER.DAT 0x1AAE0)
static int orig_category_pref(const sd_pilot *p, int category) {
    switch(category) {
        case CAT_CLOSE:
            return p->ap_throw;
        case 11:
            return p->ap_special;
        case CAT_JUMPING:
            return p->ap_jump;
        case CAT_LOW:
            return p->ap_high; // stored at +0xD0, used by the original for category 4
        case CAT_HIGH:
            return p->ap_low; // +0xD2
        case CAT_MEDIUM:
            return p->ap_middle; // +0xD4
        default:
            return 0;
    }
}

static void ai_original_init(controller *ctrl, ai *a, const har *h) {
    sd_pilot *p = a->pilot;
    // HAR+0x8E8/+0x8EC/+0x8F0 start at 0 and drift one step per tick towards pilot +0xD6/+0xD8/+0xDA (0x1AAE0)
    a->orig_air = 0;
    a->orig_b = 0;
    a->orig_c = 0;
    a->orig_advance = a->orig_retreat = 0;
    a->orig_forget_tick = 0;
    for(int i = 0; i < 70; i++) {
        af_move *m = af_get_move(h->af_data, i);
        a->orig_learn[i] = m ? orig_category_pref(p, m->category) : 0;
    }
    int d = a->difficulty - 1; // ai->difficulty is 1-based
    if(d < 0)
        d = 0;
    if(d > 6)
        d = 6;
    if(p->offense || p->defense) {
        // tournament pilots carry their own values
        a->orig_offense = p->offense;
        a->orig_defense = p->defense;
    } else if(p->pilot_id <= 10) {
        a->orig_offense = orig_sp_offense[p->pilot_id][d];
        a->orig_defense = orig_sp_defense[p->pilot_id][d];
    } else {
        a->orig_offense = a->orig_defense = 50;
    }
}

static bool orig_move_usable(object *o, const har *h, const af_move *m) {
    return m && is_valid_move(m, h, true) && is_move_chain_allowed(o, (af_move *)m);
}

// MASTER.DAT 0x534A0: pick a move whose ai_flags react to what the enemy is doing
static bool ai_original_react(controller *ctrl, ai *a, object *o, har *h, uint16_t mask, int k, bool urgent) {
    uint8_t order[70];
    for(int i = 0; i < 70; i++) {
        order[i] = i;
    }
    for(int i = 0; i < 70; i++) {
        int j = rand_int(70);
        uint8_t t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    // G: offense, raised as health drops (0x534A0 start)
    int g = a->orig_offense + ((h->health_max - h->health) * a->orig_offense / max2(1, h->health_max)) * 2 / 3;
    if(g < 1) {
        g = 1;
    }
    for(int n = 15; n < 70; n++) {
        af_move *m = af_get_move(h->af_data, order[n]);
        if(!m || !(m->ai_flags & mask) || !orig_move_usable(o, h, m)) {
            continue;
        }
        int w = 100;
        for(int bit = 0; bit < 16; bit++) {
            if(m->ai_flags & (1 << bit)) {
                w >>= 1;
            }
        }
        if(urgent) {
            w = 0xB4;
        }
        w += 1;
        int base = (m->category == CAT_CLOSE) ? 0x721 : 0xE42;
        int lw = orig_learn_scale(base, a->orig_learn[m->id]);
        uint32_t r = rand_int((uint32_t)max2(1, (lw + (base >> 3)) * 2));
        if((int)r <= ((w * k * g) >> 7)) {
            set_selected_move(ctrl, m);
            return true;
        }
    }
    return false;
}

static void orig_cmd_dir(controller *ctrl, object *o, int code, ctrl_event **ev) {
    // codes are relative to a right-facing HAR: 1 up, 2 up-fwd, 3 fwd, 4 down-fwd, 5 down, 6 down-back, 7 back,
    // 8 up-back (MASTER.DAT 0x533E0 mirrors them when facing left)
    bool right = object_get_direction(o) == OBJECT_FACE_RIGHT;
    int fwd = right ? ACT_RIGHT : ACT_LEFT, back = right ? ACT_LEFT : ACT_RIGHT;
    int act = ACT_STOP;
    switch(code) {
        case 1: act = ACT_UP; break;
        case 2: act = ACT_UP | fwd; break;
        case 3: act = fwd; break;
        case 4: act = ACT_DOWN | fwd; break;
        case 5: act = ACT_DOWN; break;
        case 6: act = ACT_DOWN | back; break;
        case 7: act = back; break;
        case 8: act = ACT_UP | back; break;
    }
    controller_cmd(ctrl, act, ev);
}

// MASTER.DAT 0x1AAE0: learnt weights drift back towards the pilot's preferences
static void ai_original_forget(ai *a, const har *h) {
    sd_pilot *p = a->pilot;
    for(int i = 0; i < 70; i++) {
        af_move *m = af_get_move(h->af_data, i);
        if(!m) {
            continue;
        }
        int pref = orig_category_pref(p, m->category);
        a->orig_learn[i] += (pref < a->orig_learn[i]) ? -1 : 1;
    }
}

static void ai_original_drift_prefs(ai *a) {
    sd_pilot *p = a->pilot;
    a->orig_air += (a->orig_air < p->pref_jump) ? 1 : -1; // +0x8E8 -> +0xD6
    a->orig_b += (a->orig_b < p->pref_fwd) ? 1 : -1;      // +0x8EC -> +0xD8
    a->orig_c += (a->orig_c < p->pref_back) ? 1 : -1;     // +0x8F0 -> +0xDA
}

// learning on hits (MASTER.DAT 0x18DE4:84~120)
static void ai_original_learn(ai *a, har_event event, int own_move) {
    // event.move is only valid for the hit events handled below
    if(event.type != HAR_EVENT_TAKE_HIT && event.type != HAR_EVENT_TAKE_HIT_PROJECTILE &&
       event.type != HAR_EVENT_LAND_HIT && event.type != HAR_EVENT_LAND_HIT_PROJECTILE) {
        return;
    }
    if(!event.move) {
        return;
    }
    int dmg = (int)event.move->damage;
    float learning = a->pilot->learning;
    switch(event.type) {
        case HAR_EVENT_TAKE_HIT:
        case HAR_EVENT_TAKE_HIT_PROJECTILE:
            // the move we were doing when hit becomes less attractive, so does staying in the air
            if(own_move >= 0 && own_move < 70) {
                a->orig_learn[own_move] += 2 * dmg;
            }
            a->orig_air -= (int)((dmg + 25) * learning * 0.5f);
            break;
        case HAR_EVENT_LAND_HIT:
        case HAR_EVENT_LAND_HIT_PROJECTILE:
            if(event.move->id < 70) {
                a->orig_learn[event.move->id] -= (int)((dmg + 4) * learning);
            }
            break;
        default:
            break;
    }
}

static int ai_original_poll(controller *ctrl, ctrl_event **ev, object *o, har *h) {
    ai *a = ctrl->data;
    if(!a->orig_ready || a->orig_af != (const void *)h->af_data || a->orig_har_obj != ctrl->har_obj_id) {
        ai_original_init(ctrl, a, h);
        a->orig_ready = true;
        a->orig_af = h->af_data;
        a->orig_har_obj = ctrl->har_obj_id;
    }
    object *e = game_state_find_object(ctrl->gs, o->animation_state.enemy_obj_id);
    if(!e) {
        return 0;
    }
    har *eh = object_get_userdata(e);

    if(a->selected_move) {
        process_selected_move(ctrl, ev);
        return 0;
    }

    ai_original_drift_prefs(a);
    if(++a->orig_forget_tick >= 70) {
        a->orig_forget_tick = 0;
        ai_original_forget(a, h);
    }

    // defensiveness (0x514F0)
    int D = a->orig_defense / 4 + 2;

    // distance -> stance
    float dx = fabsf(o->pos.x - e->pos.x), dy = fabsf(o->pos.y - e->pos.y);
    int dist = (int)sqrtf(dx * dx + dy * dy);
    bool self_air = object_is_airborne(o), enemy_air = object_is_airborne(e);
    if(self_air) {
        dist = dist * 3 / 5;
    }
    int stance = 3;
    if(dist < 140)
        stance = 0;
    if(dist < 85)
        stance = 1;
    if(dist < 40)
        stance = 0;

    af_move *em = eh->executing_move ? af_get_move(eh->af_data, e->cur_animation->id) : NULL;
    bool can_act = h->state == STATE_STANDING || h->state == STATE_WALKTO || h->state == STATE_WALKFROM ||
                   h->state == STATE_CROUCHING || h->state == STATE_CROUCHBLOCK || h->state == STATE_JUMPING;
    if(!can_act) {
        return 0;
    }

    // counter the enemy while its move is recovering (0x514F0:330)
    if(em && (eh->damage_done || player_get_current_tick(e) + 2 >= player_get_len_ticks(e) * 2 / 3) && stance < 3) {
        int denom = 5000;
        if(enemy_air)
            denom >>= 2;
        static const int stance_mul[3] = {64, 32, 8};
        if((int)rand_int(denom) < D * stance_mul[stance]) {
            uint16_t mask = enemy_air ? (0x100 << stance) : (stance == 0 ? 0x80 : stance == 1 ? 0x01 : 0x20);
            if(ai_original_react(ctrl, a, o, h, mask, 0x23, false)) {
                return 0;
            }
        }
    }

    // react to an attack that is starting (0x534A0 mask)
    if(em && !eh->damage_done) {
        uint16_t mask = 0;
        if(em->category == CAT_HIGH)
            mask |= 0x02;
        if(em->category == CAT_LOW)
            mask |= 0x04;
        if(em->category == CAT_MEDIUM)
            mask |= 0x08;
        if(em->category == CAT_PROJECTILE)
            mask |= (e->pos.y < o->pos.y - 12) ? 0x10 : 0x2000;
        if(!enemy_air) {
            static const uint16_t ground_bits[4] = {0x80, 0x01, 0x20, 0x40};
            mask |= ground_bits[stance];
        } else {
            mask |= 0x100 << stance;
        }
        static const int stance_k[4] = {0x23, 0x1C, 9, 1};
        if(ai_original_react(ctrl, a, o, h, mask, stance_k[stance], true)) {
            return 0;
        }
    }

    // finishing moves (0x514F0:585) are handled by the arena ending hook

    // movement tendencies (0x514F0:200~310)
    int code = 0;
    if(!a->orig_advance) {
        if(orig_roll(a->orig_b * 14 + 320)) {
            a->orig_advance = 1;
        }
    } else {
        if(orig_roll(300)) {
            a->orig_advance = 0;
        }
        code = 3;
    }
    if(!a->orig_retreat) {
        if(orig_roll(a->orig_c * 12 + 150)) {
            a->orig_retreat = 1;
        }
    } else {
        if(orig_roll(a->orig_b * 6 + 160)) {
            a->orig_retreat = 0;
        }
        code = 7;
    }
    if(a->orig_advance && a->orig_retreat) {
        a->orig_advance = a->orig_retreat = 0;
    }
    if(enemy_air && orig_roll(a->orig_air + 90)) {
        code = 8;
    }
    if(a->orig_advance && orig_roll(a->orig_air + 70)) {
        code = 2;
    }
    if(stance != 3 && orig_roll(stance * a->orig_air * 2 + 80)) {
        code = 2;
    }
    if(a->orig_retreat && orig_roll(a->orig_air + 50)) {
        code = 8;
    }
    if(orig_roll(a->orig_air * 2 + 90) || (enemy_air && orig_roll(a->orig_air * 4 + 100))) {
        code = 8;
    }
    if(code) {
        orig_cmd_dir(ctrl, o, code, ev);
    } else {
        controller_cmd(ctrl, ACT_STOP, ev);
    }

    // MASTER.DAT 0x514F0:371 runs the move picker every tick while the stance is close (< 2) or the way is clear
    if(stance < 3) {
        static const uint16_t idle_bits[3] = {0x80, 0x01, 0x20};
        static const int idle_k[3] = {0x23, 0x1C, 9};
        ai_original_react(ctrl, a, o, h, enemy_air ? (0x100 << stance) : idle_bits[stance], idle_k[stance], false);
    }
    return 0;
}

static bool orig_finish_usable(object *o, const af_move *m) {
    int len = str_size(&m->move_string);
    if(len == 0) {
        return false;
    }
    for(int i = 0; i < len; i++) {
        char c = str_at(&m->move_string, i);
        if(!((c >= '1' && c <= '9') || c == 'K' || c == 'P')) {
            return false;
        }
    }
    return is_move_chain_allowed(o, (af_move *)m);
}

// MASTER.DAT 0x514F0:585: after the knockout the computer tries finishing moves from World Class up:
// scrap 1/300 per move, destruction and category 11 (e.g. the Fire Pit floor break) 1/100 above World Class
static void ai_original_finish(controller *ctrl, ctrl_event **ev, object *o, har *h) {
    ai *a = ctrl->data;
    if(a->selected_move) {
        process_selected_move(ctrl, ev);
        return;
    }
    int diff = settings_get()->gameplay.difficulty;
    // only in one player games (MASTER.DAT g_game_mode == 2), never in tournaments
    if(is_tournament(ctrl->gs) || diff < 3 || rand_int(2)) {
        return;
    }
    for(int i = 0; i < 70; i++) {
        af_move *m = af_get_move(h->af_data, i);
        if(!m || !orig_finish_usable(o, m)) {
            continue;
        }
        if(m->category == CAT_SCRAP && rand_int(300) == 0) {
            set_selected_move(ctrl, m);
            return;
        }
        if((m->category == CAT_DESTRUCTION || m->category == 11) && diff > 3 && rand_int(100) == 0) {
            set_selected_move(ctrl, m);
            return;
        }
    }
}

int ai_controller_poll(controller *ctrl, ctrl_event **ev) {
    ai *a = ctrl->data;
    object *o = game_state_find_object(ctrl->gs, ctrl->har_obj_id);
    scene *scene = game_state_get_scene(ctrl->gs);
    if(scene->id == SCENE_VS || scene->id == SCENE_NEWSROOM) {
        if(ctrl->gs->warp_speed || (scene->static_ticks_since_start + 1) % 256 == 0) {
            controller_cmd(ctrl, ACT_PUNCH, ev);
        }
    }
    if(!o) {
        return 1;
    }

    har *h = object_get_userdata(o);

    // Do not run AI while the game is paused
    if(game_state_is_paused(o->gs)) {
        return 0;
    }

    if(a->orig_mode && scene_is_arena(game_state_get_scene(o->gs)) &&
       arena_get_state(game_state_get_scene(o->gs)) == ARENA_STATE_ENDING) {
        ai_original_finish(ctrl, ev, o, h);
        return 0;
    }

    // Do not run AI while match is starting or ending
    // XXX this prevents the AI from doing scrap/destruction moves
    // XXX this could be fixed by providing a "scene changed" event
    if(scene_is_arena(game_state_get_scene(o->gs)) &&
       arena_get_state(game_state_get_scene(o->gs)) != ARENA_STATE_FIGHTING) {

        // null out selected move to fix the "AI not moving problem"
        a->selected_move = NULL;
        return 0;
    }

    // decrement act_timer
    a->act_timer--;

    // Grab all projectiles on screen
    vector_clear(&a->active_projectiles);
    game_state_get_projectiles(o->gs, &a->active_projectiles);

    if(a->orig_mode) {
        // projectiles are still blocked with the shared helper, everything else follows MASTER.DAT
        if(ai_block_projectile(ctrl, ev)) {
            return 0;
        }
        return ai_original_poll(ctrl, ev, o, h);
    }

    // Try to block har
    if(ai_block_har(ctrl, ev)) {
        return 0;
    }

    // Try to block projectiles
    if(ai_block_projectile(ctrl, ev)) {
        return 0;
    }

    // handle selected move
    if(a->selected_move) {
        // finish doing the selected move first
        process_selected_move(ctrl, ev);
        // log_debug("=== POLL === process_selected_move");
        return 0;
    }

    bool can_move = (h->state == STATE_STANDING || h->state == STATE_WALKTO || h->state == STATE_WALKFROM ||
                     h->state == STATE_CROUCHING || h->state == STATE_CROUCHBLOCK);

    bool can_interupt_tactic = (a->tactic->tactic_type == 0 ||
                                !(a->tactic->attack_type == ATTACK_CHARGE || a->tactic->attack_type == ATTACK_PUSH ||
                                  a->tactic->attack_type == ATTACK_TRIP));

    if(o) {
        object *enemy = game_state_find_object(ctrl->gs, o->animation_state.enemy_obj_id);
        // UJ tag signals to the AI it should probably jump
        if(can_move && can_interupt_tactic && player_frame_isset(enemy, TAG_UJ) && smart_sometimes(a)) {
            reset_tactic_state(a);
            controller_cmd(ctrl, ACT_UP, ev);
            controller_cmd(ctrl, ACT_STOP, ev);
            return 0;
        }
    }

    // be wary of repeated throws while attempting to complete a tactic
    if(can_move && can_interupt_tactic && a->thrown > 1 && a->difficulty > 2) {
        // attempt a quick attack to disrupt their grab/throw
        int enemy_range = get_enemy_range(ctrl);
        if((enemy_range == RANGE_CRAMPED || (enemy_range == RANGE_CLOSE && a->thrown >= 2)) &&
           (assign_move_by_cat(ctrl, CAT_LOW, false) || attempt_attack(ctrl, false))) {
            // log_debug("Spamming random attacks to avoid being thrown");
            reset_tactic_state(a);
            return 0;
        }
    }

    // attempt queued tactic
    if(a->tactic->tactic_type > 0 &&
       (can_move || (a->tactic->attack_type == ATTACK_JUMP && h->state == STATE_JUMPING))) {
        bool acted = handle_queued_tactic(ctrl, ev);
        // check if tactic is complete
        if(a->tactic->tactic_type == 0) {
            // reset movement act timer
            // set higher than zero to avoid glitching when bailing on tactics
            a->act_timer = 3;
        }

        if(acted) {
            return 0; // wait for next poll
        }
    }

    int enemy_range = get_enemy_range(ctrl);

    // attempt a random attack
    if((roll_chance(RANDOM_ATTACK_CHANCE) || diff_scale(a)) && (enemy_range <= RANGE_CLOSE || dumb_sometimes(a)) &&
       attempt_attack(ctrl, false)) {
        // log_debug("Random attack: %d", h->id);
        // reset movement act timer
        reset_act_timer(a);
        return 0;
    }

    // handle movement
    if(can_move) {
        handle_movement(ctrl, ev);
    }
    // log_debug("=== POLL === handle_movement");

    // queue a random tactic for next poll
    if((a->last_move_id == 0 || a->tactic->tactic_type == 0 || (roll_chance(RANDOM_ATTACK_CHANCE) && diff_scale(a))) &&
       can_move) {
        // log_debug("Attempt to queue random tactic[0m");
        int tacs[] = {TACTIC_SHOOT, TACTIC_CLOSE, TACTIC_FLY, TACTIC_PUSH, TACTIC_TRIP, TACTIC_GRAB, TACTIC_QUICK};
        chain_consider_tactics(ctrl, tacs, N_ELEMENTS(tacs));
    }

    return 0;
}

const char *ai_tactic(int tactic_type) {
    switch(tactic_type) {
        case TACTIC_ESCAPE:
            return "escape";
        case TACTIC_TURTLE:
            return "turtle";
        case TACTIC_GRAB:
            return "grab";
        case TACTIC_SPAM:
            return "spam";
        case TACTIC_SHOOT:
            return "shoot";
        case TACTIC_TRIP:
            return "trip";
        case TACTIC_QUICK:
            return "quick";
        case TACTIC_CLOSE:
            return "close";
        case TACTIC_FLY:
            return "fly";
        case TACTIC_PUSH:
            return "push";
        case TACTIC_COUNTER:
            return "counter";
    }
    return "?"; // Handle unexpected input
}

const char *ai_move(int move_type) {
    switch(move_type) {
        case MOVE_CLOSE:
            return "close";
        case MOVE_AVOID:
            return "avoid";
        case MOVE_JUMP:
            return "jump";
        case MOVE_HIGH_JUMP:
            return "high_jump";
        case MOVE_BLOCK:
            return "block";
    }
    return "?";
}

const char *ai_attack(int attack_type) {
    switch(attack_type) {
        case ATTACK_ID:
            return "id";
        case ATTACK_TRIP:
            return "trip";
        case ATTACK_GRAB:
            return "grab";
        case ATTACK_LIGHT:
            return "light";
        case ATTACK_HEAVY:
            return "heavy";
        case ATTACK_JUMP:
            return "jump";
        case ATTACK_RANGED:
            return "ranged";
        case ATTACK_CHARGE:
            return "charge";
        case ATTACK_PUSH:
            return "push";
        case ATTACK_RANDOM:
            return "random";
    }
    return "?";
}

void ai_controller_print_state(controller *ctrl, char *buf, size_t bufsize) {
    ai *a = ctrl->data;
    snprintf(buf, bufsize, "%s %s %s %d", ai_tactic(a->tactic->tactic_type), ai_move(a->tactic->move_type),
             ai_attack(a->tactic->attack_type), a->last_move_id);
}

void ai_controller_create(controller *ctrl, int difficulty, sd_pilot *pilot, int pilot_id) {
    ai *a = omf_calloc(1, sizeof(ai));
    a->difficulty = difficulty + 1;
    a->act_timer = 0;
    a->cur_act = 0;
    a->input_lag = 3;
    a->input_lag_timer = a->input_lag;
    a->selected_move = NULL;
    a->last_move_id = 0;
    a->orig_mode = settings_get()->gameplay.original_ai != 0;
    if(a->orig_mode) {
        a->input_lag = 1; // the original enters the whole move string at once
        a->input_lag_timer = a->input_lag;
    }
    a->move_str_pos = 0;
    memset(a->move_stats, 0, sizeof(a->move_stats));
    for(int i = 0; i < 70; i++) {
        a->move_stats[i].max_hit_dist = -1;
        a->move_stats[i].min_hit_dist = -1;
        a->move_stats[i].last_dist = -1;
    }
    a->blocked = 0;
    a->thrown = 0;
    a->shot = 0;
    vector_create(&a->active_projectiles, sizeof(object *));
    pilot->pilot_id = pilot_id;
    a->pilot = pilot;

    // one player pilots take their personality from the MASTER.DAT table; tournament pilots (with a photo)
    // already carry the values of their TRN file
    if(!pilot->photo) {
        reset_pilot_personality(pilot);
    }

    // set initial tactical state
    tactic_state *tactic = omf_calloc(1, sizeof(tactic_state));
    tactic->tactic_type = 0;
    tactic->last_tactic = 0;
    tactic->move_type = 0;
    tactic->move_timer = 0;
    tactic->attack_type = 0;
    tactic->attack_id = 0;
    tactic->attack_timer = 0;
    tactic->attack_on = 0;
    a->tactic = tactic;
    reset_tactic_state(a);

    ctrl->data = a;
    ctrl->type = CTRL_TYPE_AI;
    ctrl->poll_fun = &ai_controller_poll;
    ctrl->har_hook = &ai_har_event;
    ctrl->free_fun = &ai_controller_free;
}
