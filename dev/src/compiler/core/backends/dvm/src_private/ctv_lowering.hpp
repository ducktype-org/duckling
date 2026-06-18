#pragma once

#include "dvm_value.hpp"

#include <ctv/ctv.hpp>
#include <tsl/type_layout.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/const_value.hpp>
#include <vm/bytecode/type_of_data.hpp>

namespace compiler::lir {
	struct LIRConstant;
}

namespace compiler::backend_vm::internal {
	class FunctionLoweringContext;
	class ProgramLoweringContext;

	/**
	 * @brief Compile-time-value lowering for the DVM backend.
	 *
	 * Grouped as a struct so both context classes need to befriend only this one type to give the
	 * lowering routines access to their internals.
	 */
	class CtvLowering {
	public:
		/**
		 * @brief Lowers a compile-time value into a DVM value usable inside an instruction stream.
		 *
		 * Values representable as a single immediate (numeric, bool, char, meta type) are returned
		 * directly as a `DVMImmediate`. Everything else is materialized as a global via
		 * @ref lowerCtvToGlobal and that global's place is returned (an r-value source, copied at
		 * the use site).
		 */
		static DVMValue lowerValue(FunctionLoweringContext& fctx, const lir::LIRConstant& constant);

		static const DVMPlace& lowerCtvToNewGlobal(
			ProgramLoweringContext&      pctx,
			const ctv::CompileTimeValue& constant,
			const vm::code::TypeOfData&  type,
			bool                         is_constant,
			base::Optional<base::StrID>  lowered_global_name = {}
		);

	private:
		struct CtorLoweringResult {
			vm::code::Function ctor;
		};

		static CtorLoweringResult lowerStringLiteral(
			ProgramLoweringContext&     pctx,
			base::StrID                 global_name,
			const DVMPlace&             inserted_global_place,
			base::StrID                 content
		);

		static void constructStructureFromValues(
			FunctionLoweringContext&  ctor_ctx,
			const DVMPlace&           destination,
			const vm::code::DataType& structure_type,
			std::vector<DVMValue>     values
		);
	};
}
