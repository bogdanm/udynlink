#include "udynlink.h"
#include <stdio.h>

int is_exported_symbol(const udynlink_module_t *p_mod, const char *name) {
    udynlink_sym_t sym;

    if(udynlink_lookup_symbol(p_mod, name, &sym) == NULL) {
        return 0;
    }
    return sym.type == UDYNLINK_SYM_TYPE_EXPORTED;
}

int is_extern_symbol(const udynlink_module_t *p_mod, const char *name) {
    udynlink_sym_t sym;

    if(udynlink_lookup_symbol(p_mod, name, &sym) == NULL) {
        return 0;
    }
    return sym.type == UDYNLINK_SYM_TYPE_EXTERN;
}

int check_exported_symbols(const udynlink_module_t *p_mod, const char *slist[]) {
    for (; *slist; slist ++) {
        if (!is_exported_symbol(p_mod, *slist)) {
            printf("Exported symbol '%s' not found in symbol table.\n", *slist);
            return 0;
        }
    }
    return 1;
}

int check_extern_symbols(const udynlink_module_t *p_mod, const char *slist[]) {
    for (; *slist; slist ++) {
        if (!is_extern_symbol(p_mod, *slist)) {
            printf("Extern symbol '%s' not found in symbol table.\n", *slist);
            return 0;
        }
    }
    return 1;
}

int run_test_func(const udynlink_module_t *p_mod) {
    udynlink_sym_t sym;

    // Run test
    if (udynlink_lookup_symbol(p_mod, "test", &sym) == NULL) {
        printf("'test' symbol not found.\n");
        return 0;
    }
    int (*p_func)(void) = (int (*)(void))sym.val;
    return p_func();
}

