#ifndef HAR_ECONOMY_H
#define HAR_ECONOMY_H

#include "formats/chr.h"
#include "formats/pilot.h"

enum HAR_UPGRADE
{
    ARM_POWER,
    ARM_SPEED,
    LEG_POWER,
    LEG_SPEED,
    ARMOR,
    STUN_RES,
    N_UPGRADES,
};

static const uint8_t max_arm_speed[11] = {6, 8, 4, 6, 9, 7, 8, 6, 9, 6, 7};
static const uint8_t max_leg_speed[11] = {8, 9, 5, 6, 8, 8, 7, 6, 7, 5, 6};
static const uint8_t max_arm_power[11] = {5, 5, 9, 8, 4, 6, 6, 5, 5, 6, 7};
static const uint8_t max_leg_power[11] = {6, 6, 8, 4, 5, 7, 5, 7, 6, 7, 7};
static const uint8_t max_stun_res[11] = {8, 6, 8, 6, 7, 6, 6, 7, 7, 7, 6};
static const uint8_t max_armor[11] = {5, 7, 7, 8, 6, 8, 6, 9, 6, 6, 7};

static const int32_t har_prices[11] = {20000, 36000, 26000, 28000, 29000, 32000, 25000, 30000, 24000, 22000, 75000};
static const int32_t har_upgrade_price[11] = {380, 400, 350, 400, 500, 330, 420, 370, 450, 360, 700};
static const int32_t upgrade_level_multiplier[10] = {0, 1, 3, 7, 12, 18, 30, 50, 75, 120};

static const int32_t arm_leg_multiplier = 2;
static const int32_t stun_res_multiplier = 3;
static const int32_t armor_multiplier = 5;

float calculate_winnings(sd_pilot *winner, sd_pilot *loser, float winnings_multiplier);
bool har_can_upgrade(sd_pilot *pilot, enum HAR_UPGRADE upgrade);
void purchase_random_har(sd_pilot *pilot);
void purchase_random_har_upgrades(sd_pilot *pilot);
void upgrade_har(sd_pilot *pilot, enum HAR_UPGRADE);
int32_t upgrade_price(sd_pilot *pilot, enum HAR_UPGRADE);

// MASTER.DAT 0x3A1A0: a computer pilot spends a little money on single upgrade steps, each with its own chance.
void ai_pilot_gradual_upgrade(sd_pilot *pilot);

// MASTER.DAT 0x3ADC0: after each of the player's tournament matches the other pilots fight simulated matches
// against their neighbour in the ranking, trade places and money, and grow their offense / defense.
void trn_simulate_other_matches(sd_chr_file *chr);

#endif // HAR_ECONOMY_H
