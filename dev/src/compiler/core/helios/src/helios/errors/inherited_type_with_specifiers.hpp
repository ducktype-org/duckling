#pragma once

#include <diagnostic_interactive/message.hpp>
#include <diagnostic_interactive/stable_position.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <query_framework/context/context_fd.hpp>

#include <string>
#include <vector>

namespace compiler::helios {


	/**
	 * @brief Error reported when the extended class or an implemented interface of a class
	 * declaration carries type specifiers.
	 *
	 * A class inherits from a plain type, so specifiers such as `ref`, `box` or `const` are
	 * meaningless there and are rejected instead of being silently dropped.
	 */
	class InheritedTypeWithSpecifiersError final: public dia_int::MessageWithCodeFragmentAndCause {
		[[nodiscard]] dia_int::Metadata getMetadata() const final {
			return {
				.template_type = "message",
				.type          = "error",
				.family        = "type_check",
				.name          = "inherited_type_with_specifiers",
			};
		}

	public:
		/**
		 * @brief Tells whether an inherited type is the extended class or an implemented interface.
		 */
		enum class InheritanceKind {
			ExtendedClass,
			ImplementedInterface,
		};

		InheritedTypeWithSpecifiersError(
			query::Context&         ctx,
			dia_int::StablePosition source_position,
			std::string             class_name,
			InheritanceKind         inheritance_kind,
			tsh::SymbolType<>       inherited_type
		);
	};

}
