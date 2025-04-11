import re

PATH="/usr/local/cuda/include/nvml.h"
func = ["nvmlInit",
        "nvmlDeviceGetSupportedEventTypes",
        "nvmlDeviceRegisterEvents",
        "nvmlEventSetCreate",
        "nvmlEventSetWait",
        "nvmlDeviceGetCount",
        "nvmlDeviceGetHandleByIndex",
        "nvmlDeviceGetPciInfo",
        "nvmlEventSetFree",
        "nvmlShutdown"]

pattern = re.compile(
        r'^nvmlReturn_t DECLDIR\s+({})(\(.*?\));'.format('|'.join(map(re.escape, func))),
        flags=re.MULTILINE
)

type_pattern = re.compile(
        r'^#define\s+nvmlEventType(\w+)\s+0x.*',
        flags=re.MULTILINE
)

with open(PATH, 'r') as file:
        content = file.read()
        matched_lines = pattern.findall(content)
        type_lines = type_pattern.findall(content)

func_declares = []
func_defs = []
func_inits = []
type_strs = []

for match in matched_lines:
        func_declares.append('typedef nvmlReturn_t (*my_{}_p){};'.format(match[0], match[1]))
        func_defs.append('my_{}_p my_{};'.format(match[0], match[0]))
        func_inits.append('my_{0} = (my_{0}_p)dlsym(handle, "{0}"); \
                                \n\tif (!my_{0}) {{ \
                                \n\t\tprintf(\"Failed to load {0}: %s\\n\", dlerror()); \
                                \n\t\treturn -1; \
                                \n\t}}'.format(match[0]))

for type_line in type_lines:
        type_strs.append('case nvmlEventType{}: return \"{}\";'.format(type_line, type_line))

print('''
/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Copyright (C) 2025 Alibaba Inc
 */

'''
)
print('#include <dlfcn.h>\
        \n#include <stdio.h>\
        \n#include "/usr/local/cuda/include/nvml.h"')
print('\ntypedef const char* (*my_nvmlErrorString_p)(nvmlReturn_t result);')
print('\n'.join(func_declares))
print('\nmy_nvmlErrorString_p my_nvmlErrorString;')
print('\n'.join(func_defs))
print('\nstatic int my_nvml_setup(void* handle) \n{{\n\t{}{}\n\treturn 0;\n}}'.format('\n\t'.join(func_inits),
        '\n\tmy_nvmlErrorString = (my_nvmlErrorString_p)dlsym(handle, "nvmlErrorString"); \
         \n\tif (!my_nvmlErrorString) { \
         \n\t\tprintf(\"Failed to load nvmlErrorString: %s\\n\", dlerror()); \
         \n\t\treturn -1; \
         \n\t}'))
print('\nstatic const char* my_nvmlEventTypeString(unsigned long long type) \n{{ \
        \n\n\tswitch (type) {{ \
        \n\t{} \
        \n\tdefault: return \"Unknown\"; \
        \n\t}} \
        \n\treturn \"Unknown\"; \
        \n}}'.format('\n\t'.join(type_strs)))


