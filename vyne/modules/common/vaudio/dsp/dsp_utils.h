#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace VAudioDSP{
    struct WAVHeader {
        char chunkID[4] = {'R', 'I', 'F', 'F'};
        uint32_t chunkSize;
        char format[4] = {'W', 'A', 'V', 'E'};
        char subchunk1ID[4] = {'f', 'm', 't', ' '};
        uint32_t subchunk1Size = 16;
        uint16_t audioFormat = 3; // 3 = IEEE Float
        uint16_t numChannels = 2;
        uint32_t sampleRate = 48000;
        uint32_t byteRate = 48000 * 2 * sizeof(float);
        uint16_t blockAlign = 2 * sizeof(float);
        uint16_t bitsPerSample = 32;
        char subchunk2ID[4] = {'d', 'a', 't', 'a'};
        uint32_t subchunk2Size;
    };
}
