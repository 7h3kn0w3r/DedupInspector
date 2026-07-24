#pragma once

#include <cstdint>
#include <vector>
#include "dedup_types.h"

namespace dedup {

class StreamParser {
public:
    // Locates and parses the stream metadata (Ckhr block and chunk references)
    // for a deduplicated file given its streamOffset inside the Stream container data.
    static StreamRecord FindStreamInData(const std::vector<uint8_t>& data,
                                          uint32_t streamOffset,
                                          uint64_t originalFileSize);
};

} // namespace dedup