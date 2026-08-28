/*
 * ps5-native-app-boilerplate - ProsperoLight allocation runtime tests.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <new>

namespace
{
constexpr std::size_t kPageSize = 0x4000;

bool is_aligned(const void *address, std::size_t alignment)
{
    return reinterpret_cast<std::uintptr_t>(address) % alignment == 0;
}

bool verify_default_allocation(std::size_t size)
{
    void *address = ::operator new(size, std::nothrow);
    const bool valid = address && is_aligned(address, 32);
    ::operator delete(address, std::nothrow);
    if (!valid)
        std::fprintf(stderr, "default allocation failed: size=%zu address=%p\n", size, address);
    return valid;
}

struct alignas(64) OverAlignedObject
{
    std::byte bytes[64];
};
} // namespace

// The target runtime uses PS5 anonymous mappings for large objects. Replace
// that boundary with page-aligned host storage so this test exercises the same
// allocation bookkeeping without depending on host-specific mmap flags.
extern "C" void *mmap(void *, std::size_t length, int, int, int, long)
{
    void *address = nullptr;
    return posix_memalign(&address, kPageSize, length) == 0 ? address
                                                            : reinterpret_cast<void *>(-1);
}

extern "C" int munmap(void *address, std::size_t)
{
    std::free(address);
    return 0;
}

int main()
{
    bool valid = true;
    for (const std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{31}, std::size_t{32},
                                   std::size_t{65535}, std::size_t{65536}, std::size_t{806880}})
    {
        valid = verify_default_allocation(size) && valid;
    }

    auto *object = new (std::nothrow) OverAlignedObject;
    const bool over_aligned = object && is_aligned(object, alignof(OverAlignedObject));
    delete object;
    if (!over_aligned)
        std::fprintf(stderr, "over-aligned allocation failed: address=%p\n",
                     static_cast<void *>(object));

    return valid && over_aligned ? EXIT_SUCCESS : EXIT_FAILURE;
}
