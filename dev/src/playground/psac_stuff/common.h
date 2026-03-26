#pragma once
#include <mutex>

std::mutex cout_mutex;

#ifndef NDEBUG
	#define LOG_WORKER(TEXT)                           \
		{                                              \
			std::lock_guard lock(cout_mutex);          \
			std::cout << "[WORKER]: " << TEXT << '\n'; \
		}

	#define LOG(TEXT)                                  \
		{                                              \
			std::lock_guard lock(cout_mutex);          \
			std::cout << "[LOGGER]: " << TEXT << '\n'; \
		}

#else
	#define LOG_WORKER(TEXT)
	#define LOG(TEXT)
#endif
