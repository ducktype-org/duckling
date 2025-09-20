#include "timer.hpp"

namespace timer {

	void printAs(std::ostream& out, const Duration& duration, TimeUnit unit) {
		switch (unit) {
		case TimeUnit::Nanoseconds:
			out << std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() << " ns";
			break;
		case TimeUnit::Microseconds:
			out << std::chrono::duration_cast<std::chrono::microseconds>(duration).count() << " µs";
			break;
		case TimeUnit::Milliseconds:
			out << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << " ms";
			break;
		case TimeUnit::Seconds:
			out << std::chrono::duration_cast<std::chrono::seconds>(duration).count() << " s";
			break;
		}
	}
}
