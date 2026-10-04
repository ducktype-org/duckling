
#include <init/init.hpp>

int main() {
	init::InitObject _;
	return 0;
	/**
	 * Linter fails on exit(0) because it's not thread-safe.
	 */
}
