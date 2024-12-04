#pragma once

#include "event.hpp"

namespace vm {
	class Service {
	public:
		virtual ~Service() = 0;

		virtual void handleEvent(const Event& event) = 0;
	};

	inline Service::~Service() {}
}
