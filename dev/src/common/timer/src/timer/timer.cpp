#include "timer.hpp"

namespace timer {

	void printAs(std::ostream& out, const Duration& duration, TimeUnit unit) {
		switch (unit) {
		case TimeUnit::Nanoseconds:
			out << std::chrono::duration_cast<std::chrono::nanoseconds>(duration.value).count() << " ns";
			break;
		case TimeUnit::Microseconds:
			out << std::chrono::duration_cast<std::chrono::microseconds>(duration.value).count() << " µs";
			break;
		case TimeUnit::Milliseconds:
			out << std::chrono::duration_cast<std::chrono::milliseconds>(duration.value).count() << " ms";
			break;
		case TimeUnit::Seconds:
			out << std::chrono::duration_cast<std::chrono::seconds>(duration.value).count() << " s";
			break;
		}
	}
}
