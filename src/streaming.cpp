#include "humtrace/streaming.hpp"
#include "humtrace/json.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
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
struct EncodedCandidate {
    double nominal{};
    std::size_t supporting{}, count{};
    std::array<HarmonicMeasurement, 10> harmonics{};
};
struct EncodedFrame {
    std::uint64_t start{};
    SpectralPeak peak;
    bool has_levels{};
    SampleLevels levels;
    EncodedCandidate mains50, mains60;
};
EncodedCandidate encode(const HarmonicCandidate& candidate) {
    if (candidate.harmonics.size() > 10) throw std::runtime_error("private harmonic storage limit exceeded");
    EncodedCandidate result{candidate.nominal_frequency_hz, candidate.supporting_harmonics, candidate.harmonics.size(), {}};
    std::copy(candidate.harmonics.begin(), candidate.harmonics.end(), result.harmonics.begin());
    return result;
}
HarmonicCandidate decode(const EncodedCandidate& candidate) {
    if (candidate.count > 10) throw std::runtime_error("invalid private harmonic storage count");
    HarmonicCandidate result;
    result.nominal_frequency_hz = candidate.nominal; result.supporting_harmonics = candidate.supporting;
    result.harmonics.assign(candidate.harmonics.begin(), candidate.harmonics.begin() + static_cast<std::ptrdiff_t>(candidate.count));
    return result;
}

struct FrameIndex { std::uint64_t offset{}, length{}; };

template<class T>
T read_record(const fs::path& path, std::uint64_t index) {
    static_assert(std::is_trivially_copyable_v<T>);
    if (index > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()) / sizeof(T))
        throw std::runtime_error("analysis index is out of range");
    std::ifstream input(path, std::ios::binary);
    input.seekg(static_cast<std::streamoff>(index * sizeof(T)));
    T value{};
    if (!input.read(reinterpret_cast<char*>(&value), sizeof(T)))
        throw std::runtime_error("could not read private analysis index");
    return value;
}
void merge_evidence(FrameEvidence& all, const FrameEvidence& frame) {
    all.lo50 = std::min(all.lo50, frame.lo50); all.hi50 = std::max(all.hi50, frame.hi50);
    all.lo60 = std::min(all.lo60, frame.lo60); all.hi60 = std::max(all.hi60, frame.hi60);
    all.harmonic_count = frame.harmonic_count;
    all.harmonic_count_60 = frame.harmonic_count_60;
    if (frame.measured) {
        if (!all.measured) { all.rms_lo = frame.rms_lo; all.rms_hi = frame.rms_hi; }
        all.measured = true;
        all.rms_lo = std::min(all.rms_lo, frame.rms_lo); all.rms_hi = std::max(all.rms_hi, frame.rms_hi);
        all.sample_peak = std::max(all.sample_peak, frame.sample_peak);
        all.below_floor = all.below_floor || frame.below_floor;
    }
}

class DiskReport final : public FrameSink {
public:
    DiskReport(fs::path parent, const StreamingLimits& limits, const SessionCallbacks& callbacks, bool retain_views = false)
        : budget_{std::move(parent), limits, callbacks.cancelled}, retain_views_(retain_views) {
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
        total_samples_ = metadata.frame_count;
        rows_ = settings.spectrogram_rows;
        tracker_ = settings.tracker;
        tracker_.bin_spacing_hz = static_cast<double>(rate_) /
            static_cast<double>(select_window_size(metadata, settings));
        // Conservative scratch bound: interleaved rolling window + Welch buffers,
        // one channel's FFT, bounded association candidates and active endpoints.
        const auto window = select_window_size(metadata, settings);
        const auto scratch = static_cast<std::uint64_t>(metadata.channels) *
            (window * sizeof(double) + 6 * settings.welch.segment_size * sizeof(double)) +
            window * 64ULL + 32ULL * 1024 * 1024 + budget_.limits.max_active_tracks * 128ULL +
            (retain_views_ ? metadata.channels * (settings.waveform_buckets * 2ULL * sizeof(float) +
                settings.spectrogram_rows * 4ULL * sizeof(float)) : 0);
        if (scratch > budget_.limits.max_resident_bytes)
            throw std::runtime_error("streaming resident memory budget exceeded by analysis settings");
        counts_.resize(metadata.channels);
        suppressed_.resize(metadata.channels);
        reported_.resize(metadata.channels);
        lineage_.resize(metadata.channels);
        ambiguity_counts_.resize(metadata.channels);
        if (retain_views_) views_.resize(metadata.channels);
        for (std::size_t c = 0; c < metadata.channels; ++c) {
            associations_.emplace_back(tracker_, budget_.limits.max_active_tracks);
            append(frames(c), budget_, [](std::ostream&) {});
            append(ambiguities(c), budget_, [](std::ostream&) {});
        }
    }
    [[nodiscard]] bool wants_views() const override { return retain_views_; }
    void view_row(std::size_t channel, const std::vector<float>& row) override {
        append(row_path(channel, 0), budget_, [&](std::ostream& out) {
            out.write(reinterpret_cast<const char*>(row.data()), static_cast<std::streamsize>(row.size() * sizeof(float)));
        });
    }
    void envelope(std::size_t channel, ChannelView&& view) override { views_[channel] = std::move(view); }
    void frame(std::size_t channel, const InterferenceFrame& measured, const FramePeaks& peaks) override {
        const auto before = retain_views_ ? fs::file_size(frames(channel)) : 0;
        const bool separator = counts_[channel] != 0;
        append(frames(channel), budget_, [&](std::ostream& output) {
            if (counts_[channel]++) output << ',';
            write_frame_json(output, measured, rate_);
        });
        if (retain_views_) {
            const FrameIndex index{before + (separator ? 1 : 0), fs::file_size(frames(channel)) - before - (separator ? 1 : 0)};
            append(frame_index(channel), budget_, [&](std::ostream& out) {
                out.write(reinterpret_cast<const char*>(&index), sizeof(index));
            });
            EncodedFrame raw;
            raw.start = measured.start_sample; raw.peak = measured.strongest_peak;
            raw.has_levels = measured.levels.has_value();
            if (measured.levels) raw.levels = *measured.levels;
            raw.mains50 = encode(measured.mains_50); raw.mains60 = encode(measured.mains_60);
            append(frame_binary(channel), budget_, [&](std::ostream& out) {
                out.write(reinterpret_cast<const char*>(&raw), sizeof(raw));
            });
            FrameEvidence evidence;
            evidence.lo50 = evidence.hi50 = measured.mains_50.supporting_harmonics;
            evidence.lo60 = evidence.hi60 = measured.mains_60.supporting_harmonics;
            evidence.harmonic_count = measured.mains_50.harmonics.size();
            evidence.harmonic_count_60 = measured.mains_60.harmonics.size();
            if (measured.levels) {
                evidence.measured = true;
                evidence.rms_lo = evidence.rms_hi = measured.levels->rms_dbfs;
                evidence.sample_peak = measured.levels->sample_peak_dbfs;
                evidence.below_floor = measured.levels->rms_below_numeric_floor;
            }
            append(evidence_path(channel), budget_, [&](std::ostream& out) {
                out.write(reinterpret_cast<const char*>(&evidence), sizeof(evidence));
            });
        }
        associations_[channel].push(peaks, [&](std::size_t id, const TrackObservation& observation) {
            append(observations(channel, id), budget_, [&](std::ostream& output) {
                output.write(reinterpret_cast<const char*>(&observation), sizeof(observation));
            });
        }, [&](std::size_t id, TrackEndReason reason) { end_track(channel, id, reason); },
        [&](const TrackAmbiguity& event) {
            append(ambiguities(channel), budget_, [&](std::ostream& out) {
                if (ambiguity_counts_[channel]++) out << ", ";
                write_ambiguity_json(out, event);
            });
            for (const auto id : event.parent_track_ids) lineage_[channel][id].end = event.id;
            for (const auto id : event.child_track_ids) lineage_[channel][id].start = event.id;
        });
        std::size_t active = 0;
        for (const auto& association : associations_) active += association.active_count();
        if (active > budget_.limits.max_active_tracks)
            throw std::runtime_error("streaming active track memory budget exceeded");
    }
    void finish(std::size_t channel, ReportChannel& summary) override {
        associations_[channel].finish([&](std::size_t id, TrackEndReason reason) { end_track(channel, id, reason); });
        summary.suppressed_tracks = suppressed_[channel];
        if (retain_views_) {
            append(track_index(channel), budget_, [&](std::ostream& out) {
                for (std::size_t id = 1; id <= associations_[channel].tracks_created(); ++id) {
                    budget_.checkpoint();
                    if (!fs::exists(track_info(channel, id))) continue;
                    const auto value = static_cast<std::uint64_t>(id);
                    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
                    ++reported_[channel];
                }
            });
            build_pyramid(channel);
        }
    }
    void write(std::ostream& destination, const AnalysisReport& summary,
               const fs::path& parent = {}, const SessionCallbacks* callbacks = nullptr) {
        Budget publication{parent.empty() ? budget_.directory : parent, budget_.limits,
                           callbacks ? callbacks->cancelled : budget_.cancelled};
        publication.limits.max_disk_bytes -= budget_.used;
        BudgetBuffer buffer(destination.rdbuf(), publication);
        std::ostream output(&buffer);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        ReportArrays arrays;
        arrays.frames = [&](std::ostream& out, std::size_t c) { copy_file_contents(out, frames(c), publication); };
        arrays.ambiguities = [&](std::ostream& out, std::size_t c) { copy_file_contents(out, ambiguities(c), publication); };
        arrays.tracks = [&](std::ostream& out, std::size_t c) {
            bool first = true;
            for (std::size_t id = 1; id <= associations_[c].tracks_created(); ++id) {
                publication.checkpoint();
                const auto path = track_json(c, id);
                if (!fs::exists(path)) continue;
                if (!first) out << ", ";
                first = false;
                copy_file_contents(out, path, publication);
            }
        };
        write_report_json(output, summary, &arrays);
        output.flush();
        publication.checkpoint();
    }
    void freeze() { budget_.cancelled = {}; }
    std::uint64_t total_samples() const { return total_samples_; }
    const std::vector<ChannelView>& views() const { return views_; }
    std::uint64_t track_count(std::size_t c) const { return reported_.at(c); }
    InterferenceFrame frame_value(std::size_t c, std::uint64_t f) const {
        if (f >= counts_.at(c)) throw std::out_of_range("frame index");
        const auto raw = read_record<EncodedFrame>(frame_binary(c), f);
        InterferenceFrame result;
        result.start_sample = raw.start; result.strongest_peak = raw.peak;
        if (raw.has_levels) result.levels = raw.levels;
        result.mains_50 = decode(raw.mains50); result.mains_60 = decode(raw.mains60);
        return result;
    }
    std::string read_frame(std::size_t c, std::uint64_t f) const {
        if (f >= counts_.at(c)) throw std::out_of_range("frame index");
        const auto index = read_record<FrameIndex>(frame_index(c), f);
        if (index.length > 65536) throw std::runtime_error("invalid private frame size");
        std::ifstream input(frames(c), std::ios::binary);
        input.seekg(static_cast<std::streamoff>(index.offset));
        std::string text(static_cast<std::size_t>(index.length), '\0');
        if (!input.read(text.data(), static_cast<std::streamsize>(text.size())))
            throw std::runtime_error("could not read private frame");
        return text;
    }
    StoredTrack read_track(std::size_t c, std::uint64_t index) const {
        if (index >= reported_.at(c)) throw std::out_of_range("track index");
        const auto id = read_record<std::uint64_t>(track_index(c), index);
        return read_record<StoredTrack>(track_info(c, static_cast<std::size_t>(id)), 0);
    }
    std::vector<float> read_row(std::size_t c, std::uint64_t index, unsigned level) const {
        if (level >= 63) throw std::out_of_range("spectrogram level");
        const auto width = std::uint64_t{1} << level;
        if (index >= (counts_.at(c) + width - 1) / width) throw std::out_of_range("spectrogram index");
        std::ifstream input(row_path(c, level), std::ios::binary);
        input.seekg(static_cast<std::streamoff>(index * rows_ * sizeof(float)));
        std::vector<float> row(rows_);
        if (!input.read(reinterpret_cast<char*>(row.data()), static_cast<std::streamsize>(row.size() * sizeof(float))))
            throw std::runtime_error("could not read private spectrogram row");
        return row;
    }
    std::vector<std::uint64_t> counts_;
private:
    fs::path frames(std::size_t c) const { return directory_ / (std::to_string(c) + "-frames.json"); }
    fs::path ambiguities(std::size_t c) const { return directory_ / (std::to_string(c) + "-ambiguities.json"); }
    fs::path observations(std::size_t c, std::size_t id) const {
        return directory_ / (std::to_string(c) + "-" + std::to_string(id) + ".observations");
    }
    fs::path track_json(std::size_t c, std::size_t id) const {
        return directory_ / (std::to_string(c) + "-" + std::to_string(id) + ".json");
    }
    fs::path frame_binary(std::size_t c) const { return directory_ / (std::to_string(c) + "-frames.bin"); }
    fs::path frame_index(std::size_t c) const { return directory_ / (std::to_string(c) + "-frame.index"); }
    fs::path track_index(std::size_t c) const { return directory_ / (std::to_string(c) + "-track.index"); }
    fs::path evidence_path(std::size_t c) const { return directory_ / (std::to_string(c) + "-evidence.bin"); }
    fs::path row_path(std::size_t c, unsigned level) const { return directory_ / (std::to_string(c) + "-rows-" + std::to_string(level) + ".bin"); }
    fs::path track_info(std::size_t c, std::size_t id) const {
        return directory_ / (std::to_string(c) + "-" + std::to_string(id) + ".summary");
    }
    void build_pyramid(std::size_t c) {
        auto count = counts_[c];
        for (unsigned level = 1; count > 1; ++level) {
            std::ifstream input(row_path(c, level - 1), std::ios::binary);
            if (!input) throw std::runtime_error("could not open spectrogram pyramid source");
            append(row_path(c, level), budget_, [&](std::ostream& out) {
                std::vector<float> left(rows_), right(rows_);
                for (std::uint64_t i = 0; i < count; i += 2) {
                    budget_.checkpoint();
                    const auto bytes = static_cast<std::streamsize>(rows_ * sizeof(float));
                    if (!input.read(reinterpret_cast<char*>(left.data()), bytes))
                        throw std::runtime_error("truncated spectrogram pyramid source");
                    if (i + 1 < count) {
                        if (!input.read(reinterpret_cast<char*>(right.data()), bytes))
                            throw std::runtime_error("truncated spectrogram pyramid source");
                        for (std::size_t r = 0; r < rows_; ++r) left[r] = std::max(left[r], right[r]);
                    }
                    out.write(reinterpret_cast<const char*>(left.data()), bytes);
                }
            });
            count = (count + 1) / 2;
        }
    }
    void end_track(std::size_t channel, std::size_t id, TrackEndReason reason) {
        const auto path = observations(channel, id);
        Track track;
        track.id = id;
        const auto link = lineage_[channel].find(id);
        if (link != lineage_[channel].end()) {
            track.start_ambiguity_id = link->second.start;
            track.end_ambiguity_id = link->second.end;
            lineage_[channel].erase(link);
        }
        scan(path, budget_, [&](const TrackObservation& observation) {
            if (track.observed_frames++ == 0) track.first_frame = observation.frame_index;
            track.last_frame = observation.frame_index;
        });
        if (suppressed_.size() < associations_.size()) suppressed_.resize(associations_.size());
        if (track.observed_frames < tracker_.min_observed_frames) {
            ++suppressed_[channel]; budget_.erase(path); return;
        }
        track.end_reason = reason;
        summarize_track(track, rate_, [&](const ObservationVisitor& visit) { scan(path, budget_, visit); },
            [&](std::size_t rank, bool frequency) { return select(path, budget_, rank, frequency); });
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
            }, [&](std::ostream& gaps) {
                std::optional<std::size_t> previous;
                bool first = true;
                scan(path, budget_, [&](const TrackObservation& observation) {
                    if (previous && observation.frame_index > *previous + 1) {
                        if (!first) gaps << ", ";
                        first = false;
                        gaps << "{\"first_frame_inclusive\": " << *previous + 1
                             << ", \"last_frame_inclusive\": " << observation.frame_index - 1 << '}';
                    }
                    previous = observation.frame_index;
                });
            });
        });
        if (retain_views_) {
            StoredTrack stored{track.id, track.first_frame, track.last_frame, track.observed_frames,
                track.missing_frames, track.median_frequency_hz, track.median_level_dbfs, track.drift_hz_per_second, {}};
            stored.start_ambiguity_id = track.start_ambiguity_id;
            stored.end_ambiguity_id = track.end_ambiguity_id;
            stored.end_reason = track.end_reason;
            for (std::size_t f = track.first_frame; f <= track.last_frame; ++f) {
                if ((f % 1024) == 0) budget_.checkpoint();
                merge_evidence(stored.evidence, read_record<FrameEvidence>(evidence_path(channel), f));
            }
            append(track_info(channel, id), budget_, [&](std::ostream& out) {
                out.write(reinterpret_cast<const char*>(&stored), sizeof(stored));
            });
        }
        budget_.erase(path);
    }
    Budget budget_;
    fs::path directory_;
    std::uint32_t rate_{};
    bool retain_views_{};
    std::uint64_t total_samples_{};
    std::size_t rows_{};
    std::vector<ChannelView> views_;
    std::vector<std::uint64_t> reported_;
    TrackerSettings tracker_;
    std::vector<PeakAssociator> associations_;
    std::vector<std::size_t> suppressed_;
    struct Lineage { std::optional<std::size_t> start, end; };
    std::vector<std::map<std::size_t, Lineage>> lineage_;
    std::vector<std::size_t> ambiguity_counts_;
};
} // namespace

struct DiskAnalysis::Impl {
    std::shared_ptr<DiskReport> disk;
    AnalysisReport summary;
};
DiskAnalysis::DiskAnalysis(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {
    if (!impl_ || !impl_->disk) throw std::invalid_argument("missing private analysis storage");
}
const AnalysisReport& DiskAnalysis::report() const { return impl_->summary; }
const std::vector<ChannelView>& DiskAnalysis::views() const { return impl_->disk->views(); }
std::uint64_t DiskAnalysis::frame_count(std::size_t c) const { return impl_->disk->counts_.at(c); }
std::uint64_t DiskAnalysis::track_count(std::size_t c) const { return impl_->disk->track_count(c); }
InterferenceFrame DiskAnalysis::frame(std::size_t c, std::uint64_t f) const { return impl_->disk->frame_value(c, f); }
std::string DiskAnalysis::frame_json(std::size_t c, std::uint64_t f) const { return impl_->disk->read_frame(c, f); }
StoredTrack DiskAnalysis::track(std::size_t c, std::uint64_t t) const { return impl_->disk->read_track(c, t); }
std::vector<float> DiskAnalysis::spectrogram_row(std::size_t c, std::uint64_t f, unsigned level) const { return impl_->disk->read_row(c, f, level); }
std::uint64_t DiskAnalysis::frame_start_sample(std::size_t c, std::uint64_t f) const {
    if (f >= frame_count(c)) throw std::out_of_range("frame index");
    return std::min(f * report().hop_size_samples, impl_->disk->total_samples() - report().frame_size_samples);
}
void DiskAnalysis::export_json(const std::string& output, bool replace_existing, bool full_input_path,
                               const SessionCallbacks& callbacks) const {
    require_distinct_paths(output, report().input_path);
    auto summary = report();
    if (!full_input_path) {
        const auto name = path_from_utf8(summary.input_path).filename().u8string();
        summary.input_path.assign(name.begin(), name.end());
    }
    auto parent = path_from_utf8(output).parent_path();
    if (parent.empty()) parent = fs::current_path();
    if (callbacks.progress) callbacks.progress(Stage::Finalizing, 0.0);
    write_file_atomically(output, [&](std::ostream& out) { impl_->disk->write(out, summary, parent, &callbacks); }, replace_existing);
    if (callbacks.progress) callbacks.progress(Stage::Done, 1.0);
}
std::shared_ptr<DiskAnalysis> analyze_file_paged(const std::string& input, const PipelineSettings& settings,
                                              const SessionCallbacks& callbacks, const StreamingLimits& limits) {
    auto impl = std::make_shared<DiskAnalysis::Impl>();
    impl->disk = std::make_shared<DiskReport>(fs::temp_directory_path(), limits, callbacks, true);
    auto analysis_callbacks = callbacks;
    analysis_callbacks.progress = [&](Stage stage, double fraction) {
        if (stage != Stage::Done && callbacks.progress) callbacks.progress(stage, fraction);
    };
    auto result = analyze_file(input, settings, analysis_callbacks, false, impl->disk.get());
    impl->summary = std::move(result.report);
    impl->disk->freeze();
    if (callbacks.progress) callbacks.progress(Stage::Done, 1.0);
    return std::make_shared<DiskAnalysis>(std::move(impl));
}

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
