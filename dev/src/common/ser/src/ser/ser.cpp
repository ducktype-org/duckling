#include <ser/ser.hpp>       // IWYU pragma: keep
#include <ser/std/all.hpp>   // IWYU pragma: keep
#include <ser/test.hpp>      // IWYU pragma: keep

namespace ser::duckling {

	/**
	 * @brief Anchor for the header-only library.
	 *
	 * The includes above are the point of this translation unit: they make a plain
	 * build compile every ser header, instead of leaving that to whichever test or
	 * user happens to include them. The symbol itself only keeps the static archive
	 * from being empty.
	 */
	extern const int MODULE_VERSION;

	const int MODULE_VERSION = SER_VERSION;

}
