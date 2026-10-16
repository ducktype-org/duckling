// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <base/str/str_utils.hpp>

#include <vm/bytecode/validator/errors.hpp>

/**
 * @brief What belongs here: the loader's static verification. A test loads a program that parses
 * fine and checks that the loader accepts it (`right/`) or rejects it with the right error
 * (`wrong/<area>/`). Every semantic error found before a program runs belongs here: unknown names,
 * labels, jumps, locals, types (and their `assert_size`), C pointer instructions, globals,
 * literals, function signatures and calls, and the execution config (IO, single thread, read-only
 * globals).
 *
 * Inheritance errors have their own suite in `loader/inheritance/`. FFI errors need the test
 * library, so they have their own suite in `loader/ffi/`.
 */
class BCVerificationTests: public VmLoaderTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCVerificationTests

public:
	VM_LOADER_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(jumpSkipBlock);
		TESTER_ADD_TEST(manyJumps);
		TESTER_ADD_TEST(initDeinit);
		TESTER_ADD_TEST(twoInits);

		// Function verification

		// @TODO: #2895 restore this check when possible
		// TESTER_ADD_TEST(multipleFunctions);
		TESTER_ADD_TEST(useArgumentAfterCall);
		TESTER_ADD_TEST(mainVerification);
		TESTER_ADD_TEST(multipleRetVals);
		TESTER_ADD_TEST(deinitMainRetVal);
		TESTER_ADD_TEST(invalidBuiltinFunction);
		TESTER_ADD_TEST(pathWithoutEnd);
		TESTER_ADD_TEST(invalidSignatures);
		TESTER_ADD_TEST(unknownFunction);
		TESTER_ADD_TEST(invalidFunctionCall);
		TESTER_ADD_TEST(invalidRetTypeInCall);

		// Jump verification
		TESTER_ADD_TEST(jumpBetween);
		TESTER_ADD_TEST(jumpIntoBlock);
		TESTER_ADD_TEST(jumpOutOfBlock);
		TESTER_ADD_TEST(duplicatedDataFields);
		TESTER_ADD_TEST(unknownMethod);
		TESTER_ADD_TEST(jumpWithInplaceCast);
		TESTER_ADD_TEST(validJumpOutOfBlock);

		// Label verification
		TESTER_ADD_TEST(multipleLabels);
		TESTER_ADD_TEST(labelNotFound);

		// Local variable verification
		TESTER_ADD_TEST(noInit);
		TESTER_ADD_TEST(beforeInit);
		TESTER_ADD_TEST(afterDeinit);
		TESTER_ADD_TEST(invalidName);
		TESTER_ADD_TEST(repeatedName);
		TESTER_ADD_TEST(wrongDowncast);
		TESTER_ADD_TEST(wrongPtrMov);

		// @note: Not implemented yet
		TESTER_ADD_TEST(derefWrongType);
		TESTER_ADD_TEST(refOnPrimitive);

		// Type verification
		TESTER_ADD_TEST(correctDefinitions);
		TESTER_ADD_TEST(unknownType);
		TESTER_ADD_TEST(incorrectDefinitions);
		TESTER_ADD_TEST(duplicatedVariantAlternatives);
		TESTER_ADD_TEST(wrongTypeMov);
		TESTER_ADD_TEST(wrongTypeSize);
		TESTER_ADD_TEST(globalWrongTypeMov);
		TESTER_ADD_TEST(globalWrongTypeSize);
		TESTER_ADD_TEST(usingPointerAsPrimitive);
		TESTER_ADD_TEST(variantWrongType);
		TESTER_ADD_TEST(fixedSizeTableWrongType);
		TESTER_ADD_TEST(inplaceCasts);
		TESTER_ADD_TEST(sizeZeroPrimitive);
		TESTER_ADD_TEST(emptyVariant);
		TESTER_ADD_TEST(nonInstantiableVariant);
		TESTER_ADD_TEST(nonInstantiableDynamicTable);

		// Type size assertions
		TESTER_ADD_TEST(packedStructSize);
		TESTER_ADD_TEST(assertSizeMatches);
		TESTER_ADD_TEST(assertSizeMismatchFails);
		TESTER_ADD_TEST(assertSizeOnPointerTypeFails);

		// C pointer verification
		TESTER_ADD_TEST(cpointerToUnknownTypeFails);
		TESTER_ADD_TEST(movPCptrStrictnessFails);
		TESTER_ADD_TEST(cptrLoadThroughVoidCPtrFails);
		TESTER_ADD_TEST(cptrLoadPointeeMismatchFails);
		TESTER_ADD_TEST(cptrLoadPackedPointeeFails);
		TESTER_ADD_TEST(cptrStoreThroughVoidCPtrFails);
		TESTER_ADD_TEST(cptrStorePointeeMismatchFails);
		TESTER_ADD_TEST(cptrStorePackedPointeeFails);
		TESTER_ADD_TEST(cptrReadThroughVoidCPtrFails);
		TESTER_ADD_TEST(cptrReadPointeeMismatchFails);
		TESTER_ADD_TEST(cptrWriteThroughVoidCPtrFails);
		TESTER_ADD_TEST(cptrWritePointeeMismatchFails);
		TESTER_ADD_TEST(cptrReadIntoPointerPointeeFails);
		TESTER_ADD_TEST(cptrWriteFromPointerPointeeFails);

		// Literal verification
		TESTER_ADD_TEST(invalidLiterals);

		// Execution verification
		TESTER_ADD_TEST(incorrectUsesIO);
		TESTER_ADD_TEST(incorrectWritesToGlobal);
		TESTER_ADD_TEST(callGraphPropagatesIO);
		TESTER_ADD_TEST(crossBatchPropagatesIO);
		TESTER_ADD_TEST(tailCallPropagatesIO);
		TESTER_ADD_TEST(spawningThreadViolatesSingleThread);
		TESTER_ADD_TEST(validProgramPassesConfig);

		// Globals verification
		TESTER_ADD_TEST(wrongGlobalInitializationMethod);
		TESTER_ADD_TEST(wrongGlobalImmSize);
		TESTER_ADD_TEST(wrongGlobalField);
		TESTER_ADD_TEST(wrongGlobalTblSize);
		TESTER_ADD_TEST(globalNoConstructor);
		TESTER_ADD_TEST(globalNoDestructor);
		TESTER_ADD_TEST(invalidGlobalConstructorSignature);
	}

private:
	// False positives
	void jumpSkipBlock() { loadValidDbc("right/jump_skip_block.dbc"); }

	void validJumpOutOfBlock() { loadValidDbc("right/valid_jump_out_of_block.dbc"); }

	void initDeinit() { loadValidDbc("right/init_deinit.dbc"); }

	void manyJumps() { loadValidDbc("right/many_jumps.dbc"); }

	void twoInits() { loadValidDbc("right/two_inits.dbc"); }

	// Function verification
	// void multipleFunctions() {
	// 	loadInvalidDbc(
	// 		"wrong/functions/multiple_functions.dbc",
	// 		{
	// 			"Function with this name already exists.",
	// 		}
	// 	);
	// }

	// Function verification
	void useArgumentAfterCall() {
		loadInvalidDbc(
			"wrong/functions/use_argument_after_call.dbc",
			{
				vm::code::UnknownLocalNameError::ERR_MSG,
			}
		);
	}

	void mainVerification() {
		loadInvalidDbc(
			"wrong/functions/invalid_main_ret_type.dbc", { vm::code::InvalidMainReturnType::ERR_MSG }
		);
		loadInvalidDbc(
			"wrong/functions/invalid_main_ret_size.dbc", { vm::code::InvalidMainReturnType::ERR_MSG }
		);
	}

	void multipleRetVals() {
		loadInvalidDbc(
			"wrong/functions/return_type_mismatch.dbc",
			{ vm::code::InvalidFunctionCallArgumentsError::ERR_MSG }
		);
		loadInvalidDbc(
			"wrong/functions/deinit_ret_val.dbc", { vm::code::RetValDeinitError::ERR_MSG }
		);
	}

	// Jump verification
	void jumpBetween() {
		loadInvalidDbc(
			"wrong/jumps/jump_between.dbc",
			{
				vm::code::StackStructureMismatchError::ERR_MSG,
				vm::code::StackStructureMismatchError::NOTE_MSG,
			}
		);
	}

	void jumpIntoBlock() {
		loadInvalidDbc(
			"wrong/jumps/jump_into_block.dbc",
			{
				vm::code::StackStructureMismatchError::ERR_MSG,
				vm::code::StackStructureMismatchError::NOTE_MSG,
			}
		);
	}

	void jumpOutOfBlock() {
		loadInvalidDbc(
			"wrong/jumps/jump_out_of_block.dbc",
			{
				vm::code::StackStructureMismatchError::ERR_MSG,
				vm::code::StackStructureMismatchError::NOTE_MSG,
			}
		);
	}

	void jumpWithInplaceCast() {
		loadInvalidDbc(
			"wrong/jumps/jump_with_inplace_cast.dbc",
			{
				vm::code::StackStructureMismatchError::ERR_MSG,
				vm::code::StackStructureMismatchError::NOTE_MSG,
			}
		);
	}

	// Deinitialized value verification
	void noInit() {
		loadInvalidDbc(
			"wrong/init_deinit/no_init.dbc",
			{
				vm::code::UnknownLocalNameError::ERR_MSG,
			}
		);
	}

	void beforeInit() {
		loadInvalidDbc(
			"wrong/init_deinit/before_init.dbc",
			{
				vm::code::UnknownLocalNameError::ERR_MSG,
			}
		);
	}

	void afterDeinit() {
		loadInvalidDbc(
			"wrong/init_deinit/after_deinit.dbc",
			{
				vm::code::UnknownLocalNameError::ERR_MSG,
			}
		);
	}

	void invalidName() {
		loadInvalidDbc(
			"wrong/init_deinit/invalid_name.dbc",
			{
				vm::code::UnknownLocalNameError::ERR_MSG,
			}
		);
	}

	void repeatedName() {
		loadInvalidDbc(
			"wrong/init_deinit/repeated_name.dbc",
			{
				vm::code::DuplicatedLocalNameError::ERR_MSG,
			}
		);
	}

	void derefWrongType() {
		loadInvalidDbc(
			"wrong/pointers/deref_wrong_type.dbc",
			{
				vm::code::PointerTypeMismatchError::ERR_MSG,
			}
		);
	}

	void refOnPrimitive() {
		loadInvalidDbc(
			"wrong/pointers/ref_on_primitive.dbc",
			{
				vm::code::InvalidArgumentTypeError::ERR_MSG,
			}
		);
	}

	// Type verification
	void wrongTypeMov() {
		loadInvalidDbc(
			"wrong/types/wrong_type_mov.dbc",
			{
				vm::code::ArgumentMismatchError::ERR_MSG,
			}
		);
	}

	void wrongTypeSize() {
		loadInvalidDbc(
			"wrong/types/wrong_type_size.dbc",
			{
				vm::code::InvalidArgumentSizeError::ERR_MSG,
			}
		);
	}

	void globalWrongTypeMov() {
		loadInvalidDbc(
			"wrong/types/global_wrong_type_mov.dbc",
			{
				vm::code::ArgumentMismatchError::ERR_MSG,
			}
		);
	}

	void globalWrongTypeSize() {
		loadInvalidDbc(
			"wrong/types/global_wrong_type_size.dbc",
			{
				vm::code::InvalidArgumentSizeError::ERR_MSG,
			}
		);
	}

	void usingPointerAsPrimitive() {
		loadInvalidDbc(
			"wrong/types/using_pointer_as_primitive.dbc",
			{
				vm::code::InvalidArgumentTypeError::ERR_MSG,
			}
		);
	}

	void duplicatedDataFields() {
		loadInvalidDbc(
			"wrong/data_type/duplicated_fields.dbc",
			{
				vm::code::DuplicatedFieldError::ERR_MSG,
			}
		);
	}

	void variantWrongType() {
		loadInvalidDbc(
			"wrong/types/variant_wrong_type.dbc",
			{
				vm::code::VariantTypeMismatchError::ERR_MSG,
			}
		);
	}

	void unknownMethod() {
		loadInvalidDbc(
			"wrong/unknown_method.dbc",
			{
				vm::code::InvalidVirtualMethodImplementationError::ERR_MSG,
			}
		);
	}

	void fixedSizeTableWrongType() {
		loadInvalidDbc(
			"wrong/types/fixed_size_table_wrong_type.dbc",
			{
				vm::code::FixedSizeTableTypeMismatchError::ERR_MSG,
			}
		);
	}

	// A valid in-place cast is run in `runtime/primitives/`.
	void inplaceCasts() {
		loadInvalidDbc(
			"wrong/types/inplace_cast_size_mismatch.dbc",
			{
				vm::code::CastSizeMismatchError::ERR_MSG,
			}
		);
	}

	void correctDefinitions() { loadValidDbc("right/correct_definitions.dbc"); }

	void incorrectDefinitions() {
		loadInvalidDbc(
			"wrong/types/cyclic_dependency.dbc",
			{
				vm::code::CyclicDependencyError::ERR_MSG,
			}
		);
	}

	void duplicatedVariantAlternatives() {
		loadInvalidDbc(
			"wrong/types/duplicated_variant_alternative.dbc",
			{
				vm::code::DuplicatedVariantAlternativeError::ERR_MSG,
			}
		);
	}

	void wrongDowncast() {
		loadInvalidDbc(
			"wrong/pointers/wrong_downcast.dbc",
			{
				vm::code::InvalidDowncastError::ERR_MSG,
			}
		);
	}

	void wrongPtrMov() {
		loadInvalidDbc(
			"wrong/types/wrong_ptr_mov.dbc",
			{
				vm::code::PointerTypeMismatchError::ERR_MSG,
			}
		);
	}

	void incorrectUsesIO() {
		auto ec  = vm::api::ExecutionConfig{};
		ec.no_io = true;
		loadInvalidDbc(
			"wrong/config/io.dbc",
			{
				vm::code::ExecutionConfigViolationError::ERR_MSG,
				"no_io",
			},
			ec
		);
	}

	void incorrectWritesToGlobal() {
		auto ec      = vm::api::ExecutionConfig{};
		ec.read_only = true;
		loadInvalidDbc(
			"wrong/config/read_only.dbc",
			{
				vm::code::ExecutionConfigViolationError::ERR_MSG,
				"read_only",
			},
			ec
		);
	}

	// A calls B, where B performs IO. Under no_io the effect must propagate along the call
	// graph so the program is rejected.
	void callGraphPropagatesIO() {
		auto ec  = vm::api::ExecutionConfig{};
		ec.no_io = true;
		loadInvalidDbc(
			"wrong/config/call_io.dbc",
			{
				vm::code::ExecutionConfigViolationError::ERR_MSG,
				"no_io",
			},
			ec
		);
	}

	// B (does IO) is loaded first as an "old" function; a later batch whose main calls B must
	// inherit the IO effect and be rejected under no_io.
	void crossBatchPropagatesIO() {
		auto ec  = vm::api::ExecutionConfig{};
		ec.no_io = true;
		loadThenLoadInvalidDbc(
			"wrong/config/io_lib.dbc",
			"wrong/config/call_old_io.dbc",
			{
				vm::code::ExecutionConfigViolationError::ERR_MSG,
				"no_io",
			},
			ec
		);
	}

	// Like above, but the old IO function is reached only through a tail call — the effect must
	// still propagate across the tail-call edge.
	void tailCallPropagatesIO() {
		auto ec  = vm::api::ExecutionConfig{};
		ec.no_io = true;
		loadThenLoadInvalidDbc(
			"wrong/config/io_lib.dbc",
			"wrong/config/tailcall_old_io.dbc",
			{
				vm::code::ExecutionConfigViolationError::ERR_MSG,
				"no_io",
			},
			ec
		);
	}

	void spawningThreadViolatesSingleThread() {
		auto ec          = vm::api::ExecutionConfig{};
		ec.single_thread = true;
		loadInvalidDbc(
			"wrong/config/spawn_thread.dbc",
			{
				vm::code::ExecutionConfigViolationError::ERR_MSG,
				"single_thread",
			},
			ec
		);
	}

	// A pure program must not trigger a false positive under any config.
	void validProgramPassesConfig() {
		auto no_io           = vm::api::ExecutionConfig{};
		no_io.no_io          = true;
		auto read_only       = vm::api::ExecutionConfig{};
		read_only.read_only  = true;
		auto single          = vm::api::ExecutionConfig{};
		single.single_thread = true;

		loadValidDbc("right/config/pure.dbc", no_io);
		loadValidDbc("right/config/pure.dbc", read_only);
		loadValidDbc("right/config/pure.dbc", single);
	}

	void wrongGlobalInitializationMethod() {
		loadInvalidDbc(
			"wrong/globals/invalid_init.dbc",
			{
				vm::code::GlobalCtorAndInitialValueConflictError::ERR_MSG,
			}
		);
	}

	void wrongGlobalImmSize() {
		loadInvalidDbc(
			"wrong/globals/wrong_imm_size.dbc",
			{
				vm::code::InitialValueTypeMismatchError::ERR_MSG,
			}
		);
	}

	void wrongGlobalField() {
		loadInvalidDbc(
			"wrong/globals/wrong_field.dbc",
			{
				vm::code::InitialValueTypeMismatchError::ERR_MSG,
			}
		);
	}

	void wrongGlobalTblSize() {
		loadInvalidDbc(
			"wrong/globals/wrong_table_size.dbc",
			{
				vm::code::InitialValueTypeMismatchError::ERR_MSG,
			}
		);
	}

	void globalNoConstructor() {
		loadInvalidDbc(
			"wrong/globals/global_no_constructor.dbc",
			{ vm::code::MissingGlobalCtorDtorError::ERR_MSG }
		);
	}

	void globalNoDestructor() {
		loadInvalidDbc(
			"wrong/globals/global_no_destructor.dbc",
			{ vm::code::MissingGlobalCtorDtorError::ERR_MSG }
		);
	}

	void invalidGlobalConstructorSignature() {
		loadInvalidDbc(
			"wrong/globals/global_invalid_constructor_signature.dbc",
			{ vm::code::InvalidConstructorDestructorSignature::ERR_MSG }
		);
	}

	void deinitMainRetVal() {
		loadInvalidDbc(
			"wrong/functions/deinit_main_ret_val.dbc", { vm::code::RetValDeinitError::ERR_MSG }
		);
	}

	void invalidBuiltinFunction() {
		loadInvalidDbc(
			"wrong/functions/invalid_builtin_function.dbc",
			{ vm::code::InvalidBuiltinFunctionError::ERR_MSG }
		);
	}

	void pathWithoutEnd() {
		for (auto filename: { "wrong/functions/no_ret.dbc", "wrong/functions/empty_function.dbc" })
			loadInvalidDbc(filename, { vm::code::PathWithoutEndError::ERR_MSG });

		// A path that never returns because it loops forever or panics is fine.
		for (auto filename: { "right/infinite_loop.dbc", "right/dead_end.dbc" })
			loadValidDbc(filename);
	}

	void invalidSignatures() {
		loadInvalidDbc(
			"wrong/functions/invalid_ret_type.dbc", { vm::code::UnknownTypeError::ERR_MSG }
		);
		loadInvalidDbc(
			"wrong/functions/invalid_param_type.dbc", { vm::code::UnknownTypeError::ERR_MSG }
		);
		loadInvalidDbc(
			"wrong/functions/casting_invalid_ret.dbc", { vm::code::InvalidRetError::ERR_MSG }
		);
	}

	void sizeZeroPrimitive() {
		loadInvalidDbc(
			"wrong/types/size_zero_primitive.dbc", { vm::code::InvalidPrimitiveSizeError::ERR_MSG }
		);
	}

	void emptyVariant() {
		loadInvalidDbc(
			"wrong/types/empty_variant.dbc", { vm::code::TooFewVariantAlternativesError::ERR_MSG }
		);
	}

	void nonInstantiableVariant() {
		loadInvalidDbc(
			"wrong/types/non_instantiable_variant.dbc",
			{ vm::code::UninstantiableValueError::ERR_MSG }
		);
	}

	void nonInstantiableDynamicTable() {
		loadInvalidDbc(
			"wrong/types/non_instantiable_dynamic_table.dbc",
			{ vm::code::UninstantiableValueError::ERR_MSG }
		);
	}

	void invalidLiterals() {
		loadInvalidDbc(
			"wrong/literals/invalid_literal.dbc",
			{
				"Numeric literal overflows a 32-bit signed integer",
				"Numeric literal underflows a 32-bit signed integer",
				"Numeric literal overflows a 32-bit unsigned integer",
				"Numeric literal overflows a 64-bit signed integer",
				"Floating-point literals must be in decimal base for",
			}
		);
	}

	// Label verification
	void multipleLabels() {
		loadInvalidDbc(
			"wrong/labels/multiple_labels.dbc",
			{
				vm::code::DuplicatedLabelError::ERR_MSG,
			}
		);
	}

	void labelNotFound() {
		loadInvalidDbc(
			"wrong/labels/label_not_found.dbc",
			{
				vm::code::UnknownLabelError::ERR_MSG,
				"LB",
			}
		);
	}

	// Name resolution
	void unknownType() {
		loadInvalidDbc(
			"wrong/types/unknown_type.dbc",
			{
				base::strConcat(vm::code::UnknownTypeError::ERR_MSG, "in64"),
			}
		);
	}

	void unknownFunction() {
		loadInvalidDbc(
			"wrong/functions/unknown_function.dbc",
			{
				base::strConcat(vm::code::UnknownFunctionError::ERR_MSG, "foo"),
			}
		);
	}

	void invalidFunctionCall() {
		loadInvalidDbc(
			"wrong/functions/invalid_function_call.dbc",
			{
				vm::code::InvalidFunctionCallArgumentsError::ERR_MSG,
			}
		);
	}

	void invalidRetTypeInCall() {
		loadInvalidDbc(
			"wrong/functions/invalid_ret_type_in_call.dbc",
			{
				vm::code::InvalidFunctionCallArgumentsError::ERR_MSG,
			}
		);
	}

	// Type size assertions

	// `packed` restores the no-padding layout: 1 + 8 bytes.
	void packedStructSize() {
		loadValidDbc(inlineDbc(
			"packed_size",
			"type data: Mix { a: i8, b: i64 } packed assert_size 9\n"
			"function main { i64, ptr_argv } -> { i64 } { ret; }\n"
		));
	}

	void assertSizeMatches() {
		loadValidDbc(inlineDbc(
			"assert_ok",
			"type data: Pair { a: i32, b: i32 } assert_size 8\n"
			"function main { i64, ptr_argv } -> { i64 } { ret; }\n"
		));
	}

	void assertSizeMismatchFails() {
		loadInvalidDbc(
			inlineDbc("assert_bad", "type data: Pair { a: i32, b: i32 } assert_size 16\n"),
			{ vm::code::TypeSizeAssertError::ERR_MSG }
		);
	}

	// A type whose size depends on the pointer width (8 bytes in C, 16 in the safe interpreter)
	// has no single size to assert against.
	void assertSizeOnPointerTypeFails() {
		loadInvalidDbc(
			inlineDbc("assert_ptr", "type data: Holder { p: ptr_i64 } assert_size 8\n"),
			{ vm::code::TypeSizeAssertPointerDependentError::ERR_MSG }
		);
	}

	// C pointer verification

	void cpointerToUnknownTypeFails() {
		loadInvalidDbc(
			inlineDbc("cpointer_unknown_inner", "type cpointer: BadPtr NoSuchType\n"),
			{ vm::code::UnknownSubtypeError::ERR_MSG, "NoSuchType" }
		);
	}

	// `mov_pcptr_pcptr` requires identical types on both sides: no implicit pointee change, no
	// mixing the builtin `cptr` (unknown pointee) with a typed cpointer, distinct names stay
	// distinct even with the same pointee, and non-cpointer operands are rejected outright.
	void movPCptrStrictnessFails() {
		auto expect_mov_error = [&](const std::string& name,
		                            const std::string& types,
		                            const std::string& a_type,
		                            const std::string& b_type,
		                            std::string_view   keyword) {
			loadInvalidDbc(
				inlineDbc(
					name,
					types + "function main { i64, ptr_argv } -> { i64 } {\n"
						+ "    init_pany_type a, " + a_type + ";\n" + "    init_pany_type b, "
						+ b_type + ";\n"
						+ "    mov_pcptr_pcptr a, b;\n"
						  "    ret;\n"
						  "}\n"
				),
				{ keyword }
			);
		};
		expect_mov_error(
			"mov_pcptr_across_types",
			"type cpointer: APtr i64\ntype cpointer: BPtr i32\n",
			"APtr",
			"BPtr",
			vm::code::CPointerTypeMismatchError::ERR_MSG
		);
		expect_mov_error(
			"mov_pcptr_cptr_to_typed",
			"type cpointer: APtr i64\n",
			"APtr",
			"cptr",
			vm::code::CPointerTypeMismatchError::ERR_MSG
		);
		expect_mov_error(
			"mov_pcptr_same_pointee",
			"type cpointer: APtr i64\ntype cpointer: BPtr i64\n",
			"APtr",
			"BPtr",
			vm::code::CPointerTypeMismatchError::ERR_MSG
		);
		expect_mov_error(
			"mov_pcptr_non_cpointer", "", "i64", "i64", vm::code::InvalidArgumentTypeError::ERR_MSG
		);
	}

	// The builtin `cptr` has no pointee, so it cannot be dereferenced with a typed load.
	void cptrLoadThroughVoidCPtrFails() {
		loadInvalidDbc(
			inlineDbc(
				"load_through_void",
				"function main { i64, ptr_argv } -> { i64 } {\n"
				"    init_pany_type c, cptr;\n"
				"    init_pany_type v, i64;\n"
				"    load_pany_pcptr v, c;\n"
				"    ret;\n"
				"}\n"
			),
			{ vm::code::CPtrNotDereferenceableError::ERR_MSG }
		);
	}

	void cptrLoadPointeeMismatchFails() {
		loadInvalidDbc(
			inlineDbc(
				"load_pointee_mismatch",
				"type cpointer: I64Ptr i64\n"
				"function main { i64, ptr_argv } -> { i64 } {\n"
				"    init_pany_type p, I64Ptr;\n"
				"    init_pany_type v, i32;\n"
				"    load_pany_pcptr v, p;\n"
				"    ret;\n"
				"}\n"
			),
			{ vm::code::CPtrPointeeMismatchError::ERR_MSG }
		);
	}

	// A packed pointee is not FFI-compliant, so its native layout cannot be assumed.
	void cptrLoadPackedPointeeFails() {
		loadInvalidDbc(
			inlineDbc(
				"load_packed_pointee",
				"type data: P { a: i8, b: i64 } packed\n"
				"type cpointer: PPtr P\n"
				"function main { i64, ptr_argv } -> { i64 } {\n"
				"    init_pany_type p, PPtr;\n"
				"    init_pany_type s, P;\n"
				"    load_pany_pcptr s, p;\n"
				"    ret;\n"
				"}\n"
			),
			{ vm::code::CPtrNotDereferenceableError::ERR_MSG }
		);
	}

	// The store-side mirror of cptrLoadThroughVoidCPtrFails.
	void cptrStoreThroughVoidCPtrFails() {
		loadInvalidDbc(
			inlineDbc(
				"store_through_void",
				"function main { i64, ptr_argv } -> { i64 } {\n"
				"    init_pany_type c, cptr;\n"
				"    init_pany_type v, i64;\n"
				"    store_pcptr_pany c, v;\n"
				"    ret;\n"
				"}\n"
			),
			{ vm::code::CPtrNotDereferenceableError::ERR_MSG }
		);
	}

	// The store-side mirror of cptrLoadPointeeMismatchFails.
	void cptrStorePointeeMismatchFails() {
		loadInvalidDbc(
			inlineDbc(
				"store_pointee_mismatch",
				"type cpointer: I64Ptr i64\n"
				"function main { i64, ptr_argv } -> { i64 } {\n"
				"    init_pany_type p, I64Ptr;\n"
				"    init_pany_type v, i32;\n"
				"    store_pcptr_pany p, v;\n"
				"    ret;\n"
				"}\n"
			),
			{ vm::code::CPtrPointeeMismatchError::ERR_MSG }
		);
	}

	// The store-side mirror of cptrLoadPackedPointeeFails.
	void cptrStorePackedPointeeFails() {
		loadInvalidDbc(
			inlineDbc(
				"store_packed_pointee",
				"type data: P { a: i8, b: i64 } packed\n"
				"type cpointer: PPtr P\n"
				"function main { i64, ptr_argv } -> { i64 } {\n"
				"    init_pany_type p, PPtr;\n"
				"    init_pany_type s, P;\n"
				"    store_pcptr_pany p, s;\n"
				"    ret;\n"
				"}\n"
			),
			{ vm::code::CPtrNotDereferenceableError::ERR_MSG }
		);
	}

	// The builtin `cptr` has no pointee, so the byte count of a raw copy is unknown.
	void cptrReadThroughVoidCPtrFails() {
		loadInvalidDbc(
			inlineDbc(
				"read_through_void",
				"function main { i64, ptr_argv } -> { i64 } {\n"
				"    init_pany_type c, cptr;\n"
				"    init_pany_type v, i64;\n"
				"    init_pany_type vp, ptr_i64;\n"
				"    ref_pptr_pany vp, v;\n"
				"    read_pptr_pcptr vp, c;\n"
				"    ret;\n"
				"}\n"
			),
			{ vm::code::CPtrNotDereferenceableError::ERR_MSG }
		);
	}

	// Both sides of a raw copy must agree on the pointee type.
	void cptrReadPointeeMismatchFails() {
		loadInvalidDbc(
			inlineDbc(
				"read_pointee_mismatch",
				"type cpointer: I64Ptr i64\n"
				"function main { i64, ptr_argv } -> { i64 } {\n"
				"    init_pany_type p, I64Ptr;\n"
				"    init_pany_type v, i32;\n"
				"    init_pany_type vp, ptr_i32;\n"
				"    ref_pptr_pany vp, v;\n"
				"    read_pptr_pcptr vp, p;\n"
				"    ret;\n"
				"}\n"
			),
			{ vm::code::CPtrPointeeMismatchError::ERR_MSG }
		);
	}

	// The write-side mirror of cptrReadThroughVoidCPtrFails.
	void cptrWriteThroughVoidCPtrFails() {
		loadInvalidDbc(
			inlineDbc(
				"write_through_void",
				"function main { i64, ptr_argv } -> { i64 } {\n"
				"    init_pany_type c, cptr;\n"
				"    init_pany_type v, i64;\n"
				"    init_pany_type vp, ptr_i64;\n"
				"    ref_pptr_pany vp, v;\n"
				"    write_pcptr_pptr c, vp;\n"
				"    ret;\n"
				"}\n"
			),
			{ vm::code::CPtrNotDereferenceableError::ERR_MSG }
		);
	}

	// The write-side mirror of cptrReadPointeeMismatchFails.
	void cptrWritePointeeMismatchFails() {
		loadInvalidDbc(
			inlineDbc(
				"write_pointee_mismatch",
				"type cpointer: I64Ptr i64\n"
				"function main { i64, ptr_argv } -> { i64 } {\n"
				"    init_pany_type p, I64Ptr;\n"
				"    init_pany_type v, i32;\n"
				"    init_pany_type vp, ptr_i32;\n"
				"    ref_pptr_pany vp, v;\n"
				"    write_pcptr_pptr p, vp;\n"
				"    ret;\n"
				"}\n"
			),
			{ vm::code::CPtrPointeeMismatchError::ERR_MSG }
		);
	}

	// A raw copy must not overwrite VM-managed data: a struct holding a VM pointer is not
	// FFI-compliant, so a cpointer to it is not dereferenceable.
	void cptrReadIntoPointerPointeeFails() {
		loadInvalidDbc(
			inlineDbc(
				"read_into_pointer_pointee",
				"type data: Holder { p: ptr_i64, v: i64 }\n"
				"type pointer: HolderPtr Holder\n"
				"type cpointer: HolderCPtr Holder\n"
				"function main { i64, ptr_argv } -> { i64 } {\n"
				"    init_pany_type c, HolderCPtr;\n"
				"    init_pany_type hp, HolderPtr;\n"
				"    read_pptr_pcptr hp, c;\n"
				"    ret;\n"
				"}\n"
			),
			{ vm::code::CPtrNotDereferenceableError::ERR_MSG }
		);
	}

	// A raw copy must not leak raw block addresses to native memory either.
	void cptrWriteFromPointerPointeeFails() {
		loadInvalidDbc(
			inlineDbc(
				"write_from_pointer_pointee",
				"type data: Holder { p: ptr_i64, v: i64 }\n"
				"type pointer: HolderPtr Holder\n"
				"type cpointer: HolderCPtr Holder\n"
				"function main { i64, ptr_argv } -> { i64 } {\n"
				"    init_pany_type c, HolderCPtr;\n"
				"    init_pany_type hp, HolderPtr;\n"
				"    write_pcptr_pptr c, hp;\n"
				"    ret;\n"
				"}\n"
			),
			{ vm::code::CPtrNotDereferenceableError::ERR_MSG }
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/loader/verification/");
