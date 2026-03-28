#include "object.h"
#include <string.h>

void object_init(object_t *obj) {
    if (!obj) return;
    memset(obj, 0, sizeof(object_t));
    // Object-only fields are zeroed. Base entity initialization can be done by caller.
}
