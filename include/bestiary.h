#ifndef BESTIARY_H
#define BESTIARY_H
#define MAX_SPECIES 64
#include "d:/projekti/peli/include/entity.h"
#include "d:/projekti/peli/include/actor.h"




// Template describing a creature species
typedef struct {

    char name[32];
    char symbol;

    int max_hp;
    int attack;
    int defense;
    int magic;
    int speed;

    int level;
    int xp_value;

} CreatureTemplate;


// Bestiary storage
extern CreatureTemplate bestiary[MAX_SPECIES];


// Initialize creature templates
void bestiary_init(void);


// Get template by ID
CreatureTemplate* bestiary_get(int id);


// IDs for creatures
enum
{
    CREATURE_PLAYER,
    CREATURE_GOBLIN,
    CREATURE_ORC,
    CREATURE_TROLL
};

#endif