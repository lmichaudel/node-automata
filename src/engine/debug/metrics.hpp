#pragma once

#include "common/types.hpp"

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace metrics {

	enum class Kind : u8 {
		Timing,
		Counter,
		Gauge,
	};

	struct Snapshot {
		std::string tag;
		std::string unit;
		Kind kind{Kind::Gauge};
		f64 value{0.0};
		f64 average{0.0};
		f64 minimum{0.0};
		f64 maximum{0.0};
		u64 samples{0};
	};

	// Starts a new metrics frame. Values added more than once in a frame are accumulated.
	void begin_frame();
	void set(std::string_view tag, f64 value, std::string_view unit = {});
	void add(std::string_view tag, f64 value, std::string_view unit = {});
	void count(std::string_view tag, u64 amount = 1);
	std::vector<Snapshot> snapshots();

	class ScopedTimer {
	  public:
		explicit ScopedTimer(std::string_view tag);
		~ScopedTimer();
		ScopedTimer(const ScopedTimer&) = delete;
		ScopedTimer& operator=(const ScopedTimer&) = delete;
		ScopedTimer(ScopedTimer&& other) noexcept;
		ScopedTimer& operator=(ScopedTimer&&) = delete;

		void stop();

	  private:
		using Clock = std::chrono::steady_clock;

		std::string tag;
		Clock::time_point started;
		bool running{true};
	};

	[[nodiscard]] ScopedTimer time(std::string_view tag);

} // namespace metrics

#define METRIC_DETAIL_JOIN_INNER(a, b) a##b
#define METRIC_DETAIL_JOIN(a, b) METRIC_DETAIL_JOIN_INNER(a, b)
#define METRIC_SCOPE(tag)                                                                          \
	[[maybe_unused]] const auto METRIC_DETAIL_JOIN(metric_scope_, __LINE__) = metrics::time(tag)
