#pragma once

#include <base/pointers/ref.hpp>

#include <set>

namespace events {

	/**
	 * @brief A wrapper to function that is executed on emission
	 *
	 * @tparam Event Argument of the function
	 */
	template<class Event>
	class Emitter;

	template<class Event>
	class Listener {
		friend class Emitter<Event>;
		using Handler = std::function<void(Event)>;

		Handler              handler;
		MRef<Emitter<Event>> emitter;

	public:
		explicit Listener(Handler&& handler): handler(std::move(handler)) {}

		/**
		 * @brief If Listener is attached to some Emitter then detaches from it
		 */
		void detach() {
			if (emitter) emitter->detachListener(Ref(this));
		}

		~Listener() { detach(); }
	};

	/**
	 * @brief Simple class to propagate events using Listeners
	 *
	 * @tparam Event Argument of the functions
	 */
	template<class Event>
	class Emitter {
		std::set<Ref<Listener<Event>>> listeners;

	public:
		/**
		 * @brief Attach Listener to the Emitter
		 */
		void attachListener(Ref<Listener<Event>> listener) {
			listener->detach();
			listeners.insert(listener);
			listener->emitter = this;
		}

		/**
		 * @brief Attach Listener to the Emitter
		 */
		void attachListener(Listener<Event>& listener) { attachListener(&listener); }

		/**
		 * @brief Detach Listener from the Emitter
		 */
		void detachListener(Ref<Listener<Event>> listener) {
			listeners.erase(listener);
			listener->emitter = nullptr;
		}

		/**
		 * @brief Detach Listener from the Emitter
		 */
		void detachListener(Listener<Event>& listener) { detachListener(&listener); }

		/**
		 * @brief Emit event to all attached Listeners
		 */
		void emitEvent(const Event& event) const {
			auto copy = listeners;
			for (const auto& listener: copy) listener->handler(event);
		}

		~Emitter() {
			auto copy = listeners;
			for (const auto& listener: copy) listener->detach();
		}
	};
}
