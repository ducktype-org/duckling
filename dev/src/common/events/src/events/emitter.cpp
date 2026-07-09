// Translation unit so the (otherwise header-only) Events library produces an object
// file; macOS `ar` rejects empty static archives.
#include "emitter.hpp"
