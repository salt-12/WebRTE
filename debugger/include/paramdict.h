// golden
// 12/20/18
//

#include <ps4.h>

struct paramdict {
    char **keys;
    char **values;
    int length;
    unsigned int size; // same size is used for both, if one needs more memory both are realloc'd
};

struct paramdict *paramdict_alloc();
int paramdict_add(struct paramdict *pd, char *key, char *value);
char *paramdict_search(struct paramdict *pd, char *key);
int paramdict_free(struct paramdict *pd);
