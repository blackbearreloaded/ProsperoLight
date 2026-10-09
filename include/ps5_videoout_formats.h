/* SPDX-License-Identifier: GPL-3.0-or-later
 * Shared scanout semantics from ProsperoLight's hardware-validated presenter.
 * SDR: BGRA8; HDR10: packed R10 G10 B10 A2 (red in the low bits), BT.2020/PQ output tagging.
 */
#pragma once
#include <stdint.h>
#define VIDEO_OUT_PIXEL_FORMAT_SDR UINT64_C(0x8000000000000000)
#define VIDEO_OUT_PIXEL_FORMAT_HDR UINT64_C(0x8100070422000000)

/* HFR and VRR are separate title capabilities. Older declarations include
 * 0x80000 (VRR 120 Hz); firmware 6.02 uses 0x40000 (VRR). Both declare HFR
 * through 0x40. Do not require a VRR bit to offer the fixed 120 Hz mode.
 */
#define PS5_VIDEOOUT_ATTRIBUTE_120HZ 0x40u
#define PS5_VIDEOOUT_ATTRIBUTE_VRR_MASK 0xc0000u
static inline int ps5_videoout_declares_hfr(uint64_t attribute3)
{
    return (attribute3 & PS5_VIDEOOUT_ATTRIBUTE_120HZ) != 0;
}
static inline int ps5_videoout_hfr_accepted(int supported, int configured)
{
    return supported > 0 && configured == 0;
}
