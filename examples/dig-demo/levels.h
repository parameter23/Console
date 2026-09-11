/**
 * @file levels.h
 * @brief Level (re)construction for the dig-demo game.
 */
#ifndef LEVELS_H
#define LEVELS_H

/**
 * @brief Procedurally (re)builds a level into map_data.
 * @param index Level index; currently ignored (there is only one demo
 *              level) but kept in the signature so adding real level
 *              data later doesn't change the call site.
 */
void level_load(int index);

#endif
