#ifndef OBJECT_H
#define OBJECT_H

#include "entity.h"


typedef struct Object {
    Entity base;
    // Add object-specific fields here
} Object;

typedef Object object_t; // for compatibility

// Object initialization (fills with zero and sets up base entity defaults)
void object_init(object_t *obj);

#endif // OBJECT_H
