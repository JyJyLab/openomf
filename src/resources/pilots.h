#ifndef PILOTS_H
#define PILOTS_H

#include <stdint.h>

typedef struct pilot_t {
    int power, agility, endurance;
    uint8_t color_3; ///< HAR Tertiary Color.
    uint8_t color_2; ///< HAR Secondary Color.
    uint8_t color_1; ///< HAR Primary Color.
    void *userdata;
    int sex;
} pilot;

void pilot_get_info(pilot *pilot, int id);

// MASTER.DAT 0x494BC: stat shift applied to computer opponents in one player games, by difficulty
// (Punching Bag -5, Rookie -2, Veteran/World Class 0, Champion +2, Deadly +4, Ultimate +7). Stats never go below 0.
// Agility sets the HAR's walking, jumping and falling speed; the difficulty bonus never lifts it above the player's
// agility (an opponent that is already faster keeps its own value), so higher difficulties do not out-speed the player.
void pilot_apply_sp_difficulty(int *power, int *agility, int *endurance, int difficulty, int player_agility);

#endif // PILOTS_H
