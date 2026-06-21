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
	class CTVLowering {
	public:
		/**
		 * @brief Lowers a compile-time value into a DVM value usable inside an instruction stream.
		 *
		 * Values representable as a single immediate (numeric, bool, char, meta type) are returned
		 * directly as a `DVMImmediate`. Everything else is materialized as a global constant via
		 * @ref lowerCtvToGlobal and that global's place is returned.
		 */
		static DVMValue lowerValue(FunctionLoweringContext& fctx, const lir::LIRConstant& constant);

		/**
		 * @brief Construct a new global with a compile-time value and return a place referring to
		 * it. It may use the constructor or an initial value constant, depending on the value being
		 * lowered.
		 *
		 * @param constant The compile-time value that the global will be initialized with.
		 * @param type The DVM type of the global.
		 * @param is_constant Whether the global being created should be marked constant.
		 * @param lowered_global_name If provided, the global will be created with this name.
		 * Otherwise, a fresh name will be generated.
		 */
		static const DVMPlace& lowerCTVToNewGlobal(
			ProgramLoweringContext&      pctx,
			const ctv::CompileTimeValue& constant,
			const vm::code::TypeOfData&  type,
			bool                         is_constant,
			base::Optional<base::StrID>  lowered_global_name = {}
		);

	private:
		/**
		 * @brief Non-trivial value lowerers can return the function to that initializes the global.
		 */
		struct CtorLoweringResult {
			vm::code::Function ctor;
		};

		/**
		 * @brief Small helper that lowers a string literal into a static global
		 * and returns the constructor that assembles the slice pointing to it.
		 * The constructor should be used as the string literal global `ctor`.
		 */
		static CtorLoweringResult lowerStringLiteral(
			ProgramLoweringContext& pctx,
			base::StrID             global_name,
			const DVMPlace&         inserted_global_place,
			base::StrID             content
		);

		/**
		 * @brief Helper setting the fields of a structure one by one from a list of values.
		 *
		 * @param destination The place of the structure to construct.
		 * @param structure_type The type of the structure to construct.
		 * @param values The values to set the structure's fields to. The order must match the order
		 * of the fields in the structure type.
		 */
		static void constructStructureFromValues(
			FunctionLoweringContext&     ctor_ctx,
			const DVMPlace&              destination,
			const vm::code::DataType&    structure_type,
			const std::vector<DVMValue>& values
		);
	};
}
