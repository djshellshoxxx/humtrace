// libFuzzer target for the WAV/RF64/BW64 reader. Only std::runtime_error
// (WavError, OperationCancelled) may escape the parser; anything else, or any
// sanitizer report, is a finding.
#include "humtrace/wav.hpp"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    // The first byte steers batch sizes and seeking; the rest is the file.
    const std::uint8_t control = size > 0 ? data[0] : 0;
    const std::string bytes(reinterpret_cast<const char*>(data), size);
    try {
        std::istringstream stream(bytes, std::ios::binary);
        humtrace::CancellationToken token;
        humtrace::WavStreamReader reader(stream, &token);
        const auto frames = reader.metadata().frame_count;
        const std::size_t batch = 1 + (control & 0x3f);
        std::uint64_t samples = 0;
        while (true) {
            const auto out = reader.read_frames(batch);
            if (out.empty())
                break;
            samples += out.size();
        }
        if (samples != frames * reader.metadata().channels)
            __builtin_trap();
        if (frames > 0 && (control & 0x40) != 0) {
            reader.seek_frame(frames / 2);
            (void)reader.read_frames(batch);
        }
        if ((control & 0x80) != 0) {
            token.request_cancel();
            reader.seek_frame(0);
            (void)reader.read_frames(batch);
        }
        std::istringstream again(bytes, std::ios::binary);
        (void)humtrace::decode_wav(again);
    } catch (const std::runtime_error&) {
        // Expected rejection of malformed input or cancellation.
    }
    return 0;
}
