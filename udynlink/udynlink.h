/* ARM Cortex-M micro dynamic linker (udynlink) public interface.
 *
 * Copyright (c) 2016 Bogdan Marinescu
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UDYNLINK_H__
#define __UDYNLINK_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// Data structures and macros

// Module header structure
typedef struct {
    uint32_t sign;                              // module signature
    //uint16_t mod_version;                       // module version (major, minor)
    //uint16_t udynlink_version;                  // version of udynlink used to compile module (major, minor)
    uint16_t num_lot;                           // number of LOT entries
    uint16_t num_rels;                          // number of relocations
    uint32_t symt_size;                         // size of symbol table in bytes
    uint32_t code_size;                         // size of code section in bytes
    uint32_t data_size;                         // size of data section in bytes
    uint32_t bss_size;                          // size of bss section in bytes
    // Then relocations (num_rels * 8 bytes)
    // Then the symbol table (symt_size bytes, rounded up to 4)
    // Then the code (rounded up to a multiple of 4 bytes)
    // Then data
} udynlink_module_header_t;

// Copy mode for udynlink_load_module_copy: copy everything, copy without the header (just code and
// data) or execute in place (copy only the data).
typedef enum {
    UDYNLINK_LOAD_MODE_COPY_ALL,
    _UDYNLINK_LOAD_MODE_FIRST = UDYNLINK_LOAD_MODE_COPY_ALL, // for testing only
    UDYNLINK_LOAD_MODE_COPY_CODE,
    UDYNLINK_LOAD_MODE_XIP,
    _UDYNLINK_LOAD_MODE_LAST = UDYNLINK_LOAD_MODE_XIP // for testing only
} udynlink_load_mode_t;

// Representation of a loaded module in memory
typedef struct _udynlink_module_t {
    const udynlink_module_header_t *p_header;   // pointer to module header
    union {
        void *p_ram;                            // pointer to module RAM
        uint32_t ram_base;                      // same thing as a number
    };
    uint8_t info;                               // load mode (above) and RAM ownserhsip info
} udynlink_module_t;

// A symbol (mapping between a name and a value). Symbols can be both functions and
// variables and can live in both the code region or the memory region.
#define UDYNLINK_SYM_TYPE_LOCAL               0   // static (module local) symbol
#define UDYNLINK_SYM_TYPE_EXPORTED            1   // exported symbol
#define UDYNLINK_SYM_TYPE_EXTERN              2   // extern (unknown to the module) symbol
#define UDYNLINK_SYM_TYPE_NAME                3   // this is the name of the module

#define UDYNLINK_SYM_LOCATION_CODE            0   // symbol in code section
#define UDYNLINK_SYM_LOCATION_DATA            1   // symbol in data section

typedef struct {
    const char *name;                           // symbol name (NULL for local symbols)
    uint32_t val;                               // symbol value
    uint8_t type;                               // type (see above)
    uint8_t location;                           // location (see above)
} udynlink_sym_t;

// Error codes returned by various functions
#define UDYNLINK_ERROR_CODES \
_UDYNLINK_EXPAND(UDYNLINK_OK),\
_UDYNLINK_EXPAND(UDYNLINK_ERR_LOAD_INVALID_SIGN),\
_UDYNLINK_EXPAND(UDYNLINK_ERR_LOAD_RAM_LEN_LOW),\
_UDYNLINK_EXPAND(UDYNLINK_ERR_LOAD_OUT_OF_MEMORY),\
_UDYNLINK_EXPAND(UDYNLINK_ERR_LOAD_UNABLE_TO_XIP),\
_UDYNLINK_EXPAND(UDYNLINK_ERR_LOAD_NO_MORE_HANDLES),\
_UDYNLINK_EXPAND(UDYNLINK_ERR_LOAD_INVALID_MODE),\
_UDYNLINK_EXPAND(UDYNLINK_ERR_LOAD_BAD_RELOCATION_TABLE),\
_UDYNLINK_EXPAND(UDYNLINK_ERR_LOAD_UNKNOWN_SYMBOL),\
_UDYNLINK_EXPAND(UDYNLINK_ERR_LOAD_DUPLICATE_NAME),\
_UDYNLINK_EXPAND(UDYNLINK_ERR_INVALID_MODULE)

#define _UDYNLINK_EXPAND(x)                   x
typedef enum {
    UDYNLINK_ERROR_CODES
} udynlink_error_t;
#undef _UDYNLINK_EXPAND

// Debug levels for udynlink_debug (order is important!)
// If this enum is modified, the correponding array in udynlink_debug must also be modified!
typedef enum {
    UDYNLINK_DEBUG_NONE,
    UDYNLINK_DEBUG_ERROR,
    UDYNLINK_DEBUG_WARNING,
    UDYNLINK_DEBUG_INFO
} udynlink_debug_level_t;

// Convenience macros
#define UDYNLINK_MAKE_VERSION(major, minor)   (((major) << 8) | (minor))
#define UDYNLINK_GET_MAJOR_VERSION(v)         (((v) >> 16) & 0xFF)
#define UDYNLINK_GET_MINOR_VERSION(v)         ((v) & 0xFF)

#define UDYNLINK_DEBUG(...)                   udynlink_debug(__func__, __LINE__, __VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////
// Public interface

// Loads a module.
// base_addr - the start address of the module in memory.
// load_addr - the address where the module will be loaded. It can be:
//     - NULL to tell the function to allocate RAM and load the module there.
//     - a RAM address where the module will be loaded.
// load_size - if "load_addr" is not NULL, the size of the memory region allocated at "load_addr".
// load_mode - specifies how the module at "base_addr" will be loaded to memory.
// p_error - pointer to an int where the error result of the function will be written (can be NULL).
// Returns a pointer to the module handle, or NULL for error.
// p_error is filled with the error code.
udynlink_module_t *udynlink_load_module(const void *base_addr, void *load_addr, uint32_t load_size, udynlink_load_mode_t load_mode, udynlink_error_t *p_error);

// Unloads the specified module. Returns the status of the unload operation.
udynlink_error_t udynlink_unload_module(udynlink_module_t *p_mod);

// Return the RAM space required by the module.
// This contains the LOT relocations + .data + .bss (+.text if the module was loaded with udynlink_load_module_copy).
uint32_t udynlink_get_ram_size(const udynlink_module_t *p_mod);

// Returns the name of the given module.
const char *udynlink_get_module_name(const udynlink_module_t *p_mod);

// Lookup the given module by name.
// Returns NULL if the module is not found.
udynlink_module_t *udynlink_lookup_module(const char *name);

// Lookup the given symbol. Writes the result in p_sym.
// p_module - pointer to the module to search, or NULL to search in all modules.
// name - name of the symbol
// p_sym - structure to fill with information about the symbol
// Returns p_sym if the symbol is found, false otherwise.
udynlink_sym_t *udynlink_lookup_symbol(const udynlink_module_t *p_mod, const char *name, udynlink_sym_t *p_sym);

// patch the exported functions with the address of the udynlink_get_lot_base function
void udynlink_patch_exported_func(const udynlink_module_t *p_mod);

// Lookup the given symbol. Returns its value if found, 0 otherwise.
uint32_t udynlink_get_symbol_value(const udynlink_module_t *p_mod, const char *name);

// Sets the debug level.
// level - the debug level (see udynlink_debug_level_t above)
void udynlink_set_debug_level(udynlink_debug_level_t level);

// Return the LOT address for the function at the given address
uint32_t udynlink_get_lot_base(uint32_t pc);

#ifdef __cplusplus
}
#endif

#endif // #ifndef __UDYNLINK_H__

