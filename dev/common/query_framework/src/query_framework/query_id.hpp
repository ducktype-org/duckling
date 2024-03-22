#pragma once

#include <base/ints.hpp>

namespace query {
	struct QueryID {
	// private: @TODO
		u64 val;
		constexpr u64 asInt() const { return val; }
	};

	/**
	 * @brief A simple counter for providing unique query id-s.
	 * This function should never be used outside the framework.
	 */
	QueryID nextQueryId();

	/**
	 * @brief Provides query id of "outside world" query.
	 * This function should never be used outside the framework.
	 */
	QueryID outsideWorldQueryID();

}

