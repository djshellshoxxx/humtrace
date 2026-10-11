#include "humtrace/wav.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void u16(std::string& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<char>(value & 0xff));
    bytes.push_back(static_cast<char>((value >> 8) & 0xff));
}
void u32(std::string& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<char>((value >> shift) & 0xff));
}
void tag(std::string& bytes, const char* value) { bytes.append(value, 4); }
std::string pcm16_wav() {
    std::string body;
    tag(body, "fmt "); u32(body, 16); u16(body, 1); u16(body, 1);
    u32(body, 8000); u32(body, 16000); u16(body, 2); u16(body, 16);
    tag(body, "data"); u32(body, 4); u16(body, 16384); u16(body, 0x8000);
    std::string result;
    tag(result, "RIFF"); u32(result, static_cast<std::uint32_t>(body.size() + 4));
    tag(result, "WAVE"); result += body;
    return result;
}
std::string pcm24_wav() {
    std::string body;
    tag(body, "fmt "); u32(body, 16); u16(body, 1); u16(body, 1);
    u32(body, 48000); u32(body, 144000); u16(body, 3); u16(body, 24);
    tag(body, "data"); u32(body, 3); body.append("\0\0\x80", 3); body.push_back('\0');
    std::string result;
    tag(result, "RIFF"); u32(result, static_cast<std::uint32_t>(body.size() + 4));
    tag(result, "WAVE"); result += body;
    return result;
}
std::string float32_wav() {
    std::string body;
    tag(body, "fmt "); u32(body, 16); u16(body, 3); u16(body, 1);
    u32(body, 48000); u32(body, 192000); u16(body, 4); u16(body, 32);
    tag(body, "data"); u32(body, 4); body.append("\0\0\x80\x3e", 4);
    std::string result;
    tag(result, "RIFF"); u32(result, static_cast<std::uint32_t>(body.size() + 4));
    tag(result, "WAVE"); result += body;
    return result;
}
std::string pcm16_multiple_data_chunks_wav() {
    std::string body;
    tag(body, "fmt "); u32(body, 16); u16(body, 1); u16(body, 1);
    u32(body, 8000); u32(body, 16000); u16(body, 2); u16(body, 16);
    tag(body, "data"); u32(body, 2); u16(body, 16384);
    tag(body, "data"); u32(body, 2); u16(body, 0x8000);
    std::string result;
    tag(result, "RIFF"); u32(result, static_cast<std::uint32_t>(body.size() + 4));
    tag(result, "WAVE"); result += body;
    return result;
}
struct ExtensibleFormat {
    std::uint16_t channels{2};
    std::uint16_t container_bits{24};
    std::uint16_t valid_bits{20};
    std::uint32_t channel_mask{0x3};
    std::uint16_t subtype{1};
    std::uint16_t cb_size{22};
    std::uint32_t fmt_length{40};
};
std::string extensible_wav(const ExtensibleFormat& format, const std::string& data) {
    const auto block_align = static_cast<std::uint16_t>(format.channels * (format.container_bits / 8));
    std::string body;
    tag(body, "fmt "); u32(body, format.fmt_length); u16(body, 0xFFFE); u16(body, format.channels);
    u32(body, 48000); u32(body, 48000U * block_align); u16(body, block_align); u16(body, format.container_bits);
    u16(body, format.cb_size); u16(body, format.valid_bits); u32(body, format.channel_mask);
    u16(body, format.subtype); u16(body, 0); u16(body, 0); u16(body, 0x10);
    body.append("\x80\x00\x00\xAA\x00\x38\x9B\x71", 8);
    body.resize(8 + format.fmt_length, '\0');
    tag(body, "data"); u32(body, static_cast<std::uint32_t>(data.size())); body += data;
    if (data.size() % 2 != 0) body.push_back('\0');
    std::string result;
    tag(result, "RIFF"); u32(result, static_cast<std::uint32_t>(body.size() + 4));
    tag(result, "WAVE"); result += body;
    return result;
}
std::string pcm16_two_format_chunks_wav() {
    std::string body;
    for (int copy = 0; copy < 2; ++copy) {
        tag(body, "fmt "); u32(body, 16); u16(body, 1); u16(body, 1);
        u32(body, 8000); u32(body, 16000); u16(body, 2); u16(body, 16);
    }
    tag(body, "data"); u32(body, 2); u16(body, 0);
    std::string result;
    tag(result, "RIFF"); u32(result, static_cast<std::uint32_t>(body.size() + 4));
    tag(result, "WAVE"); result += body;
    return result;
}
// Generic builder for hardening tests: chunks with optional size overrides.
struct Chunk {
    std::string id;
    std::string payload;
    std::optional<std::uint32_t> declared_size{};
    bool pad{true};
};
std::string fmt_payload(std::uint16_t format, std::uint16_t channels, std::uint32_t rate, std::uint16_t bits,
                        std::optional<std::uint16_t> block_align = std::nullopt) {
    std::string p;
    const auto align = block_align.value_or(static_cast<std::uint16_t>(channels * (bits / 8)));
    u16(p, format); u16(p, channels); u32(p, rate); u32(p, rate * align); u16(p, align); u16(p, bits);
    return p;
}
std::string riff(const std::vector<Chunk>& chunks, std::optional<std::uint32_t> riff_size = std::nullopt) {
    std::string body;
    for (const auto& chunk : chunks) {
        body += chunk.id;
        u32(body, chunk.declared_size.value_or(static_cast<std::uint32_t>(chunk.payload.size())));
        body += chunk.payload;
        if (chunk.pad && chunk.payload.size() % 2 != 0) body.push_back('\0');
    }
    std::string result;
    tag(result, "RIFF"); u32(result, riff_size.value_or(static_cast<std::uint32_t>(body.size() + 4)));
    tag(result, "WAVE"); result += body;
    return result;
}
std::string error_of(const std::string& bytes, std::string* stage = nullptr) {
    std::istringstream stream(bytes, std::ios::binary);
    try { (void)humtrace::decode_wav(stream); }
    catch (const humtrace::WavError& error) { if (stage) *stage = error.stage(); return error.what(); }
    catch (const std::runtime_error& error) { if (stage) stage->clear(); return error.what(); }
    return {};
}
std::string pcm16_samples(std::initializer_list<std::uint16_t> values) {
    std::string s; for (auto v : values) u16(s, v); return s;
}
bool decode_throws(const std::string& bytes) {
    std::istringstream stream(bytes, std::ios::binary);
    try { (void)humtrace::decode_wav(stream); }
    catch (const std::runtime_error&) { return true; }
    return false;
}
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void decodes_pcm16_metadata_and_samples() {
    std::istringstream stream(pcm16_wav(), std::ios::binary);
    const auto audio = humtrace::decode_wav(stream);
    require(audio.sample_rate_hz == 8000, "sample rate is preserved");
    require(audio.channels == 1, "channel count is preserved");
    require(audio.bit_depth == 16, "bit depth is preserved");
    require(audio.interleaved_samples.size() == 2, "sample frames are decoded");
    require(audio.interleaved_samples[0] > 0.49 && audio.interleaved_samples[0] < 0.51,
            "positive PCM sample is normalized");
    require(audio.interleaved_samples[1] == -1.0, "minimum PCM sample is normalized");
}
void rejects_non_wave_input() {
    std::istringstream stream("NOPE", std::ios::binary);
    bool threw = false;
    try { (void)humtrace::decode_wav(stream); }
    catch (const std::runtime_error&) { threw = true; }
    require(threw, "invalid container is rejected");
}
void decodes_signed_24_bit_minimum() {
    std::istringstream stream(pcm24_wav(), std::ios::binary);
    const auto audio = humtrace::decode_wav(stream);
    require(audio.bit_depth == 24, "24-bit depth is preserved");
    require(audio.interleaved_samples.size() == 1, "odd-sized data chunk padding is skipped");
    require(audio.interleaved_samples[0] == -1.0, "24-bit sign extension is correct");
}
void decodes_ieee_float32_samples() {
    std::istringstream stream(float32_wav(), std::ios::binary);
    const auto audio = humtrace::decode_wav(stream);
    require(audio.format_code == 3, "IEEE float format code is preserved");
    require(audio.bit_depth == 32, "float bit depth is preserved");
    require(audio.interleaved_samples.size() == 1, "one float sample is decoded");
    require(std::abs(audio.interleaved_samples[0] - 0.25) < 1e-7,
            "little-endian IEEE float sample is decoded");
}
void streaming_reader_returns_bounded_frame_batches() {
    std::istringstream stream(pcm16_wav(), std::ios::binary);
    humtrace::WavStreamReader reader(stream);
    require(reader.metadata().frame_count == 2, "stream reader exposes total frame count");
    const auto first = reader.read_frames(1);
    const auto second = reader.read_frames(1);
    const auto end = reader.read_frames(8);
    require(first.size() == 1 && second.size() == 1, "reader respects requested frame batch size");
    require(first[0] > 0.49 && first[0] < 0.51 && second[0] == -1.0,
            "streaming sample order and normalization are preserved");
    require(end.empty(), "reader returns an empty batch at end of audio");
}
void streaming_reader_continues_across_data_chunks() {
    std::istringstream stream(pcm16_multiple_data_chunks_wav(), std::ios::binary);
    humtrace::WavStreamReader reader(stream);
    const auto samples = reader.read_frames(8);
    require(samples.size() == 2, "frames across multiple data chunks are returned together");
    require(samples[0] > 0.49 && samples[0] < 0.51 && samples[1] == -1.0,
            "sample order is retained across data chunk boundaries");
}
void decodes_extensible_pcm_with_valid_bits_and_mask() {
    // 20 valid bits left-justified in a 24-bit container: left = +0.5, right = minimum.
    const std::string data("\x00\x00\x40\x00\x00\x80", 6);
    std::istringstream stream(extensible_wav({}, data), std::ios::binary);
    const auto audio = humtrace::decode_wav(stream);
    require(audio.extensible, "extensible header is reported");
    require(audio.format_code == 1, "PCM subtype GUID maps to integer PCM");
    require(audio.bit_depth == 24 && audio.valid_bits == 20, "container width and valid bits are kept apart");
    require(audio.channel_mask == 0x3, "channel mask is preserved as written");
    require(audio.interleaved_samples.size() == 2 && audio.interleaved_samples[0] == 0.5 &&
                audio.interleaved_samples[1] == -1.0,
            "left-justified extensible PCM normalizes by container width");
}
void decodes_extensible_float_with_unspecified_layout() {
    ExtensibleFormat format;
    format.channels = 3; format.container_bits = 32; format.valid_bits = 32;
    format.channel_mask = 0; format.subtype = 3;
    const std::string data("\0\0\x80\x3e\0\0\x80\xbe\0\0\0\0", 12);
    std::istringstream stream(extensible_wav(format, data), std::ios::binary);
    const auto audio = humtrace::decode_wav(stream);
    require(audio.format_code == 3 && audio.channels == 3, "float subtype GUID maps to IEEE float");
    require(audio.channel_mask == 0, "an unspecified speaker layout is not guessed");
    require(audio.interleaved_samples.size() == 3 && audio.interleaved_samples[0] == 0.25 &&
                audio.interleaved_samples[1] == -0.25 && audio.interleaved_samples[2] == 0.0,
            "extensible float samples are decoded");
}
void legacy_formats_report_full_valid_bits_and_no_mask() {
    std::istringstream stream(pcm16_wav(), std::ios::binary);
    const humtrace::WavStreamReader reader(stream);
    require(!reader.metadata().extensible && reader.metadata().valid_bits == 16 &&
                reader.metadata().channel_mask == 0,
            "non-extensible headers expose container width as valid bits");
}
void rejects_malformed_extensible_headers() {
    const std::string data("\0\0\0\0\0\0", 6);
    ExtensibleFormat short_chunk; short_chunk.fmt_length = 18; short_chunk.cb_size = 0;
    require(decode_throws(extensible_wav(short_chunk, data)), "short extensible format chunk is rejected");
    ExtensibleFormat short_extension; short_extension.cb_size = 10;
    require(decode_throws(extensible_wav(short_extension, data)), "short cbSize extension is rejected");
    ExtensibleFormat unknown_subtype; unknown_subtype.subtype = 2;
    require(decode_throws(extensible_wav(unknown_subtype, data)), "compressed subtype is rejected");
    ExtensibleFormat too_many_valid_bits; too_many_valid_bits.valid_bits = 25;
    require(decode_throws(extensible_wav(too_many_valid_bits, data)), "valid bits above container are rejected");
    ExtensibleFormat zero_valid_bits; zero_valid_bits.valid_bits = 0;
    require(decode_throws(extensible_wav(zero_valid_bits, data)), "zero valid bits are rejected");
    ExtensibleFormat oversized_mask; oversized_mask.channel_mask = 0x7;
    require(decode_throws(extensible_wav(oversized_mask, data)), "mask with more speakers than channels is rejected");
    ExtensibleFormat partial_float; partial_float.container_bits = 32; partial_float.valid_bits = 24;
    partial_float.subtype = 3;
    require(decode_throws(extensible_wav(partial_float, std::string(8, '\0'))),
            "float with fewer valid bits than its container is rejected");
    require(decode_throws(pcm16_two_format_chunks_wav()), "duplicate format chunks are rejected");
}
void accepts_empty_data_chunk() {
    std::istringstream stream(riff({{"fmt ", fmt_payload(1, 1, 8000, 16)}, {"data", ""}}), std::ios::binary);
    humtrace::WavStreamReader reader(stream);
    require(reader.metadata().frame_count == 0, "empty data chunk has zero frames");
    require(reader.read_frames(16).empty(), "empty data chunk reads nothing");
    require(reader.metadata().container == "RIFF", "RIFF container is reported");
    require(reader.metadata().decoder == humtrace::wav_decoder_id, "decoder id is reported");
    require(std::string(humtrace::wav_decoder_id) == "humtrace-wav 1.1", "decoder id string");
}
void skips_unknown_and_odd_chunks() {
    const auto bytes = riff({{"LIST", std::string("\x01\x02\x03", 3)}, {"fmt ", fmt_payload(1, 1, 8000, 16)},
                             {"junk", std::string(5, '\xff')}, {"data", pcm16_samples({16384})},
                             {"bext", "z"}});
    std::istringstream stream(bytes, std::ios::binary);
    const auto audio = humtrace::decode_wav(stream);
    require(audio.interleaved_samples.size() == 1 && audio.interleaved_samples[0] == 0.5,
            "unknown and odd-sized chunks are skipped with padding");
    require(audio.container == "RIFF" && audio.decoder == "humtrace-wav 1.1", "WavAudio carries container/decoder");
}
void rejects_truncated_headers_and_reads() {
    const auto good = pcm16_wav();
    for (std::size_t length = 0; length < good.size(); ++length)
        require(decode_throws(good.substr(0, length)), "every truncation of a valid file is rejected");
    std::string stage;
    const auto message = error_of(good.substr(0, 8), &stage);
    require(stage == "header" && message.find("not a RIFF/WAVE file") != std::string::npos,
            "short header reports the header stage");
    error_of(good.substr(0, good.size() - 2), &stage);
    require(stage == "header", "RIFF size beyond a truncated stream is a header-stage error");
}
void rejects_chunk_size_overruns() {
    std::string stage;
    const auto data = pcm16_samples({1, 2});
    error_of(riff({{"fmt ", fmt_payload(1, 1, 8000, 16)}, {"data", data, 0xFFFFFFF0U}}), &stage);
    require(stage == "chunks", "data size beyond the container is rejected");
    require(decode_throws(riff({{"fmt ", fmt_payload(1, 1, 8000, 16)}, {"data", data}}, 0xFFFFFFFFU)),
            "RIFF size beyond the stream is rejected");
    require(decode_throws(riff({{"fmt ", fmt_payload(1, 1, 8000, 16)}, {"data", data}}, 2)),
            "RIFF size smaller than the header is rejected");
    require(decode_throws(riff({{"fmt ", fmt_payload(1, 1, 8000, 16), 0x7FFFFFFFU}, {"data", data}})),
            "fmt size beyond the container is rejected");
    std::string big_fmt = fmt_payload(1, 1, 8000, 16);
    big_fmt.resize(5000, '\0');
    const auto message = error_of(riff({{"fmt ", big_fmt}, {"data", data}}), &stage);
    require(stage == "fmt" && message.find("limit") != std::string::npos, "oversized fmt chunk hits the cap");
    require(decode_throws(riff({{"fmt ", fmt_payload(1, 1, 8000, 16)}, {"data", "abc", std::nullopt, false}})),
            "odd chunk without padding at container end is rejected");
    auto trailing = riff({{"fmt ", fmt_payload(1, 1, 8000, 16)}, {"data", data}});
    trailing += "xyz";
    trailing[4] = static_cast<char>(trailing[4] + 3);
    error_of(trailing, &stage);
    require(stage == "chunks", "1-7 leftover bytes inside RIFF are rejected");
    std::istringstream extra(riff({{"fmt ", fmt_payload(1, 1, 8000, 16)}, {"data", data}}) + "tail",
                             std::ios::binary);
    require(humtrace::decode_wav(extra).interleaved_samples.size() == 2, "bytes after the RIFF container are ignored");
}
void rejects_bad_alignment() {
    const auto data = pcm16_samples({1, 2});
    require(decode_throws(riff({{"fmt ", fmt_payload(1, 1, 8000, 16, 3)}, {"data", std::string(6, '\0')}})),
            "block align inconsistent with channels and depth is rejected");
    require(decode_throws(riff({{"fmt ", fmt_payload(1, 2, 8000, 16)}, {"data", std::string(6, '\0')}})),
            "data chunk with a partial frame is rejected");
    require(decode_throws(riff({{"fmt ", fmt_payload(1, 1, 8000, 12)}, {"data", data}})),
            "unsupported PCM depth is rejected");
    require(decode_throws(riff({{"fmt ", fmt_payload(3, 1, 8000, 16)}, {"data", data}})),
            "16-bit float is rejected");
    require(decode_throws(riff({{"fmt ", fmt_payload(2, 1, 8000, 16)}, {"data", data}})),
            "compressed format code is rejected");
    require(decode_throws(riff({{"fmt ", fmt_payload(1, 1, 8000, 16).substr(0, 14)}, {"data", data}})),
            "short fmt chunk is rejected");
    require(decode_throws(riff({{"data", data}})), "missing fmt chunk is rejected");
    require(decode_throws(riff({{"fmt ", fmt_payload(1, 1, 8000, 16)}})), "missing data chunk is rejected");
}
void enforces_channel_and_rate_caps() {
    std::string stage;
    error_of(riff({{"fmt ", fmt_payload(1, 0, 8000, 16, 2)}, {"data", std::string(2, '\0')}}), &stage);
    require(stage == "fmt", "zero channels are rejected at the fmt stage");
    const auto huge = error_of(riff({{"fmt ", fmt_payload(1, 4096, 8000, 8)}, {"data", std::string(4096, '\0')}}),
                               &stage);
    require(stage == "fmt" && huge.find("exceeds the limit") != std::string::npos, "channel cap is enforced");
    require(decode_throws(riff({{"fmt ", fmt_payload(1, 65535, 8000, 8)}, {"data", std::string(65535, '\0')}})),
            "maximum 16-bit channel count is rejected");
    std::istringstream at_cap(riff({{"fmt ", fmt_payload(1, humtrace::wav_limits::max_channels, 8000, 8)},
                                    {"data", std::string(humtrace::wav_limits::max_channels, '\x80')}}),
                              std::ios::binary);
    require(humtrace::decode_wav(at_cap).interleaved_samples.size() == humtrace::wav_limits::max_channels,
            "channel count at the cap is accepted");
    require(decode_throws(riff({{"fmt ", fmt_payload(1, 1, 0, 16)}, {"data", pcm16_samples({1})}})),
            "zero sample rate is rejected");
    require(decode_throws(riff({{"fmt ", fmt_payload(1, 1, humtrace::wav_limits::max_sample_rate_hz + 1, 16)},
                                {"data", pcm16_samples({1})}})),
            "sample rate above the cap is rejected");
}
void rejects_non_finite_floats() {
    for (const auto* bits : {"\x00\x00\x80\x7f", "\x00\x00\x80\xff", "\x00\x00\xc0\x7f"}) {
        std::string stage;
        const auto message = error_of(riff({{"fmt ", fmt_payload(3, 1, 8000, 32)}, {"data", std::string(bits, 4)}}),
                                      &stage);
        require(stage == "data" && message.find("non-finite") != std::string::npos, "inf/nan float32 rejected");
    }
    std::string nan64(8, '\0'); nan64[6] = '\xf8'; nan64[7] = '\x7f';
    require(decode_throws(riff({{"fmt ", fmt_payload(3, 1, 8000, 64)}, {"data", nan64}})), "nan float64 rejected");
}
void error_messages_do_not_echo_payload() {
    std::string fmt = fmt_payload(1, 1, 8000, 16);
    fmt[0] = 'Q'; fmt[1] = 'Z';  // format code spelled "QZ"
    const auto message = error_of(riff({{"QZQZ", "QZQZ"}, {"fmt ", fmt}, {"data", pcm16_samples({1})}}));
    require(!message.empty() && message.find("QZ") == std::string::npos, "messages never include file bytes");
}
void reads_pcm8_pcm32_and_float64() {
    std::istringstream pcm8(riff({{"fmt ", fmt_payload(1, 1, 8000, 8)}, {"data", std::string("\x00\x80\xc0", 3)}}),
                            std::ios::binary);
    require(humtrace::decode_wav(pcm8).interleaved_samples == std::vector<double>({-1.0, 0.0, 0.5}),
            "unsigned PCM8 is normalized");
    std::string s32; u32(s32, 0x80000000U); u32(s32, 0x40000000U);
    std::istringstream pcm32(riff({{"fmt ", fmt_payload(1, 2, 8000, 32)}, {"data", s32}}), std::ios::binary);
    require(humtrace::decode_wav(pcm32).interleaved_samples == std::vector<double>({-1.0, 0.5}),
            "PCM32 is normalized");
    std::string f64(8, '\0'); f64[6] = '\xe0'; f64[7] = '\x3f';  // 0.5
    std::istringstream float64(riff({{"fmt ", fmt_payload(3, 1, 8000, 64)}, {"data", f64}}), std::ios::binary);
    require(humtrace::decode_wav(float64).interleaved_samples == std::vector<double>({0.5}), "float64 decoded");
}
void in_memory_cap_is_caller_visible() {
    require(!decode_throws(pcm16_wav()), "valid file decodes under the default cap");
    std::istringstream stream(pcm16_wav(), std::ios::binary);
    bool threw = false;
    try { (void)humtrace::decode_wav(stream, nullptr, 1); } catch (const humtrace::WavError&) { threw = true; }
    require(threw, "decode_wav enforces the caller's sample cap");
}
void cancellation_is_observed() {
    humtrace::CancellationToken token;
    token.request_cancel();
    std::istringstream early(pcm16_wav(), std::ios::binary);
    bool cancelled = false;
    try { (void)humtrace::decode_wav(early, &token); } catch (const humtrace::OperationCancelled&) { cancelled = true; }
    require(cancelled, "a cancelled token stops decoding");

    humtrace::CancellationToken mid;
    const std::size_t frames = humtrace::wav_limits::read_batch_bytes;  // two internal batches of 16-bit mono
    std::istringstream big(riff({{"fmt ", fmt_payload(1, 1, 8000, 16)}, {"data", std::string(frames * 2, '\0')}}),
                           std::ios::binary);
    humtrace::WavStreamReader reader(big, &mid);
    require(reader.read_frames(10).size() == 10, "reads before cancellation succeed");
    mid.request_cancel();
    cancelled = false;
    try { (void)reader.read_frames(frames); } catch (const humtrace::OperationCancelled&) { cancelled = true; }
    require(cancelled && reader.frame_position() == 10, "cancelled read leaves the position unchanged");
}
void seek_frame_moves_across_chunks() {
    std::istringstream stream(pcm16_multiple_data_chunks_wav(), std::ios::binary);
    humtrace::WavStreamReader reader(stream);
    reader.seek_frame(1);
    const auto second = reader.read_frames(4);
    require(second.size() == 1 && second[0] == -1.0, "seek lands in the second data chunk");
    reader.seek_frame(0);
    require(reader.read_frames(1)[0] == 0.5, "seek back to start");
    reader.seek_frame(2);
    require(reader.read_frames(1).empty(), "seek to end reads nothing");
    bool threw = false;
    try { reader.seek_frame(3); } catch (const humtrace::WavError&) { threw = true; }
    require(threw, "seek past the end is rejected");
}
void unknown_containers_are_rejected() {
    auto bytes = pcm16_wav();
    bytes.replace(0, 4, "RIFX");
    require(error_of(bytes).find("not a RIFF/WAVE file") != std::string::npos, "big-endian RIFX is rejected");
    bytes = pcm16_wav();
    bytes.replace(8, 4, "AVI ");
    require(decode_throws(bytes), "non-WAVE form type is rejected");
}
} // namespace

int main() {
    decodes_pcm16_metadata_and_samples();
    rejects_non_wave_input();
    decodes_signed_24_bit_minimum();
    decodes_ieee_float32_samples();
    streaming_reader_returns_bounded_frame_batches();
    streaming_reader_continues_across_data_chunks();
    decodes_extensible_pcm_with_valid_bits_and_mask();
    decodes_extensible_float_with_unspecified_layout();
    legacy_formats_report_full_valid_bits_and_no_mask();
    rejects_malformed_extensible_headers();
    accepts_empty_data_chunk();
    skips_unknown_and_odd_chunks();
    rejects_truncated_headers_and_reads();
    rejects_chunk_size_overruns();
    rejects_bad_alignment();
    enforces_channel_and_rate_caps();
    rejects_non_finite_floats();
    error_messages_do_not_echo_payload();
    reads_pcm8_pcm32_and_float64();
    in_memory_cap_is_caller_visible();
    cancellation_is_observed();
    seek_frame_moves_across_chunks();
    unknown_containers_are_rejected();
    std::cout << "All WAV tests passed.\n";
}
