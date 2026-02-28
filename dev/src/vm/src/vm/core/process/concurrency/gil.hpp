#pragma once

#include <mutex>
#include <base/types/ints.hpp>

namespace vm {

	class GIL final {

	private:      
		/**
		 * @brief Main GIL mutex for a process.
		 * It stems from assumption that only one thread can be executing DVM code at the time.
		 */
		std::mutex gil;

		/**
		 * @brief Count of VM operations from the last time GIL was acquired.
		 */
		u64 operations = 0;

		/**
		 * @brief Maximum amount of operations that can be executed by thread without giving up GIL.
		 */
		const static u64 MAX_GIL_OPERATIONS = 50;
    
    public:

    	/**
		 * @brief Acquires GIL. If you leave this function you always have right to interpret DVM
		 * code.
		 */
		void acquire();

		/**
		 * @brief Releases GIL. If you leave this function you no longr can interpret DVM code.
		 * It also zeroes operations counter, for the next person to take GIL.
		 */
		void release();

		/**
		* @brief Decides whether current thread should give up GIL based on set GIL policy.
		In the future it will have seprarte interace, for now it is simple counter.
		*/
		bool shouldRelease();
    };
}