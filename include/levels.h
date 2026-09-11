#ifndef LEVELS_H
#define LEVELS_H

/* Procedurally (re)builds a level into map_data. index is currently
 * ignored - there is only one demo level - but kept in the signature so
 * adding real level data later doesn't change the call site. */
void level_load(int index);

#endif
