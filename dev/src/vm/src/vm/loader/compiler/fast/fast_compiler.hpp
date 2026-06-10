#pragma once

#include <vm/core/fast/program/instructions/relocatable.hpp>
#include <vm/core/fast/program/program.hpp>
#include <vm/core/fast/program/type.hpp>
#include <vm/loader/compiler/ivm_compiler.hpp>

namespace vm::loader::compiler::fast {
	class FastCompiler final: public vm::loader::compiler::IVMCompiler {
	public:
		FastCompiler(const code::ValidProgram& high_program);

		[[nodiscard]] CRef<vm::fast::ProgramBase> getProgramBase() const;
		[[nodiscard]] CRef<vm::fast::reloc::RelocFunctionCollection> getRelocatableFunctions() const;

	protected:
		void compileNewTypes(const std::vector<code::valid_type::ValidType>& new_types) override;

		void compileNewGlobals(const std::vector<code::GlobalData>& new_globals) override;

		void compileNewFunctions(const std::vector<code::valid_function::ValidFunction>& new_functions
		) override;

		void compileNewExtCFunctions(const std::vector<code::ExternalCFunction>& new_functions
		) override;

		[[nodiscard]] ProgramSize getCurrentProgramSize() const override;

	private:
		vm::fast::ProgramBase                    program;
		vm::fast::reloc::RelocFunctionCollection reloc_functions;
	};
}
