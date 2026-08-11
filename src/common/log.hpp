#pragma once

#include <SDL3/SDL_log.h>
#include <format>
#include <string>

namespace log {
	template <typename... Args>
	inline void info(std::format_string<Args...> fmt, Args&&... args) {
		std::string message = std::format(fmt, std::forward<Args>(args)...);
		SDL_Log("%s", message.c_str());
	}

	template <typename... Args>
	inline void error(std::format_string<Args...> fmt, Args&&... args) {
		std::string message = std::format(fmt, std::forward<Args>(args)...);
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message.c_str());
	}

	template <typename... Args>
	inline void warn(std::format_string<Args...> fmt, Args&&... args) {
		std::string message = std::format(fmt, std::forward<Args>(args)...);
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", message.c_str());
	}
} // namespace log