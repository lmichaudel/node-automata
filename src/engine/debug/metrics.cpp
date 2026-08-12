#include "metrics.hpp"

#include <algorithm>
#include <limits>
#include <unordered_map>

namespace metrics {
	namespace {
		struct Entry {
			std::string unit;
			Kind kind{Kind::Gauge};
			f64 current{0.0};
			f64 latest{0.0};
			f64 sum{0.0};
			f64 minimum{std::numeric_limits<f64>::max()};
			f64 maximum{std::numeric_limits<f64>::lowest()};
			u64 samples{0};
			bool touched{false};
		};

		std::unordered_map<std::string, Entry> entries;

		void finalize(Entry& entry) {
			if (!entry.touched) {
				return;
			}
			entry.latest = entry.current;
			entry.sum += entry.current;
			entry.minimum = std::min(entry.minimum, entry.current);
			entry.maximum = std::max(entry.maximum, entry.current);
			++entry.samples;
			entry.current = 0.0;
			entry.touched = false;
		}

		Entry& get(std::string_view tag, Kind kind, std::string_view unit) {
			Entry& entry = entries[std::string{tag}];
			entry.kind = kind;
			if (!unit.empty()) {
				entry.unit = unit;
			}
			return entry;
		}

		void add_value(std::string_view tag, f64 value, Kind kind, std::string_view unit) {
			Entry& entry = get(tag, kind, unit);
			entry.current += value;
			entry.touched = true;
		}
	} // namespace

	void begin_frame() {
		for (auto& [tag, entry] : entries) {
			(void)tag;
			finalize(entry);
		}
	}

	void set(std::string_view tag, f64 value, std::string_view unit) {
		Entry& entry = get(tag, Kind::Gauge, unit);
		entry.current = value;
		entry.touched = true;
	}

	void add(std::string_view tag, f64 value, std::string_view unit) {
		add_value(tag, value, Kind::Gauge, unit);
	}

	void count(std::string_view tag, u64 amount) {
		add_value(tag, static_cast<f64>(amount), Kind::Counter, "");
	}

	std::vector<Snapshot> snapshots() {
		std::vector<Snapshot> result;
		result.reserve(entries.size());
		for (const auto& [tag, entry] : entries) {
			const bool has_current = entry.touched;
			const f64 value = has_current ? entry.current : entry.latest;
			const u64 sample_count = entry.samples + static_cast<u64>(has_current);
			const f64 sum = entry.sum + (has_current ? entry.current : 0.0);
			const f64 minimum =
				sample_count == 0
					? 0.0
					: std::min(entry.minimum, has_current ? entry.current : entry.minimum);
			const f64 maximum =
				sample_count == 0
					? 0.0
					: std::max(entry.maximum, has_current ? entry.current : entry.maximum);
			result.push_back(Snapshot{
				.tag = tag,
				.unit = entry.unit,
				.kind = entry.kind,
				.value = value,
				.average = sample_count == 0 ? 0.0 : sum / static_cast<f64>(sample_count),
				.minimum = minimum,
				.maximum = maximum,
				.samples = sample_count,
			});
		}
		std::ranges::sort(result, {}, &Snapshot::tag);
		return result;
	}

	ScopedTimer::ScopedTimer(std::string_view timer_tag) : tag(timer_tag), started(Clock::now()) {
	}

	ScopedTimer::~ScopedTimer() {
		stop();
	}

	ScopedTimer::ScopedTimer(ScopedTimer&& other) noexcept
		: tag(std::move(other.tag)), started(other.started), running(other.running) {
		other.running = false;
	}

	void ScopedTimer::stop() {
		if (!running) {
			return;
		}
		const auto elapsed = std::chrono::duration<f64, std::milli>{Clock::now() - started}.count();
		add_value(tag, elapsed, Kind::Timing, "ms");
		running = false;
	}

	ScopedTimer time(std::string_view tag) {
		return ScopedTimer{tag};
	}

} // namespace metrics
