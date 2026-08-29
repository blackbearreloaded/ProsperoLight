/*
 * ps5-native-app-boilerplate / ProsperoLight - Physical USB input mapping.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MOONLIGHT_PHYSICAL_INPUT_HPP
#define MOONLIGHT_PHYSICAL_INPUT_HPP

#include <stdint.h>

namespace prosperolight::physical_input
{
constexpr uint32_t kLeftControl = 0x01;
constexpr uint32_t kLeftShift = 0x02;
constexpr uint32_t kLeftAlt = 0x04;
constexpr uint32_t kLeftMeta = 0x08;
constexpr uint32_t kRightControl = 0x10;
constexpr uint32_t kRightShift = 0x20;
constexpr uint32_t kRightAlt = 0x40;
constexpr uint32_t kRightMeta = 0x80;

constexpr uint8_t kModifierShift = 0x01;
constexpr uint8_t kModifierControl = 0x02;
constexpr uint8_t kModifierAlt = 0x04;
constexpr uint8_t kModifierMeta = 0x08;
constexpr uint8_t kNonNormalized = 0x01;

struct KeyMapping
{
    uint16_t virtual_key;
    uint8_t flags;

    constexpr explicit operator bool() const
    {
        return virtual_key != 0;
    }
};

constexpr KeyMapping MapKey(uint16_t usage)
{
    if (usage >= 4 && usage <= 29)
        return {static_cast<uint16_t>(0x41 + usage - 4), 0};
    if (usage >= 30 && usage <= 38)
        return {static_cast<uint16_t>(0x31 + usage - 30), 0};
    if (usage >= 58 && usage <= 69)
        return {static_cast<uint16_t>(0x70 + usage - 58), 0};
    if (usage >= 89 && usage <= 97)
        return {static_cast<uint16_t>(0x61 + usage - 89), 0};
    if (usage >= 104 && usage <= 115)
        return {static_cast<uint16_t>(0x7c + usage - 104), 0};

    switch (usage)
    {
    case 39:
        return {0x30, 0};
    case 40:
    case 88:
        return {0x0d, 0};
    case 41:
        return {0x1b, 0};
    case 42:
        return {0x08, 0};
    case 43:
        return {0x09, 0};
    case 44:
        return {0x20, 0};
    case 45:
        return {0xbd, 0};
    case 46:
        return {0xbb, 0};
    case 47:
        return {0xdb, 0};
    case 48:
    case 49:
        return {0xdc, 0};
    case 51:
        return {0xba, 0};
    case 52:
        return {0xde, 0};
    case 53:
        return {0xc0, 0};
    case 54:
        return {0xbc, 0};
    case 55:
        return {0xbe, 0};
    case 56:
        return {0xbf, 0};
    case 57:
        return {0x14, 0};
    case 70:
        return {0x2c, 0};
    case 71:
        return {0x91, 0};
    case 72:
        return {0x13, 0};
    case 73:
        return {0x2d, 0};
    case 74:
        return {0x24, 0};
    case 75:
        return {0x21, 0};
    case 76:
        return {0x2e, 0};
    case 77:
        return {0x23, 0};
    case 78:
        return {0x22, 0};
    case 79:
        return {0x27, 0};
    case 80:
        return {0x25, 0};
    case 81:
        return {0x28, 0};
    case 82:
        return {0x26, 0};
    case 83:
        return {0x90, 0};
    case 84:
        return {0x6f, 0};
    case 85:
        return {0x6a, 0};
    case 86:
        return {0x6d, 0};
    case 87:
        return {0x6b, 0};
    case 98:
        return {0x60, 0};
    case 99:
        return {0x6e, 0};
    case 100:
    case 135:
        return {0xe2, kNonNormalized};
    case 101:
        return {0x5d, 0};
    case 116:
        return {0x2b, 0};
    case 117:
        return {0x2f, 0};
    case 119:
        return {0x29, 0};
    case 137:
        return {0xdc, kNonNormalized};
    case 144:
        return {0x1c, 0};
    case 145:
        return {0x1d, 0};
    case 224:
        return {0xa2, 0};
    case 225:
        return {0xa0, 0};
    case 226:
        return {0xa4, 0};
    case 227:
        return {0x5b, 0};
    case 228:
        return {0xa3, 0};
    case 229:
        return {0xa1, 0};
    case 230:
        return {0xa5, 0};
    case 231:
        return {0x5c, 0};
    case 268:
        return {0xac, 0};
    case 269:
        return {0xaa, 0};
    case 270:
        return {0xab, 0};
    case 271:
        return {0xa8, 0};
    case 272:
        return {0xa9, 0};
    case 273:
        return {0xa7, 0};
    case 274:
        return {0xa6, 0};
    default:
        return {};
    }
}

constexpr uint8_t MoonlightModifiers(uint32_t modifiers)
{
    uint8_t result = 0;

    if (modifiers & (kLeftShift | kRightShift))
        result |= kModifierShift;
    if (modifiers & (kLeftControl | kRightControl))
        result |= kModifierControl;
    if (modifiers & (kLeftAlt | kRightAlt))
        result |= kModifierAlt;
    if (modifiers & (kLeftMeta | kRightMeta))
        result |= kModifierMeta;
    return result;
}

constexpr uint16_t ModifierUsage(uint32_t bit)
{
    switch (bit)
    {
    case kLeftControl:
        return 224;
    case kLeftShift:
        return 225;
    case kLeftAlt:
        return 226;
    case kLeftMeta:
        return 227;
    case kRightControl:
        return 228;
    case kRightShift:
        return 229;
    case kRightAlt:
        return 230;
    case kRightMeta:
        return 231;
    default:
        return 0;
    }
}

constexpr int16_t ClampMotion(int32_t value)
{
    return value > INT16_MAX ? INT16_MAX : value < INT16_MIN ? INT16_MIN
                                                               : static_cast<int16_t>(value);
}

constexpr int8_t ClampScroll(int32_t value)
{
    return value > INT8_MAX ? INT8_MAX : value < -INT8_MAX ? -INT8_MAX
                                                        : static_cast<int8_t>(value);
}
} // namespace prosperolight::physical_input

#endif
