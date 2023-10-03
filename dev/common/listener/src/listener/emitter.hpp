#pragma once

#include "listener.hpp"
#include <base/ints.hpp>
#include <concepts>
#include <queue>
#include <set>

template<class Event>
class Emitter {
private:
	std::set<Listener<Event>*> listeners;
	std::queue<Event>          eventQueue;

public:
	void addEvent(const Event& event) noexcept
	requires std::copy_constructible<Event>
	{
		eventQueue.push(event);
	}

	void addEvent(Event&& event) noexcept
	requires std::move_constructible<Event>
	{
		eventQueue.emplace(std::move(event));
	}

	void fire(const Event& event) noexcept {
		for (auto l: listeners) l->onNotify(event);
	}

	void processEvents() noexcept {
		while (!eventQueue.empty()) {
			fire(eventQueue.front());
			eventQueue.pop();
		}
	}

	/**
	 * Adds listener to the emitter's notify list.<br>
	 * Unless manually erased, the emitter will clean added listener during it's destruction.
	 * @param listener
	 */
	void attach(Listener<Event>* listener) noexcept { listeners.emplace(listener); }

	void detach(Listener<Event>* listener) noexcept { listeners.erase(listener); }

	void removeAllListeners() noexcept { listeners.clear(); }

	[[nodiscard]]
	usize listenerCount() const noexcept {
		return listeners.size();
	}
};
