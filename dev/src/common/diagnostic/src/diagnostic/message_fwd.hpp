// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
