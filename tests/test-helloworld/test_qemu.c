#include "udynlink.h"
#include "mod_hello_module_data.h"
#include "test_utils.h"
#include <stdio.h>

int test_qemu(void) {
    const char *exported_syms[] = {"hello", "test", NULL};
    const char *extern_syms[] = {"printf", NULL};
    udynlink_module_t *p_mod;
    int res = 0;

    for (int i = (int)_UDYNLINK_LOAD_MODE_FIRST; i <= (int)_UDYNLINK_LOAD_MODE_LAST; i ++) {
        if ((p_mod = udynlink_load_module(mod_hello_module_data, NULL, 0, (udynlink_load_mode_t)i, NULL)) == NULL)
            return 0;
        CHECK_RAM_SIZE(p_mod, 0);
        if (!check_exported_symbols(p_mod, exported_syms))
            goto exit;
        if (!check_extern_symbols(p_mod, extern_syms))
            goto exit;
        if (!run_test_func(p_mod))
            goto exit;
        udynlink_unload_module(p_mod);
    }
    res = 1;
    p_mod = NULL;
exit:
    if (p_mod)
        udynlink_unload_module(p_mod);
    return res;
}
