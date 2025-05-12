#pragma once

#include "common.hpp"
#include "interactive_message.hpp"

#include <base/box.hpp>

#include <string_view>

namespace dia {
	class InteractiveLogger {
	private:
		std::vector<nlohmann::json> messages{};
		bool                        is_logging = false;
		SerializationParams         m_params{};

		static InteractiveLogger& get() {
			static InteractiveLogger logger;
			return logger;
		}

		void m_log(base::Box<InteractiveMessage> message);

		InteractiveLogger()  = default;
		~InteractiveLogger() = default;

	public:
		bool dump_static = true;

		static void log(base::Box<InteractiveMessage> message) {
			InteractiveLogger& logger = InteractiveLogger::get();
			logger.m_log(std::move(message));
		}

		static bool logging() { return InteractiveLogger::get().is_logging; }

		static void set_logging(bool is_logging) {
			InteractiveLogger::get().is_logging = is_logging;
		}

		static SerializationParams& params() { return InteractiveLogger::get().m_params; }

		static void set_params(const SerializationParams& params) {
			InteractiveLogger::get().m_params = params;
		}

		void dump(std::string_view filename);
	};
}

#define LOG_ERROR(ClassName, ...)                                           \
	if (!dia::InteractiveLogger::logging()) {                               \
		dia::InteractiveLogger::set_logging(true);                          \
		dia::InteractiveLogger::log(base::makeBox<ClassName>(__VA_ARGS__)); \
		dia::InteractiveLogger::set_logging(false);                         \
	}
