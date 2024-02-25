#pragma once

#include <map>
#include <set>
#include <string>

#include <base/optional.hpp>

#include "type_info.hpp"

namespace ts {
    enum class Visibility {
        Public,
        Protected,
        Private
    };

    class InterfaceElement final {
        /**
         * \brief The input parameter types of this element of the interface.
         * If the optional is empty, then the element is a field.
         *
         * Otherwise, if the vector inside the optional is empty, then it is a parameterless method.
         */
        const base::Optional<std::vector<TypeInfo>> parameter_types;

        /**
         * \brief The type of a field or the return type of a method.
         */
        const TypeInfo result_type;

        /**
         * \brief Where the element was declared.
         *
         * For example, if a class A implements an interface I which defines method foo,
         * then it is important that the foo method in A is in actuality I.foo.
         * In this context, I is the source of A.foo.
         */
        const TypeInfo source;

        const Visibility visibility;
    };

    class TypeInterface final {
        std::map<std::string, std::set<InterfaceElement>> elements{};
    };
}
