#include "game/scenes/mechlab/har_economy.h"
#include "formats/pilot.h"
#include "utils/c_array_util.h"
#include "utils/log.h"
#include "utils/miscmath.h"
#include "utils/random.h"

bool har_can_upgrade(sd_pilot *pilot, enum HAR_UPGRADE upgrade) {
    switch(upgrade) {
        case ARM_POWER:
            return pilot->arm_power < max_arm_power[pilot->har_id];
        case ARM_SPEED:
            return pilot->arm_speed < max_arm_speed[pilot->har_id];
        case LEG_POWER:
            return pilot->leg_power < max_leg_power[pilot->har_id];
        case LEG_SPEED:
            return pilot->leg_speed < max_leg_speed[pilot->har_id];
        case ARMOR:
            return pilot->armor < max_armor[pilot->har_id];
        case STUN_RES:
            return pilot->stun_resistance < max_stun_res[pilot->har_id];
        default:
            abort();
    }
}

void purchase_random_har(sd_pilot *pilot) {
    int32_t possible_purchases[10];
    size_t n = 0;
    for(int32_t i = 0; i < 10; ++i) {
        if(pilot->money >= har_prices[i]) {
            possible_purchases[n] = i;
            ++n;
        }
    }
    assert(n > 0 && "Pilot does not have enough money to purchase any HAR");
    pilot->har_id = possible_purchases[rand_int(n)];
    pilot->money -= har_prices[pilot->har_id];
    log_debug("Pilot %s purchased HAR %u and has %d money left", str_c(&pilot->name), pilot->har_id, pilot->money);
}

void purchase_random_har_upgrades(sd_pilot *pilot) {
    enum HAR_UPGRADE possible_upgrades[N_UPGRADES];
    size_t n;
    do {
        n = 0;
        for(enum HAR_UPGRADE u = ARM_POWER; u < N_UPGRADES; ++u) {
            if(har_can_upgrade(pilot, u) && pilot->money >= upgrade_price(pilot, u)) {
                possible_upgrades[n] = u;
                ++n;
            }
        }
        if(n == 0) {
            break;
        }
        enum HAR_UPGRADE chosen_upgrade = possible_upgrades[rand_int(n)];
        upgrade_har(pilot, chosen_upgrade);
        log_debug("Pilot %s bought upgrade %u and has %d money left", str_c(&pilot->name), chosen_upgrade,
                  pilot->money);
    } while(n > 0);
}

int32_t upgrade_price(sd_pilot *pilot, enum HAR_UPGRADE upgrade) {
    int32_t price = har_upgrade_price[pilot->har_id];
    switch(upgrade) {
        case ARM_POWER:
            price *= upgrade_level_multiplier[pilot->arm_power + 1] * arm_leg_multiplier;
            break;
        case ARM_SPEED:
            price *= upgrade_level_multiplier[pilot->arm_speed + 1] * arm_leg_multiplier;
            break;
        case LEG_POWER:
            price *= upgrade_level_multiplier[pilot->leg_power + 1] * arm_leg_multiplier;
            break;
        case LEG_SPEED:
            price *= upgrade_level_multiplier[pilot->leg_speed + 1] * arm_leg_multiplier;
            break;
        case ARMOR:
            price *= upgrade_level_multiplier[pilot->armor + 1] * armor_multiplier;
            break;
        case STUN_RES:
            price *= upgrade_level_multiplier[pilot->stun_resistance + 1] * stun_res_multiplier;
            break;
        default:
            abort();
    }
    return price;
}

void upgrade_har(sd_pilot *pilot, enum HAR_UPGRADE upgrade) {
    int32_t price = upgrade_price(pilot, upgrade);
    assert(pilot->money >= price);
    assert(har_can_upgrade(pilot, upgrade));

    pilot->money -= price;
    switch(upgrade) {
        case ARM_POWER:
            pilot->arm_power++;
            break;
        case ARM_SPEED:
            pilot->arm_speed++;
            break;
        case LEG_POWER:
            pilot->leg_power++;
            break;
        case LEG_SPEED:
            pilot->leg_speed++;
            break;
        case ARMOR:
            pilot->armor++;
            break;
        case STUN_RES:
            pilot->stun_resistance++;
            break;
        default:
            abort();
    }
}

static void update_total_value(sd_pilot *pilot) {
    int value = har_prices[pilot->har_id];
    for(int i = 1; i < pilot->arm_power; i++) {
        value += har_upgrade_price[pilot->har_id] * upgrade_level_multiplier[i] * arm_leg_multiplier;
    }

    for(int i = 1; i < pilot->arm_speed; i++) {
        value += har_upgrade_price[pilot->har_id] * upgrade_level_multiplier[i] * arm_leg_multiplier;
    }

    for(int i = 1; i < pilot->leg_power; i++) {
        value += har_upgrade_price[pilot->har_id] * upgrade_level_multiplier[i] * arm_leg_multiplier;
    }

    for(int i = 1; i < pilot->leg_speed; i++) {
        value += har_upgrade_price[pilot->har_id] * upgrade_level_multiplier[i] * arm_leg_multiplier;
    }

    for(int i = 1; i < pilot->armor; i++) {
        value += har_upgrade_price[pilot->har_id] * upgrade_level_multiplier[i] * armor_multiplier;
    }

    for(int i = 1; i < pilot->stun_resistance; i++) {
        value += har_upgrade_price[pilot->har_id] * upgrade_level_multiplier[i] * stun_res_multiplier;
    }

    pilot->total_value = (value * 70) / 100;
}

static int rank_additions[] = {
    500, 200, 160, 120, 100, 90, 80, 70, 60, 55, 50, 45, 40, 35, 30, 25, 20,
    17,  15,  14,  13,  12,  11, 11, 10, 9,  8,  8,  7,  7,  6,  6,  5,  0,
};

static float rank_scale(unsigned rank) {
    assert(rank >= 1);
    rank -= 1;
    float a = rank * 0.1 + 1.0;
    float b = rank * 0.2 + 1.0;
    return b * a * a;
}

static float rank_bonus(unsigned rank) {
    assert(rank >= 1);
    rank -= 1;
    if(rank < N_ELEMENTS(rank_additions)) {
        return rank_additions[rank];
    }
    return 5.0;
}

float calculate_winnings(sd_pilot *winner, sd_pilot *loser, float winnings_multiplier) {
    update_total_value(winner);
    update_total_value(loser);

    int adjusted_value = winner->total_value - (har_prices[0] * 85) / 100 - 500;
    float loser_value = (loser->total_value + loser->money) / 45.0 + (int)loser->winnings;
    float winnings = loser_value + (adjusted_value / rank_scale(winner->rank) / 30.0);

    winnings += winner->trn_rank_money * rank_bonus(winner->rank);
    winnings *= 0.7f;
    winnings *= winnings_multiplier;
    return winnings;
}

static const int32_t training_cost[] = {5,   8,   12,   18,   24,   30,   45,   60,    80,    110,   150,   250,  400,
                                         700, 1000, 1400, 2000, 2800, 4000, 5500, 7500, 10000, 14000, 20000, 30000};

static void ai_try_upgrade(sd_pilot *pilot, enum HAR_UPGRADE upgrade, int chance) {
    if(har_can_upgrade(pilot, upgrade) && rand_int(chance) == 0) {
        int32_t price = upgrade_price(pilot, upgrade);
        if(price < pilot->money) {
            upgrade_har(pilot, upgrade);
        }
    }
}

static void ai_try_training(sd_pilot *pilot, uint8_t *stat) {
    if(*stat < 25 && rand_int(3) == 0) {
        int32_t price = training_cost[*stat] * 10;
        if(price < pilot->money) {
            pilot->money -= price;
            (*stat)++;
        }
    }
}

void ai_pilot_gradual_upgrade(sd_pilot *pilot) {
    ai_try_upgrade(pilot, ARM_SPEED, 6);
    ai_try_upgrade(pilot, LEG_SPEED, 6);
    ai_try_upgrade(pilot, ARM_POWER, 6);
    ai_try_upgrade(pilot, LEG_POWER, 6);
    ai_try_upgrade(pilot, ARMOR, 10);
    ai_try_upgrade(pilot, STUN_RES, 8);
    ai_try_training(pilot, &pilot->power);
    ai_try_training(pilot, &pilot->agility);
    ai_try_training(pilot, &pilot->endurance);
}

// MASTER.DAT 0x3A0C4: pilot strength used by the simulated matches
static int pilot_strength(const sd_pilot *p) {
    return p->winnings / 50 + 2 * (p->power + p->agility + p->endurance) +
           8 * (p->arm_power + p->leg_power + p->arm_speed + p->leg_speed) + 14 * p->armor + 10 * p->stun_resistance;
}

static sd_pilot *pilot_with_rank(sd_chr_file *chr, int rank) {
    for(int i = 0; i < (int)chr->pilot.enemies_inc_unranked; i++) {
        if(chr->enemies[i] && chr->enemies[i]->pilot.rank == rank) {
            return &chr->enemies[i]->pilot;
        }
    }
    return NULL;
}

void trn_simulate_other_matches(sd_chr_file *chr) {
    int ranked = chr->pilot.enemies_ex_unranked + 1; // includes the player
    float mult = chr->winnings_multiplier > 0 ? chr->winnings_multiplier : 1.0f;
    bool fought[MAX_CHR_ENEMIES + 2] = {false};

    // challenges from the bottom of the ranking upwards, 65% chance each
    for(int rank = ranked; rank >= 2; rank--) {
        sd_pilot *challenger = pilot_with_rank(chr, rank);
        sd_pilot *defender = pilot_with_rank(chr, rank - 1);
        if(!challenger || !defender || fought[rank] || fought[rank - 1] || rand_int(100) >= 65) {
            continue;
        }
        fought[rank] = fought[rank - 1] = true;
        int vc = pilot_strength(challenger);
        int vd = pilot_strength(defender);
        int a = (int)(vc * 1.7f) + rand_int(max2(1, (int)(vc * 1.5f)));
        int b = (int)(vd * 1.2f) + rand_int(max2(1, (int)(vd * 1.3f)));
        sd_pilot *winner = challenger, *loser = defender;
        if(!(b < a || rand_int(10) == 0)) {
            winner = defender;
            loser = challenger;
        }
        int ls = pilot_strength(loser);
        winner->money += (int32_t)((ls * 6 + loser->winnings) * mult * 0.6f);
        loser->money -= (int32_t)(ls * 0.6f);
        winner->wins++;
        loser->losses++;
        if(winner == challenger) {
            // trade places in the ranking
            uint8_t r = challenger->rank;
            challenger->rank = defender->rank;
            defender->rank = r;
        }
        ai_pilot_gradual_upgrade(challenger);
        ai_pilot_gradual_upgrade(defender);
    }

    // everybody slowly gets better (caps by tournament difficulty, table 0x81E34)
    static const int caps[4][2] = {{39, 35}, {70, 70}, {120, 120}, {240, 240}};
    static const int growth_base[4] = {4, 6, 13, 27};
    int d = chr->pilot.difficulty & 3;
    int growth = (int)(growth_base[d] * mult);
    for(int i = 0; i < (int)chr->pilot.enemies_inc_unranked; i++) {
        if(!chr->enemies[i]) {
            continue;
        }
        sd_pilot *p = &chr->enemies[i]->pilot;
        if((int)rand_int(100) < growth && ((int)rand_int(400) > p->offense || rand_int(50) == 0) && p->offense < caps[d][0]) {
            p->offense++;
        }
        if((int)rand_int(100) < growth && ((int)rand_int(400) > p->defense || rand_int(50) == 0) && p->defense < caps[d][1]) {
            p->defense++;
        }
    }
}

