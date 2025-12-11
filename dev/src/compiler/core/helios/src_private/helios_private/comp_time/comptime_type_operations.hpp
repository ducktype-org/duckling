#pragma once

#include "typesystem/higher/queries/types.hpp"
#include "typesystem/higher/symbol_type.hpp"

#include "base/collections/optional.hpp"

#include "query_framework/context.hpp"
#include "string_id/string_id.hpp"

#include "vm/api/data/process_info.hpp"
#include "vm/bytecode/bytecode.hpp"
#include <vm/bytecode/extern_c_function.hpp>

namespace compiler::helios::comptime_ops {
	/**
	 * @brief Builder for constructing variant types.
	 */
	struct VariantTypeBuilder {
		std::vector<tsh::SymbolType<>> subtypes;

		tsh::SymbolType<> produce(query::Context& ctx) {
			return tsh::SymbolType<>{
				ctx.query<tsh::QueryVariantType>({ subtypes }),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			};
		}
	};

	/**
	 * @brief Builder for constructing tuple types.
	 */
	struct TupleTypeBuilder {
		std::vector<tsh::SymbolType<>> subtypes;

		tsh::SymbolType<> produce(query::Context& ctx) {
			return tsh::SymbolType<>{
				ctx.query<tsh::QueryTupleType>({ subtypes }),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			};
		}
	};

	/**
	 * @brief Builder for constructing function types.
	 */
	struct FunctionTypeBuilder {
		base::Optional<tsh::SymbolType<>> return_type;
		std::vector<tsh::SymbolType<>>    arg_types;

		tsh::SymbolType<> produce(query::Context& ctx) {
			return tsh::SymbolType<>{
				ctx.query<tsh::QueryFunctionType>({ arg_types, return_type.value() }),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			};
		}
	};

	/**
	 * @brief Builder for constructing struct types.
	 * TODOP: Maybe remove in this PR
	 */
	struct StructTypeBuilder {
		struct Field {
			base::StrID       name;
			tsh::SymbolType<> type;
		};

		std::vector<Field> fields;
	};

	// TODOP: Comment.
	vm::code::CodeCollection getComptimeTypeOperations(vm::PID pid);
}
