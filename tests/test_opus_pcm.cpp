/*
 * ps5-native-app-boilerplate / ProsperoLight - Scalar/SIMD Opus comparison.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "opus_multistream.h"

int main(int argc, char **argv)
{
    assert(argc == 5);
    const bool encode = strcmp(argv[1], "encode") == 0;
    const int channels = atoi(argv[2]);
    assert(channels == 2 || channels == 6);
    const int streams = channels == 2 ? 1 : 4;
    const int coupled = channels == 2 ? 1 : 2;
    const unsigned char stereo[] = {0, 1};
    const unsigned char surround[] = {0, 4, 1, 2, 3, 5};
    const auto *mapping = channels == 2 ? stereo : surround;
    int error = 0;
    OpusMSEncoder *encoder =
        encode ? opus_multistream_encoder_create(48000, channels, streams, coupled, mapping,
                                                 OPUS_APPLICATION_RESTRICTED_LOWDELAY, &error)
               : nullptr;
    assert(!encode || (encoder && error == OPUS_OK));
    OpusMSDecoder *decoder = !encode ? opus_multistream_decoder_create(48000, channels, streams,
                                                                       coupled, mapping, &error)
                                     : nullptr;
    assert(encode || (decoder && error == OPUS_OK));
    FILE *packets = fopen(argv[3], encode ? "wb" : "rb");
    FILE *pcm_output = encode ? nullptr : fopen(argv[4], "wb");
    assert(packets && (encode || pcm_output));
    std::array<opus_int16, 480 * 6> pcm{};
    std::array<unsigned char, 8192> packet{};
    uint64_t cursor = 0;
    for (unsigned n = 0; n < 300; ++n)
    {
        const int frames = n < 150 ? 240 : 480; // Both 5 ms and 10 ms, stereo and 5.1.
        int length = 0;
        if (encode)
        {
            for (int f = 0; f < frames; ++f)
                for (int c = 0; c < channels; ++c)
                    pcm[f * channels + c] = static_cast<opus_int16>(
                        12000 * sin(6.283185307179586 * (cursor + f) * (120 + c * 173) / 48000.0));
            length =
                opus_multistream_encode(encoder, pcm.data(), frames, packet.data(), packet.size());
            assert(length > 0);
            // Exercise decoder PLC and subsequent state recovery on an identical corpus.
            if (n % 41 == 40)
                length = 0;
            assert(fwrite(&length, sizeof(length), 1, packets) == 1);
            assert(fwrite(packet.data(), 1, length, packets) == static_cast<size_t>(length));
            cursor += frames;
        }
        else
        {
            assert(fread(&length, sizeof(length), 1, packets) == 1);
            assert(length >= 0 && static_cast<size_t>(length) <= packet.size());
            assert(fread(packet.data(), 1, length, packets) == static_cast<size_t>(length));
            const int decoded = opus_multistream_decode(decoder, length ? packet.data() : nullptr,
                                                        length, pcm.data(), frames, 0);
            assert(decoded == frames);
            assert(fwrite(pcm.data(), sizeof(opus_int16), frames * channels, pcm_output) ==
                   static_cast<size_t>(frames * channels));
        }
    }
    assert(fclose(packets) == 0);
    if (pcm_output)
        assert(fclose(pcm_output) == 0);
    if (encoder)
        opus_multistream_encoder_destroy(encoder);
    if (decoder)
        opus_multistream_decoder_destroy(decoder);
}
