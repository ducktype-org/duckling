#pragma once

#include <base/extend_cpp/strongly_typed_id.hpp>
#include <string_id/string_id.hpp>
#include <base/collections/optional.hpp>

namespace pg::ns {
    STRONG_TYPEDEF_ID_DIRECT_CREATION(NamespaceID);

    struct Namespace {
        base::StrID name;
        NamespaceID id;
        base::Optional<NamespaceID> parent_id;
    };
}

ID_STD_HASH(pg::ns::NamespaceID);
