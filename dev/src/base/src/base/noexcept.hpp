#pragma once

#if defined(BUILD_TYPE_DEBUG) || defined(BUILD_TYPE_DEVRELEASE)
	/**
     * @brief Function will not throw in Release build
     * */
	#define NOEXCEPT noexcept(false)
#else
	/**
     * @brief Function will not throw in Release build
     * */
	#define NOEXCEPT noexcept(true)
#endif
