#ifndef ENTITY_H
#define ENTITY_H

typedef struct Entity {
    int x, y;           // position
    char symbol;        // map representation
    int blocks;         // does it block movement?
} Entity;

typedef struct Item {
    Entity entity;          // still an entity
    char name[32];
    int stackable;
    // optional effect function pointer
} Item;

#endif