#include "stream_parser.h"
#include "ckhr_parser.h"

namespace dedup {

StreamRecord StreamParser::FindStreamInData(const std::vector<uint8_t>& data,
                                             uint32_t streamOffset,
                                             uint64_t originalFileSize) {
    StreamRecord result;
    result.streamOffset = streamOffset;
    result.originalFileSize = originalFileSize;

    result.chunks = CkhrParser::ParseAtOffset(data.data(), data.size(), streamOffset);
    return result;
}

} // namespace dedup