#ifndef __TEST_UTILS_H__
#define __TEST_UTIlS_H__

#include "udynlink.h"

#define CHECK_RAM_SIZE(p, s)\
do {\
    if (p->p_header->data_size + p->p_header->bss_size != s) {\
        printf("Unexpected RAM size '%u', expected '%u'\n", p->p_header->data_size + p->p_header->bss_size, s);\
        goto exit;\
    }\
} while(0)

int is_exported_symbol(const udynlink_module_t *p_mod, const char *name);
int is_extern_symbol(const udynlink_module_t *p_mod, const char *name);
int check_exported_symbols(const udynlink_module_t *p_mod, const char *slist[]);
int check_extern_symbols(const udynlink_module_t *p_mod, const char *slist[]);
int run_test_func(const udynlink_module_t *p_mod);

#endif

