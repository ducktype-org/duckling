#pragma once

#include <ser/access.hpp>
#include <ser/archive/buffer.hpp>
#include <ser/archive/in.hpp>
#include <ser/archive/out.hpp>
#include <ser/builtin/array.hpp>
#include <ser/builtin/enum.hpp>
#include <ser/builtin/pointer_deny.hpp>
#include <ser/builtin/scalar.hpp>
#include <ser/concepts.hpp>
#include <ser/config.hpp>
#include <ser/debug.hpp>
#include <ser/detail/adl.hpp>
#include <ser/detail/bytes.hpp>
#include <ser/detail/describe.hpp>
#include <ser/detail/dispatch.hpp>
#include <ser/detail/meta.hpp>
#include <ser/detail/ovf.hpp>
#include <ser/errc.hpp>
#include <ser/hash.hpp>
#include <ser/pool/context.hpp>
#include <ser/serializer.hpp>
#include <ser/stream/header.hpp>
#include <ser/stream/read_write.hpp>
#include <ser/tags.hpp>
#include <ser/type_config.hpp>

namespace ser {
	inline constexpr int VERSION = SER_VERSION;

}  // namespace ser
