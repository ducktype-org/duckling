#pragma once

#include "base/optional.hpp"
#include <type_traits>
#include <variant>

namespace vm {
	struct Event {
		struct BaseEvent {
            // TODO: VCPU id and so on...
        };

		struct MemoryEvent: public BaseEvent {};

		struct FunctionCallEvent: public BaseEvent {};

		template<class Ev>
		Event(const Ev& e): data(e) {}

		Event() = default;

		template<class Ev>
		[[nodiscard]]
		base::Optional<Ev> get() const {
			if (std::holds_alternative<Ev>(data)) return std::optional<Ev>(std::get<Ev>(data));
			return {};
		}

		template<class Ev>
		[[nodiscard]]
		bool is() const {
			return std::holds_alternative<Ev>(data);
		}

	private:
		std::variant<MemoryEvent, FunctionCallEvent> data;
	};
    static_assert(std::is_trivially_copyable_v<Event>);
}
