/*
 * ps5-native-app-boilerplate - Native application startup.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Initializes the platform C runtime, runs static constructors and main, and
 * hands normal process termination back to the platform runtime.
 */

#include <stddef.h>
#include <stdint.h>

typedef void (*destructor_fn)(void);
typedef void (*initializer_fn)(void);

extern void _init_env(void *process_parameters);
extern int atexit(destructor_fn callback);
extern void exit(int status) __attribute__((noreturn));
extern int main(int argc, char **argv, char **envp);

__attribute__((weak)) void catchReturnFromMain(int status)
{
    (void)status;
}

extern initializer_fn __preinit_array_start[] __attribute__((weak));
extern initializer_fn __preinit_array_end[] __attribute__((weak));
extern initializer_fn __init_array_start[] __attribute__((weak));
extern initializer_fn __init_array_end[] __attribute__((weak));
extern initializer_fn __fini_array_start[] __attribute__((weak));
extern initializer_fn __fini_array_end[] __attribute__((weak));

static void run_forward(initializer_fn *first, initializer_fn *last)
{
    if (first == NULL || last == NULL)
        return;
    while (first != last)
        (*first++)();
}

static void run_reverse(initializer_fn *first, initializer_fn *last)
{
    if (first == NULL || last == NULL)
        return;
    while (last != first)
        (*--last)();
}

void _init(void)
{
    run_forward(__preinit_array_start, __preinit_array_end);
    run_forward(__init_array_start, __init_array_end);
}

void _fini(void)
{
    run_reverse(__fini_array_start, __fini_array_end);
}

__attribute__((noreturn, visibility("default"))) void _start(void *process_parameters,
                                                             destructor_fn loader_teardown)
{
    const int argc = *(const int *)process_parameters;
    char **argv = (char **)((uint8_t *)process_parameters + sizeof(uint64_t));

    _init_env(process_parameters);
    if (loader_teardown != NULL)
        (void)atexit(loader_teardown);
    (void)atexit(_fini);
    _init();
    const int status = main(argc, argv, NULL);
    catchReturnFromMain(status);
    exit(status);
}
