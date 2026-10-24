// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <backends/llvm/llvm_backend.hpp>
#include <driver/incremental_utils/collect_input.hpp>
#include <driver/test_utils.hpp>
#include <frontend/module_tree/module_flags/module_flags.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/module_tree_modifier.hpp>
#include <frontend/module_tree/source_file.hpp>
#include <frontend/pst_parser/source_position_locked.hpp>
#include <global_state/backend_options.hpp>
#include <helios/queries/queries.hpp>
#include <lir/lir_lowering/lir_unit.hpp>
#include <mir/mir_lowering/mir_unit.hpp>

#include <filesystem/file.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/external/api.hpp>
#include <query_framework/module_flags/module_flags.hpp>
#include <tester/tester.hpp>

#include <string>
#include <vector>

/**
 * Recompiles a module in the same process after its source changed, the way a long-lived
 * session (language server, benchmark driver) does.
 */
class LLVMIncrementalTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LLVMIncrementalTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(classFieldAddedTest); }

protected:
	void beforeAll() override {
		query::setTrackReverseGraph(true);
		compiler::frontend::use_module_modifier_remove = true;

		fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		auto         init_result    = compiler::driver::test_utils::initializeCompilerForTests(
            {},
            artifacts_path,
            { .std_lib_type = compiler::driver::options_types::StdLibOptions::DefaultStd{} },
            { .llvm_backend = global_state::BackendOptions::LLVMBackend{} }
        );
		assertTrue(init_result.status().isOk(), "Compiler initialization failed");
	}

private:
	using Inputs = std::vector<query::external::InputData>;

	static void collectInputs(const fs::File& file, Inputs& out) {
		out.push_back(pst::SourcePositionLocked::getQueryInputNode());
		for (auto& source_file:
		     compiler::frontend::SourceFile::getSourceFilesFromPath(file.getFilePath()))
			compiler::driver::collectQueryInputsFromPst(source_file->getPST(), out);
	}

	void lowerAndVerify(compiler::frontend::ModuleID module_id) {
		using namespace compiler;
		query::utils::withContextDo([&](query::Context& ctx) {
			auto& module_hout = ctx.query<helios::QueryModuleHOUT>(module_id)->valueOrPanic();
			auto  mir_result  = mir::lowerToMIRUnit(ctx, &module_hout).valueOrPanic();
			auto  lir_result  = lir::lowerToLIRUnit(ctx, mir_result);
			auto  llvm_module
				= backend_llvm::Module::fromLIRUnit(ctx, lir_result, base::StrID("test_module"));
			assertTrue(llvm_module.verify().isOk(), "LLVM module verification failed");
		});
	}

	/**
	 * @brief A class that gains a field keeps its name, so its LLVM struct must not be reused
	 * from the previous compilation.
	 */
	void classFieldAddedTest() {
		const std::filesystem::path dir
			= fs::FileManager::createRandomVirtualDirectory().getFilePath().getPath() / "acc";
		fs::FileManager::createVirtualFolder(fs::FilePath(dir));
		const fs::FilePath file_path(dir / "acc.dk");

		const std::string body
			= "\tsum: i64;\n"
			  "\tpeak: i64;\n"
			  "}\n"
			  "fun f(x: i64) -> i64 = {\n"
			  "\tvar q: Acc;\n"
			  "\tq.sum = x;\n"
			  "\tq.peak = 2i64;\n"
			  "\treturn q.sum + q.peak;\n"
			  "}\n";
		const fs::File file
			= fs::FileManager::createVirtualFile(file_path, "class Acc {\n" + body, true);

		const auto module_id
			= compiler::frontend::createModuleTreeWithRandomPackageID(fs::File(fs::FilePath(dir)));
		lowerAndVerify(module_id);

		Inputs previous_inputs;
		collectInputs(file, previous_inputs);
		fs::FileManager::createVirtualFile(file_path, "class Acc {\n\textra: i64;\n" + body, true);
		compiler::frontend::ModuleTreeModifier::fileModified(file);
		Inputs new_inputs;
		collectInputs(file, new_inputs);
		query::external::invalidateQueries(std::move(new_inputs), { previous_inputs }, {});

		lowerAndVerify(module_id);
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/backends/llvm/tests/")
