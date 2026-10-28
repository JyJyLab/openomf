
#include "resources/pilots.h"
#include "game/common_defines.h"
#include "game/utils/settings.h"

void pilot_get_info(pilot *pilot, int id) {
    // TODO read this from MASTER.DAT
    // XXX the colors are eyeballed
    pilot->sex = PILOT_SEX_MALE;
    switch(id) {
        case PILOT_CRYSTAL:
            pilot->power = 5;
            pilot->agility = 16;
            pilot->endurance = 9;
            pilot->color_1 = 5;
            pilot->color_2 = 11;
            pilot->color_3 = 8;
            pilot->sex = PILOT_SEX_FEMALE;
            break;
        case PILOT_STEFFAN:
            pilot->power = 13;
            pilot->agility = 9;
            pilot->endurance = 8;
            pilot->color_1 = 10;
            pilot->color_2 = 15;
            pilot->color_3 = 7;
            break;
        case PILOT_MILANO:
            pilot->power = 7;
            pilot->agility = 20;
            pilot->endurance = 4;
            pilot->color_1 = 11;
            pilot->color_2 = 12;
            pilot->color_3 = 7;
            break;
        case PILOT_CHRISTIAN:
            pilot->power = 9;
            pilot->agility = 7;
            pilot->endurance = 15;
            pilot->color_1 = 8;
            pilot->color_2 = 15;
            pilot->color_3 = 6;
            break;
        case PILOT_SHIRRO:
            pilot->power = 20;
            pilot->agility = 1;
            pilot->endurance = 8;
            pilot->color_1 = 4;
            pilot->color_2 = 7;
            pilot->color_3 = 14;
            break;
        case PILOT_JEANPAUL:
            pilot->power = 9;
            pilot->agility = 10;
            pilot->endurance = 11;
            pilot->color_1 = 1;
            pilot->color_2 = 7;
            pilot->color_3 = 6;
            break;
        case PILOT_IBRAHIM:
            pilot->power = 10;
            pilot->agility = 1;
            pilot->endurance = 20;
            pilot->color_1 = 8;
            pilot->color_2 = 6;
            pilot->color_3 = 14;
            break;
        case PILOT_ANGEL:
            pilot->power = 7;
            pilot->agility = 10;
            pilot->endurance = 13;
            pilot->color_1 = 0;
            pilot->color_2 = 15;
            pilot->color_3 = 7;
            pilot->sex = PILOT_SEX_FEMALE;
            break;
        case PILOT_COSSETTE:
            pilot->power = 14;
            pilot->agility = 8;
            pilot->endurance = 8;
            pilot->color_1 = 0;
            pilot->color_2 = 8;
            pilot->color_3 = 2;
            pilot->sex = PILOT_SEX_FEMALE;
            break;
        case PILOT_RAVEN:
            pilot->power = 14;
            pilot->agility = 4;
            pilot->endurance = 12;
            pilot->color_1 = 9;
            pilot->color_2 = 10;
            pilot->color_3 = 4;
            break;
        case PILOT_KREISSACK:
            pilot->power = 16;
            pilot->agility = 15;
            pilot->endurance = 16;
            pilot->color_1 = 5;
            pilot->color_2 = 1;
            pilot->color_3 = 15;
            break;
    }
}

static int sp_difficulty_shift(int value, int shift) {
    // the original subtracts a "penalty" and clamps at zero
    return value > shift ? value - shift : 0;
}

void pilot_apply_sp_difficulty(int *power, int *agility, int *endurance, int difficulty, int speed_limit) {
    static const int penalty[] = {5, 2, 0, 0, -2, -4, -7};
    if(difficulty < 0 || difficulty > 6) {
        return;
    }
    int shift = penalty[difficulty];
    int base_agility = *agility;
    *power = sp_difficulty_shift(*power, shift);
    *agility = sp_difficulty_shift(*agility, shift);
    *endurance = sp_difficulty_shift(*endurance, shift);
    if(shift < 0 && speed_limit >= 0) {
        // speed limit: the bonus stops at speed_limit
        int limit = base_agility > speed_limit ? base_agility : speed_limit;
        if(*agility > limit) {
            *agility = limit;
        }
    }
}

int pilot_cpu_speed_limit(void) {
    if(!settings_get()->gameplay.cpu_speed_limit) {
        return -1;
    }
    int fastest = 0;
    for(int id = PILOT_CRYSTAL; id <= PILOT_RAVEN; id++) {
        pilot p;
        pilot_get_info(&p, id);
        if(p.agility > fastest) {
            fastest = p.agility;
        }
    }
    return fastest;
}
