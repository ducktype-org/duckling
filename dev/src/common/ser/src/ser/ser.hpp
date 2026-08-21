#pragma once
 
#include <ser/config.hpp>
#include <ser/errc.hpp>
#include <ser/tags.hpp>
#include <ser/debug.hpp>
#include <ser/concepts.hpp>
 
#include <ser/detail/meta.hpp>
#include <ser/detail/ovf.hpp>
#include <ser/detail/bytes.hpp>
#include <ser/detail/adl.hpp>

#include <ser/serializer.hpp>
#include <ser/type_config.hpp>
#include <ser/access.hpp>
#include <ser/pool/context.hpp>

#include <ser/archive/buffer.hpp>
#include <ser/archive/out.hpp>
#include <ser/archive/in.hpp>

#include <ser/builtin/scalar.hpp>
#include <ser/builtin/enum.hpp>
#include <ser/builtin/array.hpp>
#include <ser/builtin/pointer_deny.hpp>

#include <ser/detail/describe.hpp>
#include <ser/detail/dispatch.hpp>

#include <ser/hash.hpp>
#include <ser/stream/header.hpp>
#include <ser/stream/read_write.hpp>

namespace ser {
    inline constexpr int version = SER_VERSION;
 
}  // namespace ser
 