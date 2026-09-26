#pragma once
#include <nlohmann/json.hpp>

#include <string>
#include <utility>

namespace dap {

	enum class EventType { Initialized, Stopped, Terminated, Exited, Output, Breakpoint };

	inline std::string_view toString(EventType type) {
		switch (type) {
		case EventType::Initialized:
			return "initialized";
		case EventType::Stopped:
			return "stopped";
		case EventType::Terminated:
			return "terminated";
		case EventType::Exited:
			return "exited";
		case EventType::Output:
			return "output";
		case EventType::Breakpoint:
			return "breakpoint";
		}
		return "unknown";
	}

	struct Event {
		virtual ~Event() = default;

		[[nodiscard]] virtual EventType getType() const = 0;

		[[nodiscard]] virtual bool hasBody() const { return false; }

		[[nodiscard]] virtual nlohmann::json getBody() const { return {}; }
	};

	struct InitializedEvent final: public Event {
		[[nodiscard]] EventType getType() const override { return EventType::Initialized; }
	};

	struct TerminatedEvent final: public Event {
		[[nodiscard]] EventType getType() const override { return EventType::Terminated; }
	};

	struct ExitedEvent final: public Event {
		int64_t exitCode;  // NOLINT(readability-identifier-naming)

		explicit ExitedEvent(int64_t code): exitCode(code) {}

		[[nodiscard]] EventType getType() const override { return EventType::Exited; }

		[[nodiscard]] bool hasBody() const override { return true; }

		[[nodiscard]] nlohmann::json getBody() const override;
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ExitedEvent, exitCode)

	inline nlohmann::json ExitedEvent::getBody() const { return *this; }

	struct StoppedEvent final: public Event {
		std::string reason;
		uint64_t    threadId;                  // NOLINT(readability-identifier-naming)
		bool        allThreadsStopped = true;  // NOLINT(readability-identifier-naming)

		StoppedEvent(std::string r, uint64_t tid, bool ats = true):
			  reason(std::move(r)),
			  threadId(tid),
			  allThreadsStopped(ats) {}

		[[nodiscard]] EventType getType() const override { return EventType::Stopped; }

		[[nodiscard]] bool hasBody() const override { return true; }

		[[nodiscard]] nlohmann::json getBody() const override;
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(StoppedEvent, reason, threadId, allThreadsStopped)

	inline nlohmann::json StoppedEvent::getBody() const { return *this; }

	struct Breakpoint final {
		bool                       verified;
		std::optional<uint64_t>    line;
		std::optional<std::string> message;

		[[nodiscard]] nlohmann::json toJson() const {
			nlohmann::json j = { { "verified", verified } };

			if (line.has_value()) j["line"] = line.value();
			if (message.has_value()) j["message"] = message.value();

			return j;
		}
	};

	struct BreakpointEvent final: public Event {
		std::string reason;
		Breakpoint  breakpoint;

		BreakpointEvent(std::string r, Breakpoint bp):
			  reason(std::move(r)),
			  breakpoint(std::move(bp)) {}

		[[nodiscard]] EventType getType() const override { return EventType::Breakpoint; }

		[[nodiscard]] bool hasBody() const override { return true; }

		[[nodiscard]] nlohmann::json getBody() const override {
			return { { "reason", reason }, { "breakpoint", breakpoint.toJson() } };
		}
	};

	struct OutputEvent final: public Event {
		std::string output;
		std::string category = "stdout";

		explicit OutputEvent(std::string out, std::string cat):
			  output(std::move(out)),
			  category(std::move(cat)) {}

		[[nodiscard]] EventType getType() const override { return EventType::Output; }

		[[nodiscard]] bool hasBody() const override { return true; }

		[[nodiscard]] nlohmann::json getBody() const override;
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(OutputEvent, output, category)

	inline nlohmann::json OutputEvent::getBody() const { return *this; }
}
