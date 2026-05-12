#pragma once

#include <vm/loader/compiler/compiler.hpp>

namespace vm::loader::compiler::fast {
	class FastCompiler final: public vm::loader::compiler::Compiler {
	public:
		FastCompiler(const code::ValidProgram& high_program);

	protected:
		void compileNewTypes(const std::vector<code::valid_type::ValidType>& new_types) override;

		void compileNewGlobals(const std::vector<code::GlobalData>& new_globals) override;

		void compileNewFunctions(const std::vector<code::Function>& new_functions) override;

		void compileNewExtCFunctions(
			const std::vector<code::ExternalCFunction>& new_functions
		) override;

		ProgramSize getCurrentProgramSize() const override;

	private:
	};
}
