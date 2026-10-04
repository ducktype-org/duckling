/**
 * @file message_fwd.hpp
 *
 * @brief Forward declaration of the MessageBase class and its Box pointer type.
 */
#pragma once

#include <base/pointers/box.hpp>

namespace dia {
	class MessageBase;
}

DEFAULT_BOX_PTR_DELETER_DECLARATION(dia::MessageBase);
