// Translation unit so the (otherwise header-only) StateMachine library produces an
// object file; macOS `ar` rejects empty static archives.
#include "state_machine.hpp"
