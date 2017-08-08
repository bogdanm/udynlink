#include "udynlink.h"
#include "mod_globals1_module_data.h"
#include "test_utils.h"
#include <stdio.h>

#define EXPECTED_G_VAL                      60

int test_qemu(void) {
    const char *exported_syms[] = {"test", "g", NULL};
    udynlink_module_t *p_mod;
    int res = 0;

    for (int i = (int)_UDYNLINK_LOAD_MODE_FIRST; i <= (int)_UDYNLINK_LOAD_MODE_LAST; i ++) {
        if ((p_mod = udynlink_load_module(mod_globals1_module_data, NULL, 0, (udynlink_load_mode_t)i, NULL)) == NULL)
            return 0;
        CHECK_RAM_SIZE(p_mod, 2 * sizeof(int));
        if (!check_exported_symbols(p_mod, exported_syms))
            goto exit;
        if (!run_test_func(p_mod))
            goto exit;
        // Check the expected value of the global variable
        uint32_t v = *(int*)udynlink_get_symbol_value(p_mod, "g");
        if (v != EXPECTED_G_VAL) {
            printf("Unexpected value %d for variable 'g', expected %d\n", v, EXPECTED_G_VAL);
            goto exit;
        }
        udynlink_unload_module(p_mod);
    }
    res = 1;
    p_mod = NULL;
exit:
    if (p_mod)
        udynlink_unload_module(p_mod);
    return res;
}
