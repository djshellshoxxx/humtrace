#pragma once

#include "humtrace/streaming.hpp"
#include "../presentation/findings.hpp"
#include <list>
#include <map>
#include <tuple>

// UI-thread-only LRU caches. Analysis storage is immutable; each cache has a
// conservative byte-derived entry limit and evicts on every insertion.
class PagedData {
public:
    explicit PagedData(std::shared_ptr<humtrace::DiskAnalysis> storage, std::size_t cacheBytes = 8 * 1024 * 1024);
    [[nodiscard]] const humtrace::DiskAnalysis& storage() const { return *storage_; }
    [[nodiscard]] const humtrace::InterferenceFrame& frame(std::size_t channel, std::uint64_t index);
    [[nodiscard]] const std::vector<float>& row(std::size_t channel, std::uint64_t index, unsigned level = 0);
    [[nodiscard]] humtrace::ui::FindingRow finding(std::size_t channel, std::uint64_t index) const;
    [[nodiscard]] humtrace::StoredTrack track(std::size_t channel, std::uint64_t index) const { return storage_->track(channel, index); }
    [[nodiscard]] std::size_t cachedBytesUpperBound() const;
private:
    template<class Key, class Value> struct Cache {
        using Entry = std::pair<Key, Value>;
        std::list<Entry> entries;
        std::map<Key, typename std::list<Entry>::iterator> index;
        std::size_t capacity{};
        template<class Loader> const Value& get(const Key& key, Loader load) {
            if (const auto it = index.find(key); it != index.end()) {
                entries.splice(entries.begin(), entries, it->second);
                return it->second->second;
            }
            while (entries.size() >= capacity) { index.erase(entries.back().first); entries.pop_back(); }
            entries.emplace_front(key, load());
            index.emplace(key, entries.begin());
            while (entries.size() > capacity) { index.erase(entries.back().first); entries.pop_back(); }
            return entries.front().second;
        }
    };
    std::shared_ptr<humtrace::DiskAnalysis> storage_;
    Cache<std::pair<std::size_t, std::uint64_t>, humtrace::InterferenceFrame> frames_;
    Cache<std::tuple<std::size_t, std::uint64_t, unsigned>, std::vector<float>> rows_;
    std::size_t rowEntryBytes_{};
};
