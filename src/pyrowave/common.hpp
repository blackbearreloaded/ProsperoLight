// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
constexpr const char *PYRO_COMMIT = "186f0393b77f7755953b5ecde994bb1cec2e4155";
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 1, 2)))
#endif
void log_line(const char *fmt, ...);
[[noreturn]] void fail(const char *reason);
