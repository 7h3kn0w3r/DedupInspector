#pragma once

#include <cstdint>
#include <vector>
#include "dedup_types.h"

namespace dedup {

class CkhrParser {
public:
    // Parses the Ckhr/Smap chunk map starting at `streamOffset` within the Stream container data.
    static std::vector<ChunkHashRecord> ParseAtOffset(const uint8_t* streamData,
                                                      size_t streamDataSize,
                                                      uint32_t streamOffset);

private:
    // 'C' 'k' 'h' 'r' magic (0x72686B43 LE) and 'C' 'K' 'H' 'R' magic (0x52484B43 LE)
    static constexpr uint32_t kCkhrMagic = 0x72686B43;
    static constexpr uint32_t kCkhrMagicUpper = 0x52484B43;
};

} // namespace dedup
