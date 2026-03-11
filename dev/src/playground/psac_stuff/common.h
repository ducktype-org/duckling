#pragma once

#include <psac/psac.hpp>

std::mutex cout_mutex;

#define LOG_WORKER(TEXT)                           \
	{                                              \
		std::lock_guard lock(cout_mutex);          \
		std::cout << "[WORKER]: " << TEXT << '\n'; \
	}

#define LOG(TEXT)                                    \
	{                                                \
		std::lock_guard lock(cout_mutex);            \
		std::cout << "[LOGGER]: " << TEXT << '\n'; \
	}
