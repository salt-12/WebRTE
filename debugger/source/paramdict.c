// golden
// 12/20/18
//

#include "paramdict.h"

struct paramdict *paramdict_alloc() {
    struct paramdict *pd = (struct paramdict *)malloc(sizeof(struct paramdict));

    pd->size = 4096;
    pd->keys = (char **)malloc(pd->size);
    pd->values = (char **)malloc(pd->size);
    pd->length = 0;

    return pd;
}

int paramdict_add(struct paramdict *pd, char *key, char *value) {
    if(pd->length * sizeof(char *) >= pd->size) {
        pd->size += 4096;
        pd->keys = (char **)realloc(pd->keys, pd->size);
        pd->values = (char **)realloc(pd->values, pd->size);
    }

    pd->keys[pd->length] = key;
    pd->values[pd->length] = value;
    pd->length++;

    return 0;
}

char *paramdict_search(struct paramdict *pd, char *key) {
    int i;

    for(i = 0; i < pd->length; i++) {
        if(!strcmp(pd->keys[i], key)) {
            return pd->values[i];
        }
    }

    return NULL;
}

int paramdict_free(struct paramdict *pd) {
    free(pd->keys);
    free(pd->values);
    free(pd);

    return 0;
}
