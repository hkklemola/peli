#ifndef ACTOR_H
#define ACTOR_H

typedef struct {
    int hp;
    int max_hp;
    int attack;
    int defense;
    int magic;          // optional stat
    int speed;          // movement/action speed
    int level;          // character level
    int experience;     // XP (for player)
} Actor;

#endif