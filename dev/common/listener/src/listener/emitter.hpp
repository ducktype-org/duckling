#pragma once

#include <concepts>
#include <queue>
#include <set>

#include <base/ints.hpp>
#include <base/ref.hpp>

#include "listener.hpp"

/**
 * @brief Class for emitting a target `Event`.
 * Supports immediate firing of an event as well as a scheduling.
 */
template<class Event>
requires std::copy_constructible<Event> class Emitter {
private:
	std::set<Ref<Event>> listeners;
	std::queue<Event>    eventQueue;

public:
	void queueEvent(const Event& event) { eventQueue.push(event); }

	void fireEvent(const Event& event) {
		for (auto l: listeners) l->onNotify(event);
	}

	void processEvents() {
		while (!eventQueue.empty()) {
			fireEvent(eventQueue.front());
			eventQueue.pop();
		}
	}

	/**
	 * Adds listener to the emitter's notify list.
	 * Unless manually erased, the emitter will clean added listener during it's destruction.
	 * @param listener
	 */
	void attach(Ref<Event> listener) { listeners.emplace(listener); }

	void detach(Ref<Event> listener) { listeners.erase(listener); }

	void removeAllListeners() { listeners.clear(); }

	[[nodiscard]]
	usize listenerCount() const {
		return listeners.size();
	}
};
