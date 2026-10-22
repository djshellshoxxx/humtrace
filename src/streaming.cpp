#include "humtrace/streaming.hpp"
#include "humtrace/json.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <streambuf>
#include <type_traits>

namespace humtrace {
namespace {
namespace fs = std::filesystem;

struct Budget {
    fs::path directory;
    StreamingLimits limits;
    std::function<bool()> cancelled;
    std::uint64_t used{};
    std::uint64_t since_space_check{1024 * 1024};
    void checkpoint() const { if (cancelled && cancelled()) throw AnalysisCancelled(); }
    void take(std::uint64_t bytes) {
        if (bytes > limits.max_disk_bytes - used)
            throw std::runtime_error("streaming temporary disk quota exceeded");
        since_space_check += bytes;
        if (since_space_check >= 1024 * 1024) {
            checkpoint();
            const auto free = fs::space(directory).available;
            if (free < limits.min_free_disk_bytes || bytes > free - limits.min_free_disk_bytes)
                throw std::runtime_error("insufficient free disk space for streaming report");
            since_space_check = 0;
        }
        used += bytes;
    }
    void erase(const fs::path& path) {
        const auto bytes = fs::file_size(path);
        fs::remove(path);
        used -= bytes;
    }
};

class BudgetBuffer final : public std::streambuf {
public:
    BudgetBuffer(std::streambuf* destination, Budget& budget) : destination_(destination), budget_(budget) {}
private:
    std::streamsize xsputn(const char* text, std::streamsize size) override {
        budget_.take(static_cast<std::uint64_t>(size));
        return destination_->sputn(text, size);
    }
    int_type overflow(int_type value) override {
        if (traits_type::eq_int_type(value, traits_type::eof())) return traits_type::not_eof(value);
        const char byte = traits_type::to_char_type(value);
        return xsputn(&byte, 1) == 1 ? value : traits_type::eof();
    }
    int sync() override { return destination_->pubsync(); }
    std::streambuf* destination_;
    Budget& budget_;
};

template<class Writer>
void append(const fs::path& path, Budget& budget, Writer writer) {
    budget.checkpoint();
    std::ofstream file(path, std::ios::binary | std::ios::app);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    BudgetBuffer buffer(file.rdbuf(), budget);
    std::ostream output(&buffer);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << std::setprecision(17);
    writer(output);
    output.flush();
}

void copy_file_contents(std::ostream& output, const fs::path& path, const Budget& budget) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not read streaming report spool");
    std::array<char, 65536> block{};
    while (input) {
        budget.checkpoint();
        input.read(block.data(), static_cast<std::streamsize>(block.size()));
        output.write(block.data(), input.gcount());
    }
    if (!input.eof()) throw std::runtime_error("failed reading streaming report spool");
}

template<class Consumer>
void scan(const fs::path& path, const Budget& budget, Consumer consume) {
    static_assert(std::is_trivially_copyable_v<TrackObservation>);
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not read track spool");
    TrackObservation observation;
    std::size_t count = 0;
    while (input.read(reinterpret_cast<char*>(&observation), sizeof(observation))) {
        if ((count++ % 1024) == 0) budget.checkpoint();
        consume(observation);
    }
    if (!input.eof() || input.gcount() != 0) throw std::runtime_error("truncated track spool");
}

// IEEE doubles have an order-preserving integer key after sign normalization.
// A radix selection needs 64 bounded scans, independent of recording length.
std::uint64_t order_key(double value) {
    static_assert(std::numeric_limits<double>::is_iec559 && sizeof(double) == sizeof(std::uint64_t));
    const auto bits = std::bit_cast<std::uint64_t>(value);
    constexpr auto sign = std::uint64_t{1} << 63;
    return bits & sign ? ~bits : bits ^ sign;
}
double from_key(std::uint64_t key) {
    constexpr auto sign = std::uint64_t{1} << 63;
    return std::bit_cast<double>(key & sign ? key ^ sign : ~key);
}
double select(const fs::path& path, const Budget& budget, std::size_t rank, bool frequency) {
    std::uint64_t prefix = 0, mask = 0;
    for (int shift = 63; shift >= 0; --shift) {
        const auto bit = std::uint64_t{1} << static_cast<unsigned>(shift);
        std::size_t below = 0;
        scan(path, budget, [&](const TrackObservation& observation) {
            const auto key = order_key(frequency ? observation.frequency_hz : observation.level_dbfs);
            if ((key & mask) == prefix && (key & bit) == 0) ++below;
        });
        if (rank >= below) { prefix |= bit; rank -= below; }
        mask |= bit;
    }
    return from_key(prefix);
}
double median(const fs::path& path, const Budget& budget, std::size_t count, bool frequency) {
    const auto upper = select(path, budget, count / 2, frequency);
    return count % 2 ? upper : (select(path, budget, count / 2 - 1, frequency) + upper) / 2.0;
}

class DiskReport final : public FrameSink {
public:
    DiskReport(fs::path parent, const StreamingLimits& limits, const SessionCallbacks& callbacks)
        : budget_{std::move(parent), limits, callbacks.cancelled} {
        if (limits.max_disk_bytes == 0 || limits.max_resident_bytes == 0 || limits.max_active_tracks == 0 || limits.max_active_tracks > 65536)
            throw std::invalid_argument("streaming resource limits must be positive");
        std::random_device random;
        for (int attempt = 0; attempt < 32; ++attempt) {
            const auto candidate = budget_.directory / (".humtrace-stream-" + std::to_string(random()));
            if (fs::create_directory(candidate)) { directory_ = candidate; break; }
        }
        if (directory_.empty()) throw std::runtime_error("could not create streaming scratch directory");
        try {
            fs::permissions(directory_, fs::perms::owner_all, fs::perm_options::replace);
            budget_.take(0);
        } catch (...) { std::error_code ignored; fs::remove_all(directory_, ignored); throw; }
    }
    ~DiskReport() override { std::error_code ignored; fs::remove_all(directory_, ignored); }
    void begin(const WavMetadata& metadata, const PipelineSettings& settings) override {
        rate_ = metadata.sample_rate_hz;
        tracker_ = settings.tracker;
        tracker_.bin_spacing_hz = static_cast<double>(rate_) /
            static_cast<double>(select_window_size(metadata, settings.max_interleaved_window_samples));
        // Conservative scratch bound: interleaved rolling window + Welch buffers,
        // one channel's FFT, bounded association candidates and active endpoints.
        const auto window = select_window_size(metadata, settings.max_interleaved_window_samples);
        const auto scratch = static_cast<std::uint64_t>(metadata.channels) *
            (window * sizeof(double) + 6 * settings.welch.segment_size * sizeof(double)) +
            window * 64ULL + 32ULL * 1024 * 1024 + budget_.limits.max_active_tracks * 128ULL;
        if (scratch > budget_.limits.max_resident_bytes)
            throw std::runtime_error("streaming resident memory budget exceeded by analysis settings");
        counts_.resize(metadata.channels);
        suppressed_.resize(metadata.channels);
        for (std::size_t c = 0; c < metadata.channels; ++c) {
            associations_.emplace_back(tracker_, budget_.limits.max_active_tracks);
            append(frames(c), budget_, [](std::ostream&) {});
        }
    }
    void frame(std::size_t channel, const InterferenceFrame& measured, const FramePeaks& peaks) override {
        append(frames(channel), budget_, [&](std::ostream& output) {
            if (counts_[channel]++) output << ',';
            write_frame_json(output, measured, rate_);
        });
        associations_[channel].push(peaks, [&](std::size_t id, const TrackObservation& observation) {
            append(observations(channel, id), budget_, [&](std::ostream& output) {
                output.write(reinterpret_cast<const char*>(&observation), sizeof(observation));
            });
        }, [&](std::size_t id) { end_track(channel, id); });
        std::size_t active = 0;
        for (const auto& association : associations_) active += association.active_count();
        if (active > budget_.limits.max_active_tracks)
            throw std::runtime_error("streaming active track memory budget exceeded");
    }
    void finish(std::size_t channel, ReportChannel& summary) override {
        associations_[channel].finish([&](std::size_t id) { end_track(channel, id); });
        summary.suppressed_tracks = suppressed_[channel];
    }
    void write(std::ostream& destination, const AnalysisReport& summary) {
        BudgetBuffer buffer(destination.rdbuf(), budget_);
        std::ostream output(&buffer);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        ReportArrays arrays;
        arrays.frames = [&](std::ostream& out, std::size_t c) { copy_file_contents(out, frames(c), budget_); };
        arrays.tracks = [&](std::ostream& out, std::size_t c) {
            bool first = true;
            for (std::size_t id = 1; id <= associations_[c].tracks_created(); ++id) {
                budget_.checkpoint();
                const auto path = track_json(c, id);
                if (!fs::exists(path)) continue;
                if (!first) out << ", ";
                first = false;
                copy_file_contents(out, path, budget_);
            }
        };
        write_report_json(output, summary, &arrays);
        output.flush();
        budget_.checkpoint();
    }
    std::vector<std::uint64_t> counts_;
private:
    fs::path frames(std::size_t c) const { return directory_ / (std::to_string(c) + "-frames.json"); }
    fs::path observations(std::size_t c, std::size_t id) const {
        return directory_ / (std::to_string(c) + "-" + std::to_string(id) + ".observations");
    }
    fs::path track_json(std::size_t c, std::size_t id) const {
        return directory_ / (std::to_string(c) + "-" + std::to_string(id) + ".json");
    }
    void end_track(std::size_t channel, std::size_t id) {
        const auto path = observations(channel, id);
        Track track;
        track.id = id;
        double sum_t = 0, sum_f = 0;
        scan(path, budget_, [&](const TrackObservation& observation) {
            if (track.observed_frames++ == 0) track.first_frame = observation.frame_index;
            track.last_frame = observation.frame_index;
            sum_t += static_cast<double>(observation.start_sample) / rate_;
            sum_f += observation.frequency_hz;
        });
        if (suppressed_.size() < associations_.size()) suppressed_.resize(associations_.size());
        if (track.observed_frames < tracker_.min_observed_frames) {
            ++suppressed_[channel]; budget_.erase(path); return;
        }
        track.missing_frames = track.last_frame - track.first_frame + 1 - track.observed_frames;
        track.median_frequency_hz = median(path, budget_, track.observed_frames, true);
        track.median_level_dbfs = median(path, budget_, track.observed_frames, false);
        const auto n = static_cast<double>(track.observed_frames);
        const auto mean_t = sum_t / n, mean_f = sum_f / n;
        double covariance = 0, variance = 0;
        scan(path, budget_, [&](const TrackObservation& observation) {
            const auto dt = static_cast<double>(observation.start_sample) / rate_ - mean_t;
            covariance += dt * (observation.frequency_hz - mean_f);
            variance += dt * dt;
        });
        if (variance > 0) track.drift_hz_per_second = covariance / variance;
        append(track_json(channel, id), budget_, [&](std::ostream& output) {
            write_track_json(output, track, [&](std::ostream& observations_output) {
                bool first = true;
                scan(path, budget_, [&](const TrackObservation& observation) {
                    if (!first) observations_output << ", ";
                    first = false;
                    observations_output << "{\"frame_index\": " << observation.frame_index
                        << ", \"start_sample\": " << observation.start_sample << ", \"frequency_hz\": ";
                    json::write_number(observations_output, observation.frequency_hz);
                    observations_output << ", \"level_dbfs\": ";
                    json::write_number(observations_output, observation.level_dbfs);
                    observations_output << '}';
                });
            });
        });
        budget_.erase(path);
    }
    Budget budget_;
    fs::path directory_;
    std::uint32_t rate_{};
    TrackerSettings tracker_;
    std::vector<PeakAssociator> associations_;
    std::vector<std::size_t> suppressed_;
};
} // namespace

StreamingResult stream_file_report(const std::string& input, const std::string& output,
    const PipelineSettings& settings, const SessionCallbacks& callbacks, const StreamingLimits& limits,
    bool replace_existing, bool full_input_path) {
    require_distinct_paths(output, input);
    const auto native_output = path_from_utf8(output);
    if (!replace_existing && fs::exists(native_output)) throw std::runtime_error("output file already exists");
    auto parent = native_output.parent_path();
    if (parent.empty()) parent = fs::current_path();
    DiskReport disk(parent, limits, callbacks);
    auto analysis_callbacks = callbacks;
    analysis_callbacks.progress = [&](Stage stage, double fraction) {
        if (stage != Stage::Done && callbacks.progress) callbacks.progress(stage, fraction);
    };
    auto result = analyze_file(input, settings, analysis_callbacks, false, &disk);
    if (!full_input_path) {
        const auto filename = path_from_utf8(input).filename().u8string();
        result.report.input_path.assign(filename.begin(), filename.end());
    }
    if (callbacks.progress) callbacks.progress(Stage::Finalizing, 0.0);
    write_file_atomically(output, [&](std::ostream& destination) { disk.write(destination, result.report); }, replace_existing);
    if (callbacks.progress) callbacks.progress(Stage::Done, 1.0);
    return {std::move(result.report), std::move(disk.counts_)};
}

} // namespace humtrace
