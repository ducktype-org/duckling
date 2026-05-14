#include "compiler.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/int_conv.hpp>
#include <base/preproc/for_each.hpp>
#include <base/types/ints.hpp>

#include <string_id/string_id.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/instructions.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/serializer/serializer.hpp>
#include <vm/bytecode/validator/valid_type/finalized_kinds.hpp>
#include <vm/bytecode/validator/valid_type/type_size.hpp>
#include <vm/bytecode/validator/valid_type/valid_type.hpp>
#include <vm/bytecode/validator/valid_type/valid_type_id.hpp>
#include <vm/core/builtin_functions.hpp>

#include <ranges>

namespace vm::loader::compiler {

	 detail::FunctionStackContext Compiler::calculateStackContext(const code::Function& function) {
		detail::FunctionStackContext ctx(function);
		decltype(ctx.locals_map)                result;
		std::vector<code::valid_type::TypeSize> type_size_stack;
		code::valid_type::TypeSize              curr_stack_size{};
		code::valid_type::TypeSize              max_stack_size{};
		usize                                   max_block_count = 0;

		auto push = [&](opargs::PlaceAny local, opargs::Type type) {
			if_opt_some(result.atMaybe(local.var_name), entry) {
				if (entry->offset != curr_stack_size) {
					CORE_PANIC(
						"DuplicatedLocalNameError - used a variable again at a different offset "
						"which wasn't detected by the function validator"
					);
				}
			}

			auto type_ref = high_program.getTypeContext().getCurrentTypes().at(type.type_name);
			result.put(
				local.var_name,
				{ .offset    = curr_stack_size,
			      .stack_index = type_size_stack.size(),
			      .type      = type_ref->getID() }
			);
			code::valid_type::TypeSize type_size = type_ref->getSize();
			type_size_stack.push_back(type_size);
			curr_stack_size += type_size;
			max_stack_size  = max_stack_size.fieldMax(curr_stack_size);
			max_block_count = std::max(max_block_count, type_size_stack.size());
		};

		auto pop = [&]() {
			auto type_size = type_size_stack.back();
			type_size_stack.pop_back();
			curr_stack_size -= type_size;
		};

		auto seek_method_param_count = [&](const base::StrID& method_name) -> u64 {
			// @todo: https://github.com/ducktype-org/duckling/issues/962
			for (const code::valid_type::ValidType& type:
			     high_program.getTypeContext().getCurrentTypes()) {
				if_opt_some(type.maybeGetKindAs<code::valid_type::finalized::Structure>(), strukt) {
					if_opt_some(strukt->inheritance_metadata, inh_meta) {
						if_opt_some(inh_meta.available_methods.atMaybe(method_name), method_id) {
							CRef<code::valid_type::finalized::Function> method_type
								= high_program.getTypeContext()
							          .getCurrentTypes()
							          .at(*method_id)
							          ->getKindAs<code::valid_type::finalized::Function>();
							return method_type->parameters.size();
						}
					}
				}
			}
			CORE_UNREACHABLE();
		};

		// Label positions in high bytecode, used only for graph traversing
		// in this function. Not used when lowering to microbytecode.
		base::HashMap<base::StrID, usize> label_positions{};
		for (auto [idx, instr]: std::views::enumerate(ctx.function.body)) {
			instr_match(instr) {
				instr_case(code::instructions::Op_label, label) {
					label_positions.put(label.label.label_name, idx);
				}
				instr_default {}
			}
		}

		code::FuncSignature func_signature = ctx.function.signature;
		using namespace std::views;
		for (auto [idx, ret_type]: enumerate(func_signature.result_types))
			push(base::StrID(base::strConcat("ret", idx).c_str()), ret_type.str);
		for (auto [idx, param_type]: enumerate(func_signature.parameters))
			push(base::StrID(base::strConcat("arg", idx).c_str()), param_type.str);
		// instruction index, stack state, stack size
		std::vector<std::tuple<usize, decltype(type_size_stack), code::valid_type::TypeSize>>
						  dfs_stack{
							  { ctx.function.body.size(), {}, {} }  // sentinel
						  };
		std::vector<bool> visited_instructions(ctx.function.body.size());
		usize             index = 0;

		while (index != ctx.function.body.size()) {
			if (visited_instructions[index]) {
				std::tie(index, type_size_stack, curr_stack_size) = dfs_stack.back();
				dfs_stack.pop_back();
				continue;
			}
			visited_instructions[index] = true;

			instr_match(ctx.function.body[index]) {
				using namespace code::instructions;
				instr_case(Op_init_pany_type, instr) {
					push(instr.var, instr.type);
					index++;
				}
				instr_case(Op_deinit, instr) {
					pop();
					index++;
				}
				instr_case(Op_jmp_label, instr) { index = label_positions[instr.label.label_name]; }
				instr_case(Op_jmpIf_label, instr) {
					index++;
					dfs_stack.emplace_back(
						label_positions[instr.label.label_name], type_size_stack, curr_stack_size
					);
				}
				instr_case(Op_jmpIfNot_label, instr) {
					index++;
					dfs_stack.emplace_back(
						label_positions[instr.label.label_name], type_size_stack, curr_stack_size
					);
				}
				instr_case(Op_ret, instr) {
					std::tie(index, type_size_stack, curr_stack_size) = dfs_stack.back();
					dfs_stack.pop_back();
				}
				instr_case(Op_call_func, instr) {
					usize number_of_params = high_program.functions()
					                             .at(instr.function.function_name)
					                             ->signature.parameters.size();
					for (usize i = 0; i < number_of_params; i++) pop();
					index++;
				}
				instr_case(Op_call_builtinfunc, instr) {
					for (usize i = 0;
					     i < builtins::getBuiltinFunctionSignature(instr.function.function_name)
					             .value()
					             ->parameters.size();
					     i++) {
						pop();
					}
					index++;
				}
				instr_case(Op_call_cfunc, instr) {
					for (usize i = 0; i < high_program.extCFunctions()
					                          .at(instr.function.function_name)
					                          ->signature.parameters.size();
					     i++) {
						pop();
					}
					index++;
				}
				instr_case(Op_virtual_call_pptr_method, instr) {
					for (usize i = 0; i < seek_method_param_count(instr.method.method_name); i++)
						pop();
					index++;
				}
				instr_case(Op_ret_tailcall_func, instr) {
					std::tie(index, type_size_stack, curr_stack_size) = dfs_stack.back();
					dfs_stack.pop_back();
				}
				instr_default { index++; }
			}
		}

		ctx.locals_map        = std::move(result);
		ctx.local_stack_size  = max_stack_size;
		ctx.local_block_count = max_block_count;
		return ctx;
	}

	void Compiler::recompile() {
		ProgramSize sizes = getCurrentProgramSize();

		auto new_types = high_program.getTypeContext().getCurrentTypes()
		               | std::views::drop(sizes.type_count)
		               | std::ranges::to<std::vector<code::valid_type::ValidType>>();
		compileNewTypes(new_types);

		auto new_c_functions = high_program.extCFunctions()
		                     | std::views::drop(sizes.ext_c_function_count)
		                     | std::ranges::to<std::vector<code::ExternalCFunction>>();
		compileNewExtCFunctions(new_c_functions);

		auto new_globals = high_program.globals() | std::views::drop(sizes.global_count)
		                 | std::ranges::to<std::vector<code::GlobalData>>();

		compileNewGlobals(new_globals);

		auto new_functions = high_program.functions() | std::views::drop(sizes.function_count)
		                   | std::ranges::to<std::vector<code::Function>>();
		compileNewFunctions(new_functions);
	}

}
