/*
 * ps5-native-app-boilerplate - Minimal target C++ allocation runtime.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Bridges standard C++ allocation operators to the clean-room libc module
 * without introducing exceptions, RTTI, or the complete libc++ runtime.
 */

#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>

extern "C"
{
    void *malloc(std::size_t size);
    void free(void *address);
    int posix_memalign(void **address, std::size_t alignment, std::size_t size);
    void *mmap(void *address, std::size_t length, int protection, int flags, int descriptor,
               long offset);
    int munmap(void *address, std::size_t length);
}

namespace
{
// RmlUi expands compressed button artwork into allocations larger than the
// application libc heap can reliably satisfy. Keep small objects on that heap
// and place bulk buffers in page-backed anonymous mappings.
constexpr std::size_t kMappedAllocationThreshold = 64 * 1024;
constexpr std::size_t kDirectMemoryPageSize = 0x4000;
constexpr std::size_t kAllocationAlignment = 32;
constexpr std::uint64_t kAllocationMagic = UINT64_C(0x50524F535045524F);
constexpr int kProtectionReadWrite = 3;
constexpr int kMapPrivateAnonymous = 0x1002;

struct alignas(kAllocationAlignment) AllocationHeader
{
    std::uint64_t magic;
    std::size_t mapped_size;
};

static_assert(sizeof(AllocationHeader) == kAllocationAlignment);

[[nodiscard]] void *allocate(std::size_t size) noexcept
{
    if (size == 0)
        size = 1;
    if (size > std::numeric_limits<std::size_t>::max() - sizeof(AllocationHeader))
        return nullptr;

    const std::size_t total = sizeof(AllocationHeader) + size;
    AllocationHeader *header = nullptr;
    std::size_t mapped_size = 0;
    if (size >= kMappedAllocationThreshold)
    {
        if (total > std::numeric_limits<std::size_t>::max() - (kDirectMemoryPageSize - 1))
            return nullptr;
        mapped_size = (total + kDirectMemoryPageSize - 1) & ~(kDirectMemoryPageSize - 1);
        void *mapping =
            mmap(nullptr, mapped_size, kProtectionReadWrite, kMapPrivateAnonymous, -1, 0);
        if (mapping != reinterpret_cast<void *>(-1))
            header = static_cast<AllocationHeader *>(mapping);
    }
    else
    {
        void *storage = nullptr;
        if (posix_memalign(&storage, kAllocationAlignment, total) == 0)
            header = static_cast<AllocationHeader *>(storage);
    }
    if (!header)
        return nullptr;

    header->magic = kAllocationMagic;
    header->mapped_size = mapped_size;
    return header + 1;
}

void deallocate(void *address) noexcept
{
    if (!address)
        return;
    auto *header = static_cast<AllocationHeader *>(address) - 1;
    if (header->magic != kAllocationMagic)
    {
        free(address);
        return;
    }
    if (header->mapped_size)
        (void)munmap(header, header->mapped_size);
    else
        free(header);
}

[[nodiscard]] void *allocate_aligned(std::size_t size, std::size_t alignment) noexcept
{
    void *address = nullptr;
    if (alignment < sizeof(void *))
        alignment = sizeof(void *);
    if ((alignment & (alignment - 1)) != 0)
        return nullptr;
    return posix_memalign(&address, alignment, size == 0 ? 1 : size) == 0 ? address : nullptr;
}

[[noreturn]] void allocation_failure() noexcept
{
    __builtin_trap();
}
} // namespace

extern "C" __attribute__((noinline, visibility("hidden"))) bool
ps5ObserveOwnedAllocation(const void *address) noexcept
{
    __asm__ volatile("" : : "r"(address) : "memory");
    return address != nullptr;
}

void *operator new(std::size_t size)
{
    if (void *address = allocate(size))
        return address;
    allocation_failure();
}

void *operator new[](std::size_t size)
{
    return ::operator new(size);
}

void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
    return allocate(size);
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    return allocate(size);
}

void *operator new(std::size_t size, std::align_val_t alignment)
{
    if (void *address = allocate_aligned(size, static_cast<std::size_t>(alignment)))
        return address;
    allocation_failure();
}

void *operator new[](std::size_t size, std::align_val_t alignment)
{
    return ::operator new(size, alignment);
}

void *operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
    return allocate_aligned(size, static_cast<std::size_t>(alignment));
}

void *operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept
{
    return allocate_aligned(size, static_cast<std::size_t>(alignment));
}

void operator delete(void *address) noexcept
{
    deallocate(address);
}

void operator delete[](void *address) noexcept
{
    deallocate(address);
}

void operator delete(void *address, std::size_t) noexcept
{
    deallocate(address);
}

void operator delete[](void *address, std::size_t) noexcept
{
    deallocate(address);
}

void operator delete(void *address, std::align_val_t) noexcept
{
    free(address);
}

void operator delete[](void *address, std::align_val_t) noexcept
{
    free(address);
}

void operator delete(void *address, std::size_t, std::align_val_t) noexcept
{
    free(address);
}

void operator delete[](void *address, std::size_t, std::align_val_t) noexcept
{
    free(address);
}

void operator delete(void *address, const std::nothrow_t &) noexcept
{
    deallocate(address);
}

void operator delete[](void *address, const std::nothrow_t &) noexcept
{
    deallocate(address);
}

void operator delete(void *address, std::align_val_t, const std::nothrow_t &) noexcept
{
    free(address);
}

void operator delete[](void *address, std::align_val_t, const std::nothrow_t &) noexcept
{
    free(address);
}
