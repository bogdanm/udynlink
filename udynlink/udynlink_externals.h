/* ARM Cortex-M micro dynamic linker (udynlink) external function prototypes.
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

#ifndef __UDYNLINK_EXTERNALS_H__
#define __UDYNLINK_EXTERNALS_H__

#include <stddef.h>
#include <stdarg.h>

////////////////////////////////////////////////////////////////////////////////
// These functions must be implemented by a program that uses udynlink

int udynlink_external_is_pointer_in_ram(const void *p);
void *udynlink_external_malloc(size_t size);
void udynlink_external_free(void *p);
void udynlink_external_vprintf(const char *s, va_list va);
uint32_t udynlink_external_resolve_symbol(const char *name);

// UDYNLINK_MAX_HANDLES
//     >0: that many modules

#endif // #ifndef __UDYNLINK_EXTERNALS_H__

