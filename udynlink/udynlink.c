#include "udynlink.h"
#include "udynlink_externals.h"
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
// TODO: remove next
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// Local macros and data

#ifndef UDYNLINK_MAX_HANDLES
#warning Setting UDYNLINK_MAX_HANDLES to 1 by default
#define UDYNLINK_MAX_HANDLES                  1
#endif

#define UDYNLINK_MODULE_SIGN                  (((uint32_t)'M' << 24) | ((uint32_t)'L' << 16) | ((uint32_t)'D' << 8) | (uint32_t)'U')

static udynlink_module_t module_table[UDYNLINK_MAX_HANDLES];
static udynlink_debug_level_t debug_level;

#define _UDYNLINK_EXPAND(x)                   #x"\n"
static const char * const error_codes[] = {
    UDYNLINK_ERROR_CODES
};
#undef _UDYNLINK_EXPAND

// Symbol table masks and data
#define UDYNLINK_SYM_OFFSET_MASK              0x0FFFFFFF
#define UDYNLINK_SYM_INFO_SHIFT               28
#define UDYNLINK_SYM_INFO_CODE_MASK           0x04
#define UDYNLINK_SYM_INFO_TYPE_MASK           0x03
#define UDYNLINK_SYM_NAME_OFFSET              0

// Module structure masks
#define UDYNLINK_LOAD_MODE_MASK               (uint8_t)0x03
#define UDYNLINK_LOAD_FOREIGN_RAM_MASK        (uint8_t)0x04
#define UDYNLINK_LOAD_GET_MODE(p_mod)         (udynlink_load_mode_t)(p_mod->info & UDYNLINK_LOAD_MODE_MASK)
#define UDYNLINK_LOAD_SET_MODE(p_mod, m)      p_mod->info = (p_mod->info & (uint8_t)~UDYNLINK_LOAD_MODE_MASK) | ((uint8_t)m)
#define UDYNLINK_LOAD_IS_FOREIGN_RAM(p_mod)   ((p_mod->info & UDYNLINK_LOAD_FOREIGN_RAM_MASK) != 0)
#define UDYNLINK_LOAD_SET_FOREIGN_RAM(p_mod)  p_mod->info |= UDYNLINK_LOAD_FOREIGN_RAM_MASK
#define UDYNLINK_LOAD_CLR_FOREIGN_RAM(p_mod)  p_mod->info &= (uint8_t)~UDYNLINK_LOAD_FOREIGN_RAM_MASK
#define UDYNLINK_ASM_PROLOGUE_LEN             18 // this shall match the preamble code length in bytes
////////////////////////////////////////////////////////////////////////////////
// Helpers - debug

// Helper for the 'udynlink_debug' method
// Needed just because we don't want to have two dependencies (udynlink_external printf and
// udynlink_external_vprintf) instead of a single one (udynlink_external_vprintf).
static void internal_printf(const char *msg, ...) {
    va_list va;

    va_start(va, msg);
    udynlink_external_vprintf(msg, va);
    va_end(va);
}

// The debug output function
static void udynlink_debug(const char *func, int line, udynlink_debug_level_t level, const char *msg, ...) {
    va_list va;
    // If the array below is modified, remember to also modify the "udynlink_debug_level_t" enum in the header!
    static const char * const names[] = {"n/a", "error", "warning", "info"};

    if ((int)level > (int)debug_level) {
        return;
    }
    internal_printf("[udynlink %s in function %s, line %d] ", names[(int)level], func, line);
    va_start(va, msg);
    udynlink_external_vprintf(msg, va);
    va_end(va);
}

////////////////////////////////////////////////////////////////////////////////
// Helpers - offsets and addresses

// Returns the offset of code from the given module header address
// The code comes after the header, the relocations and the symbol table.
static uint32_t get_code_offset_from_header(const udynlink_module_header_t *p_header) {
    uint32_t res = sizeof(udynlink_module_header_t) + p_header->num_rels * 2 * sizeof(uint32_t) + p_header->symt_size;
    return res;
}

// Gets the address of the code
static uint8_t *get_code_pointer(const udynlink_module_t *p_mod) {
    const udynlink_module_header_t *p_header = p_mod->p_header;

    if (UDYNLINK_LOAD_GET_MODE(p_mod) == UDYNLINK_LOAD_MODE_COPY_CODE) { // the code is after the LOT in RAM.
        return (uint8_t*)p_mod->p_ram + p_header->num_lot * sizeof(uint32_t);// the code is after the LOT in RAM.
    } else { // the code is after the module header, the relocations and the symbol table
        return (uint8_t*)p_header + get_code_offset_from_header(p_header);
    }
}

// Gets the address of the data section (in RAM)
static uint8_t *get_data_pointer(const udynlink_module_t *p_mod) {
    const udynlink_module_header_t *p_header = p_mod->p_header;

    switch (UDYNLINK_LOAD_GET_MODE(p_mod)) {
        case UDYNLINK_LOAD_MODE_XIP: // the data is after the LOT
            return (uint8_t*)p_mod->p_ram + p_header->num_lot * sizeof(uint32_t);
        case UDYNLINK_LOAD_MODE_COPY_CODE: // the data is after the code in RAM (which is in turn after the LOT)
            return (uint8_t*)p_mod->p_ram + p_header->num_lot * sizeof(uint32_t) + p_header->code_size;
        case UDYNLINK_LOAD_MODE_COPY_ALL: // use directly the data section from the module header (after the code section)
            return (uint8_t*)p_header + get_code_offset_from_header(p_header) + p_header->code_size;
        default:
            UDYNLINK_DEBUG(UDYNLINK_DEBUG_ERROR, "Invalid load mode %d\n", (int)UDYNLINK_LOAD_GET_MODE(p_mod));
            return NULL;
    }
}

// Gets the pointer to the symbol table according to the given module header
static const uint32_t *get_sym_table_pointer(const udynlink_module_t *p_mod) {
    const udynlink_module_header_t *p_header = p_mod->p_header;

    return (uint32_t*)p_header + sizeof(udynlink_module_header_t) / sizeof(uint32_t) + p_header->num_rels * 2;
}

// Return a pointer to the relocation data (after the header)
static const uint32_t *get_relocs_pointer(const udynlink_module_t *p_mod) {
    const udynlink_module_header_t *p_header = p_mod->p_header;

    return (const uint32_t*)p_header + sizeof(udynlink_module_header_t) / sizeof(uint32_t);
}

////////////////////////////////////////////////////////////////////////////////
// Helpers - various

// Find the next free entry in the module table, return a pointer to it o NULL
static udynlink_module_t *get_next_free_module(void) {
    for (uint32_t i = 0; i < UDYNLINK_MAX_HANDLES; i ++) {
        if (module_table[i].p_header == NULL) {
            return module_table + i;
        }
    }
    return NULL;
}

// Marks the given module as "free" by zeroing its data structure
static void mark_module_free(udynlink_module_t *p_mod) {
    memset(p_mod, 0, sizeof(udynlink_module_t));
}

// Write a value to the given error pointer only if the pointer isn't NULL
static void write_error(udynlink_error_t *p, udynlink_error_t val) {
    if (p != NULL) {
        *p = val;
    }
}

// Return the entry with the specified index in the given symbol table
// Returns "p_sym" if OK, NULL if index is out of range or an error occured
static udynlink_sym_t *get_sym_at(const udynlink_module_t *p_mod, uint32_t index, udynlink_sym_t *p_sym) {
    uint32_t name_off, info;
    const uint32_t *p_symt = get_sym_table_pointer(p_mod);

    if (index >= *p_symt) { // first word in the symbol table is the number of entries
        return NULL;
    }
    // Read the offset to the name of the symbol and the symbol value
    name_off = p_symt[index * 2 + 1];
    p_sym->val = p_symt[index * 2 + 2];
    // Get symbol type and location
    info = name_off >> UDYNLINK_SYM_INFO_SHIFT;
    p_sym->type = info & UDYNLINK_SYM_INFO_TYPE_MASK;
    p_sym->location = (info & UDYNLINK_SYM_INFO_CODE_MASK) ? UDYNLINK_SYM_LOCATION_CODE : UDYNLINK_SYM_LOCATION_DATA;
    // Sanity check
    if ((index == UDYNLINK_SYM_NAME_OFFSET) && (p_sym->type != UDYNLINK_SYM_TYPE_NAME)) {
        UDYNLINK_DEBUG(UDYNLINK_DEBUG_ERROR, "Module name symbol doesn't have the correct type!\n");
        return NULL;
    }
    // Get name pointer (if available)
    if (p_sym->type != UDYNLINK_SYM_TYPE_LOCAL) { // local symbols don't have names
        p_sym->name = (const char*)p_symt + (name_off & UDYNLINK_SYM_OFFSET_MASK);
    } else {
        p_sym->name = "(N/A)";
    }
    return p_sym;
}

// Offset the given symbol relative to the required base address (.code or .data), based on the symbol location
// The function returns p_sym after it applies the offset to p_sym->val.
static udynlink_sym_t *offset_sym(const udynlink_module_t *p_mod, udynlink_sym_t *p_sym) {
    uint32_t prev_val = p_sym->val;

    if ((p_sym->type == UDYNLINK_SYM_TYPE_LOCAL) || (p_sym->type == UDYNLINK_SYM_TYPE_EXPORTED)) {
        if (p_sym->location == UDYNLINK_SYM_LOCATION_CODE) {
            p_sym->val += (uint32_t)get_code_pointer(p_mod);
        } else {
            p_sym->val += (uint32_t)get_data_pointer(p_mod);
        }
    }
    UDYNLINK_DEBUG(UDYNLINK_DEBUG_INFO, "Symbol %s relocated relative to %s, orig value is %08X, new value is %08X\n", p_sym->name, p_sym->location == UDYNLINK_SYM_LOCATION_CODE ? "code" : "data", prev_val, p_sym->val);
    return p_sym;
}

////////////////////////////////////////////////////////////////////////////////
// Public interface

udynlink_module_t *udynlink_load_module(const void *base_addr, void *load_addr, uint32_t load_size, udynlink_load_mode_t load_mode, udynlink_error_t *p_error) {
    udynlink_module_t *p_mod = NULL;
    void *ram_addr = NULL;
    udynlink_error_t res = UDYNLINK_OK;
    const udynlink_module_header_t *p_header = (const udynlink_module_header_t*)base_addr;
    udynlink_sym_t sym;

    // Find an empty space in the module table
    if((p_mod = get_next_free_module()) == NULL) {
        res = UDYNLINK_ERR_LOAD_NO_MORE_HANDLES;
        goto exit;
    }

    // Setup the module structure. Depending on the copy mode, we might need to rewrite it later.
    p_mod->p_header = p_header;
    UDYNLINK_LOAD_SET_MODE(p_mod, load_mode);

    // Check signature
    if (p_header->sign != UDYNLINK_MODULE_SIGN) {
        res = UDYNLINK_ERR_LOAD_INVALID_SIGN;
        goto exit;
    }

    // Check if a module with a duplicated name already exists
    for (uint32_t i = 0; i < UDYNLINK_MAX_HANDLES; i ++) {
        // Check for other module (not p_mod) that are in use and have the same name as the module being loaded (in p_mod)
        if ((module_table + i != p_mod) && (module_table[i].p_header != NULL) && (!strcmp(udynlink_get_module_name(module_table + i), udynlink_get_module_name(p_mod)))) {
            res = UDYNLINK_ERR_LOAD_DUPLICATE_NAME;
            goto exit;
        }
    }

    UDYNLINK_DEBUG(UDYNLINK_DEBUG_INFO, "Processing module at %p named '%s' with load mode %d\n", base_addr, udynlink_get_module_name(p_mod), (int)load_mode);

    // Allocate RAM or check given RAM region, as needed
    uint32_t ram_size = udynlink_get_ram_size(p_mod);
    if (ram_size > 0) { // is any RAM needed at all?
        if (load_addr == NULL) { // RAM must be allocated
            UDYNLINK_LOAD_CLR_FOREIGN_RAM(p_mod);
            if ((ram_addr = udynlink_external_malloc(ram_size)) == NULL) {
                res = UDYNLINK_ERR_LOAD_OUT_OF_MEMORY;
                goto exit;
            }
            UDYNLINK_DEBUG(UDYNLINK_DEBUG_INFO, "Allocated %u bytes for module at %p\n", ram_size, base_addr);
        } else { // check if the user-provided RAM region is large enough
            UDYNLINK_LOAD_SET_FOREIGN_RAM(p_mod);
            if (load_size < ram_size) {
                res = UDYNLINK_ERR_LOAD_RAM_LEN_LOW;
                goto exit;
            }
            ram_addr = load_addr;
        }
        p_mod->p_ram = ram_addr;
        UDYNLINK_DEBUG(UDYNLINK_DEBUG_INFO, "RAM area for module at %p is at %p (%u bytes)\n", base_addr, ram_addr, ram_size);
    } else {
        p_mod->p_ram = NULL;
        UDYNLINK_DEBUG(UDYNLINK_DEBUG_INFO, "Awesome! Module %p doesn't need any RAM\n", base_addr);
    }

    // Copy to RAM as needed. The first part of RAM is always the LOT, so skip it.
    uint8_t *p_temp8 = (uint8_t*)ram_addr + p_header->num_lot * sizeof(uint32_t);
    // Reuse "load_size" (since it's not used anymore) to hold the offset to code, according to the header.
    load_size = get_code_offset_from_header(p_header);
    if (load_mode == UDYNLINK_LOAD_MODE_COPY_ALL) {
        // We need to copy the whole module to RAM (header, symbol table, relocs, code, data)
        memcpy(p_temp8, base_addr, load_size + p_header->code_size + p_header->data_size);
        UDYNLINK_DEBUG(UDYNLINK_DEBUG_INFO, "Copied module at %p to RAM at %p (%u bytes)\n", base_addr, p_temp8, load_size + p_header->code_size + p_header->data_size);
        // Since we copied everything, move the pointer to the header to RAM, since the original (base_addr) might be freed eventually.
        p_mod->p_header = p_header = (const udynlink_module_header_t*)p_temp8;
    } else if (load_mode == UDYNLINK_LOAD_MODE_COPY_CODE) {
        // Copy just code and data
        memcpy(p_temp8, (const uint8_t*)base_addr + load_size, p_header->code_size + p_header->data_size);
        UDYNLINK_DEBUG(UDYNLINK_DEBUG_INFO, "Copied code and data of module %p to RAM at %p (%u bytes)\n", base_addr, p_temp8, p_header->code_size + p_header->data_size);
    } else {
        // XIP mode: copy only data
        memcpy(p_temp8, (const uint8_t*)base_addr + load_size + p_header->code_size, p_header->data_size);
        UDYNLINK_DEBUG(UDYNLINK_DEBUG_INFO, "Copied data of module %p to RAM at %p (%u bytes)\n", base_addr, p_temp8, p_header->data_size);
    }

    // Zero out BSS
    memset(get_data_pointer(p_mod) + p_header->data_size, 0, p_header->bss_size);

    // Process relocations
    // TODO: find the correct condition for the error "unable to execute in place"
    const uint32_t *p_rels = get_relocs_pointer(p_mod);
    uint32_t *p_lot = (uint32_t*)ram_addr;
    uint32_t *p_data = (uint32_t*)get_data_pointer(p_mod);
    UDYNLINK_DEBUG(UDYNLINK_DEBUG_INFO, "LOT base: %p, .data starts at %p, .code starts at %p\n", p_lot, p_data, get_code_pointer(p_mod));
    // Read and apply each (lot_offset, symt_offset) pair in turn
    for (uint32_t i = 0; i < p_header->num_rels; i ++) {
        uint32_t lot_offset = *p_rels ++;
        uint32_t symt_offset = *p_rels ++;
        if (get_sym_at(p_mod, symt_offset, &sym) == NULL) { // symbol table offset is out of range, shouldn't happen
            res = UDYNLINK_ERR_LOAD_BAD_RELOCATION_TABLE;
            goto exit;
        }
        // Relocations in LOT and .data are encoded in the same way, they can be differentiated based on the value of lot_offset.
        // If lot_offset is larger than or equal to the number of LOT entries, this relocation applies to data, not to LOT.
        uint32_t *p_rel_location = (lot_offset < p_header->num_lot) ? p_lot + lot_offset : p_data + lot_offset - p_header->num_lot;
        switch (sym.type) {
            case UDYNLINK_SYM_TYPE_LOCAL:
            case UDYNLINK_SYM_TYPE_EXPORTED:
                UDYNLINK_DEBUG(UDYNLINK_DEBUG_INFO, "Applying relocation for symbol at index %u, name=%s, type=%d, data_reloc=%d at lot_offset=%u, value=%08X\n", symt_offset, sym.name, sym.type, sym.location, lot_offset, sym.val);
                *p_rel_location = offset_sym(p_mod, &sym)->val;
                break;

            case UDYNLINK_SYM_TYPE_EXTERN:
                UDYNLINK_DEBUG(UDYNLINK_DEBUG_INFO, "Applying extern relocation for symbol at index %u, name=%s at lot_offset=%u\n", symt_offset, sym.name, lot_offset);
                // TODO: this needs a separate step (look in the static symbols of the running program)
                uint32_t sym_addr = udynlink_external_resolve_symbol(sym.name);
                if (sym_addr > 0) {
                    *p_rel_location = sym_addr;
                } else {
                    UDYNLINK_DEBUG(UDYNLINK_DEBUG_ERROR, "Unable to resolve relocation for extern symbol '%s'\n", sym.name);
                    res = UDYNLINK_ERR_LOAD_UNKNOWN_SYMBOL;
                    goto exit;
                }
                break;

            case UDYNLINK_SYM_TYPE_NAME: // no relocations should be emitted against the name of the module
                res = UDYNLINK_ERR_LOAD_BAD_RELOCATION_TABLE;
                goto exit;
        }
    }

    // All done
    UDYNLINK_DEBUG(UDYNLINK_DEBUG_INFO, "Done loading module at %p\n", base_addr);

exit:
    write_error(p_error, res);
    if (res != UDYNLINK_OK) { // there's an error, so cleanup allocated structures and memory
        UDYNLINK_DEBUG(UDYNLINK_DEBUG_ERROR, error_codes[(int)res]);
        if ((p_mod->p_ram != NULL) && !UDYNLINK_LOAD_IS_FOREIGN_RAM(p_mod)) { // free allocated memory
            udynlink_external_free(p_mod->p_ram);
            UDYNLINK_DEBUG(UDYNLINK_DEBUG_INFO, "Deallocated memory area at %p\n", p_mod->p_ram);
        }
        if (p_mod != NULL) { // mark entry in module table as "free"
            mark_module_free(p_mod);
        }
    }
    return res == UDYNLINK_OK ? p_mod : NULL;
}

udynlink_error_t udynlink_unload_module(udynlink_module_t *p_mod) {
    if ((p_mod == NULL) || (p_mod->p_header == NULL)) {
        UDYNLINK_DEBUG(UDYNLINK_DEBUG_ERROR, error_codes[(int)UDYNLINK_ERR_INVALID_MODULE]);
        return UDYNLINK_ERR_INVALID_MODULE;
    }
    UDYNLINK_DEBUG(UDYNLINK_DEBUG_INFO, "Unloading module at %p\n", p_mod);
    if ((p_mod->p_ram != NULL) && !UDYNLINK_LOAD_IS_FOREIGN_RAM(p_mod)) { // free allocated memory
        udynlink_external_free(p_mod->p_ram);
        UDYNLINK_DEBUG(UDYNLINK_DEBUG_INFO, "Deallocated memory area at %p\n", p_mod->p_ram);
    }
    mark_module_free(p_mod);
    return UDYNLINK_OK;
}

uint32_t udynlink_get_ram_size(const udynlink_module_t *p_mod) {
    const udynlink_module_header_t *p_header = p_mod->p_header;
    udynlink_load_mode_t load_mode = UDYNLINK_LOAD_GET_MODE(p_mod);

    // RAM is always needed for relocations, .data and .bss section
    uint32_t tot_size = p_header->num_lot * sizeof(uint32_t) + p_header->data_size + p_header->bss_size;
    // Depending on the copy mode, more RAM might be needed:
    // - if only code is copied, add size of the code
    // - if everything is copied, add the size of the header (including the symbol table and the relocations) and the code
    if (load_mode == UDYNLINK_LOAD_MODE_COPY_CODE) {
        tot_size += p_header->code_size;
    }
    else if (load_mode == UDYNLINK_LOAD_MODE_COPY_ALL) {
        tot_size += get_code_offset_from_header(p_header) + p_header->code_size;
    }
    return tot_size;
}

const char *udynlink_get_module_name(const udynlink_module_t *p_mod) {
    udynlink_sym_t sym;
    udynlink_sym_t *p_sym = get_sym_at(p_mod, UDYNLINK_SYM_NAME_OFFSET, &sym);

    if (p_sym == NULL) {
        return NULL;
    } else {
        return p_sym->name;
    }
}

udynlink_module_t *udynlink_lookup_module(const char *name) {
    for (uint32_t i = 0; i < UDYNLINK_MAX_HANDLES; i ++) {
        if (module_table[i].p_header != NULL) { // there's a module here
            if(!strcmp(name, udynlink_get_module_name(module_table + i))) {
                return module_table + i;
            }
        }
    }
    return NULL;
}

udynlink_sym_t *udynlink_lookup_symbol(const udynlink_module_t *p_mod, const char *name, udynlink_sym_t *p_sym) {
    uint32_t idx;

    for (uint32_t i = 0; i < UDYNLINK_MAX_HANDLES; i ++) { // iterate through all modules
        if ((p_mod == NULL) || (p_mod == module_table + i)) { // but consider only the given one if not NULL
            idx = 0;
            while (get_sym_at(module_table + i, idx ++, p_sym) != NULL) { // iterate through module's symbol table
                if (!strcmp(p_sym->name, name)) { // symbol found
                    return offset_sym(module_table + i, p_sym); // offset value properly before returning
                }
            }
        }
    }
    return NULL;
}

void udynlink_patch_exported_func(const udynlink_module_t *p_mod) {
    uint32_t idx;
    udynlink_sym_t sym;
    for (uint32_t i = 0; i < UDYNLINK_MAX_HANDLES; i ++) { // iterate through all modules
        if ((p_mod == NULL) || (p_mod == module_table + i)) { // but consider only the given one if not NULL
            idx = 0;
            while (get_sym_at(module_table + i, idx ++, &sym) != NULL) { // iterate through module's symbol table
                if (sym.type     == UDYNLINK_SYM_TYPE_EXPORTED &&
                    sym.location == UDYNLINK_SYM_LOCATION_CODE) {
                    offset_sym(module_table + i, &sym);
                    (*(uint32_t*) (sym.val + UDYNLINK_ASM_PROLOGUE_LEN)) = (uint32_t)&udynlink_get_lot_base;
                }
            }
        }
    }
}

uint32_t udynlink_get_symbol_value(const udynlink_module_t *p_mod, const char *name) {
    udynlink_sym_t sym;

    if (udynlink_lookup_symbol(p_mod, name, &sym) == NULL) {
        return 0;
    }
    return sym.val;
}

void udynlink_set_debug_level(udynlink_debug_level_t level) {
    debug_level = level;
}

uint32_t udynlink_get_lot_base(uint32_t pc) {
    udynlink_module_t *p_mod = module_table;
    uint32_t code_base;

    // Iterate through all loaded modules
    for (uint32_t i = 0; i < UDYNLINK_MAX_HANDLES; i ++, p_mod ++) {
        // Consider only loaded modules
        if (p_mod->p_header != NULL) {
            // Check PC limits
            code_base = (uint32_t)get_code_pointer(p_mod);
            if ((code_base <= pc) && (pc < code_base + p_mod->p_header->code_size)) {
                return p_mod->ram_base;
            }
        }
    }
    // Nothing found, so return 0
    // TODO: proper error handling here
    return 0;
}

