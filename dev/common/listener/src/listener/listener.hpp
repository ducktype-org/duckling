#pragma once

/**
 * Class that listens to a particular Event and executes a function on that
 * event
 * @tparam Event
 */
template<class Event>
class Listener {
public:
	virtual void onEvent(const Event& event) noexcept = 0;

	virtual ~Listener() noexcept = default;
};
