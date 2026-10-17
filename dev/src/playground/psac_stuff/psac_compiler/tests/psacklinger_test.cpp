#include <compiler_test_corpus/compiler_test_corpus.hpp>
#include <pq_shared/compiler.hpp>

#include <filesystem/file.hpp>
#include <system_command/system_command.hpp>
#include <tester/tester.hpp>

class PsacklingerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PsacklingerTest

	usize single_file_id = 0;
	usize multi_file_id  = 0;

	void singleFileCorpusTest() {
		const psack_corpus::SingleFileCase& data
			= psack_corpus::singleFileCases()[single_file_id++];
		std::cout << "Running test: " << data.name << '\n';
		fs::File f                 = fs::FileManager::createRandomVirtualFile(data.code);
		fs::File random_output_dir = fs::FileManager::createRandomTempDirectory();
		fs::File executable        = random_output_dir.createSubFile("", "psacklinged.out");
		pg::compile(f, executable.getFilePath().string());
		ASSERT_EQUAL_PRINT(data.expected_exit_code, runExecutable(executable));
	}

	void multiFileCorpusTest() {
		const psack_corpus::MultiFileCase& data = psack_corpus::multiFileCases()[multi_file_id++];
		std::cout << "Running test: " << data.name << '\n';
		fs::File root = psack_corpus::makeProgramDir(data.files);

		if (data.error_substring == nullptr) {
			ASSERT_EQUAL_PRINT(data.expected_exit_code, compileAndRun(root));
			return;
		}
		try {
			compileAndRun(root);
			ASSERT_TRUE(false);
		} catch (const std::runtime_error& e) {
			ASSERT_TRUE(std::string(e.what()).contains(data.error_substring));
		}
	}

	static u64 runExecutable(const fs::File& executable) {
		system_command::SystemCommand cmd(executable.getFilePath().string());
		return u64(cmd.execute(system_command::SystemCommand::ExitCodeHandling::Ignore));
	}

	static u64 compileAndRun(const fs::File& root) {
		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "multifile.out");
		pg::compile(root, executable.getFilePath().string());
		return runExecutable(executable);
	}

	/// Changing one function body in an imported file recompiles exactly that function.
	void incrementalImportedBodyChange() {
		const std::string root_code
			= "import math as M;\n"
			  "fun main() -> i64 = { return M.helper() + 1; }\n";

		fs::File           root = psack_corpus::makeProgramDir({
            { .relative_path = "root.dmf", .code = root_code.c_str() },
            { .relative_path = "math.dmf", .code = "fun helper() -> i64 = { return 10; }\n" },
        });
		const fs::FilePath math_path{ root.getFilePath().parentPath().getPath() / "math.dmf" };

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_import.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		pg::CompileStats initial = compiler.compile(root);
		ASSERT_EQUAL_PRINT(usize(2), initial.total_functions);
		ASSERT_EQUAL_PRINT(u64(11), runExecutable(executable));

		pg::CompileStats unchanged = compiler.update(root);
		ASSERT_EQUAL_PRINT(usize(0), unchanged.recompiled_functions);

		fs::FileManager::createVirtualFile(
			math_path, "fun helper() -> i64 = { return 20; }\n", /*allow_overwrite=*/true
		);
		pg::CompileStats incremental = compiler.update(root);
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(21), runExecutable(executable));
	}

	/// Changing a const in an imported file should recompile its reader alone, leaving
	/// the non-reader `other` cached.
	void incrementalImportedConstChange() {
		fs::File           root = psack_corpus::makeProgramDir({
            { .relative_path = "root.dmf",
		                .code          = "import conf.*;\n"
		                                           "fun main() -> i64 = { return X + 1; }\n" },
            { .relative_path = "conf.dmf", .code = "const X: i64 = 5;\n" },
        });
		const fs::FilePath conf_path{ root.getFilePath().parentPath().getPath() / "conf.dmf" };

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_import_const.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(root);
		ASSERT_EQUAL_PRINT(u64(6), runExecutable(executable));

		fs::FileManager::createVirtualFile(
			conf_path, "const X: i64 = 8;\n", /*allow_overwrite=*/true
		);
		pg::CompileStats incremental = compiler.update(root);
		ASSERT_EQUAL_PRINT(usize(2), incremental.total_functions);
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(9), runExecutable(executable));
	}

	/// Changing one function body recompiles exactly that function; identical source
	/// recompiles nothing.
	void incrementalBodyChange() {
		const std::string v1 = R"(
			fun helper() -> i64 = { return 10; }
			fun main() -> i64 = { return helper() + 1; }
		)";
		const std::string v2 = R"(
			fun helper() -> i64 = { return 20; }
			fun main() -> i64 = { return helper() + 1; }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		pg::CompileStats initial = compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(usize(2), initial.total_functions);
		ASSERT_EQUAL_PRINT(usize(2), initial.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(11), runExecutable(executable));

		pg::CompileStats unchanged = compiler.update(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(usize(0), unchanged.recompiled_functions);

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(21), runExecutable(executable));
	}

	/// Changing a global constant should recompile exactly its two readers, leaving the
	/// unrelated `other` cached.
	void incrementalGlobalsChange() {
		const std::string v1 = R"(
			const X: i64 = 5;
			fun helper() -> i64 = { return X; }
			fun other() -> i64 = { return 3; }
			fun main() -> i64 = { return helper() + X; }
		)";
		const std::string v2 = R"(
			const X: i64 = 7;
			fun helper() -> i64 = { return X; }
			fun other() -> i64 = { return 3; }
			fun main() -> i64 = { return helper() + X; }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_globals.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(3), incremental.total_functions);
		ASSERT_EQUAL_PRINT(usize(2), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(14), runExecutable(executable));
	}

	/// Changing a global variable initializer re-emits the globals object and relinks,
	/// but should recompile no functions: readers depend only on the variable's
	/// identity and type.
	void incrementalGlobalVarChange() {
		const std::string v1 = R"(
			var g: i64 = 5;
			fun main() -> i64 = { return g; }
		)";
		const std::string v2 = R"(
			var g: i64 = 9;
			fun main() -> i64 = { return g; }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_gvar.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(5), runExecutable(executable));

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(0), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(9), runExecutable(executable));
	}

	/// Changing a constant should recompile exactly its readers.
	void incrementalConstOnlyReadersRecompile() {
		const std::string v1 = R"(
			const X: i64 = 5;
			fun reader() -> i64 = { return X; }
			fun other() -> i64 = { return 7; }
			fun main() -> i64 = { return reader() + other(); }
		)";
		const std::string v2 = R"(
			const X: i64 = 8;
			fun reader() -> i64 = { return X; }
			fun other() -> i64 = { return 7; }
			fun main() -> i64 = { return reader() + other(); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_const_readers.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(12), runExecutable(executable));

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(3), incremental.total_functions);
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(15), runExecutable(executable));
	}

	/// A const used only as an array size is a dependency like any other const
	/// read: changing it recompiles exactly the function declaring the array.
	void incrementalArraySizeConstChange() {
		const std::string v1 = R"(
			const N: i64 = 4;
			fun user() -> i64 = {
				var a: i64[N];
				a[N - 1] = 9;
				return a[N - 1] + N;
			}
			fun other() -> i64 = { return 7; }
			fun main() -> i64 = { return user() + other(); }
		)";
		const std::string v2 = R"(
			const N: i64 = 6;
			fun user() -> i64 = {
				var a: i64[N];
				a[N - 1] = 9;
				return a[N - 1] + N;
			}
			fun other() -> i64 = { return 7; }
			fun main() -> i64 = { return user() + other(); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_array_size.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(20), runExecutable(executable));

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(3), incremental.total_functions);
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(22), runExecutable(executable));
	}

	/// Renaming a parameter changes only that function's own text: its signature
	/// (types only) is untouched, so callers should stay cached.
	void incrementalParamRenameOnlySelf() {
		const std::string v1 = R"(
			fun f(a: i64) -> i64 = { return a; }
			fun g() -> i64 = { return f(4); }
			fun main() -> i64 = { return g(); }
		)";
		const std::string v2 = R"(
			fun f(b: i64) -> i64 = { return b; }
			fun g() -> i64 = { return f(4); }
			fun main() -> i64 = { return g(); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_param_rename.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(4), runExecutable(executable));

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(3), incremental.total_functions);
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(4), runExecutable(executable));
	}

	/// Adding a class field should recompile only the functions that use the class.
	void incrementalClassFieldOnlyUsersRecompile() {
		const std::string v1 = R"(
			class Point {
				var x: i64;
			}
			fun reader() -> i64 = {
				var p: Point;
				p.x = 3;
				return p.x;
			}
			fun other() -> i64 = { return 7; }
			fun main() -> i64 = { return reader() + other(); }
		)";
		const std::string v2 = R"(
			class Point {
				var x: i64;
				var y: i64;
			}
			fun reader() -> i64 = {
				var p: Point;
				p.x = 3;
				return p.x;
			}
			fun other() -> i64 = { return 7; }
			fun main() -> i64 = { return reader() + other(); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_class_field.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(3), incremental.total_functions);
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));
	}

	/// Removing a function nothing depends on should relink without recompiling the
	/// unchanged survivors.
	void incrementalFunctionRemoved() {
		const std::string v1 = R"(
			fun extra() -> i64 = { return 41; }
			fun helper() -> i64 = { return 10; }
			fun main() -> i64 = { return helper(); }
		)";
		const std::string v2 = R"(
			fun helper() -> i64 = { return 10; }
			fun main() -> i64 = { return helper(); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_removed.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));

		pg::CompileStats shrunk = compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(2), shrunk.total_functions);
		ASSERT_EQUAL_PRINT(usize(0), shrunk.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));
	}

	/// Calling a function that does not exist fails; adding it should invalidate the
	/// caller's cached failure and nothing else — resolving a name depends on its
	/// absence too, and `other` never touches it.
	void incrementalMissingFunctionThenAdded() {
		const std::string v1 = R"(
			fun other() -> i64 = { return 1; }
			fun main() -> i64 = { return 5; }
		)";
		const std::string v2 = R"(
			fun other() -> i64 = { return 1; }
			fun main() -> i64 = { return gmissing(1); }
		)";
		const std::string v3 = R"(
			fun other() -> i64 = { return 1; }
			fun gmissing(p: i64) -> i64 = { return p + 5; }
			fun main() -> i64 = { return gmissing(1); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_missing_fn.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(5), runExecutable(executable));

		try {
			compiler.update(fs::FileManager::createRandomVirtualFile(v2));
			ASSERT_TRUE(false);
		} catch (const std::runtime_error& e) {
			ASSERT_TRUE(std::string(e.what()).contains("gmissing"));
		}

		pg::CompileStats added = compiler.update(fs::FileManager::createRandomVirtualFile(v3));
		ASSERT_EQUAL_PRINT(usize(3), added.total_functions);
		ASSERT_EQUAL_PRINT(usize(2), added.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(6), runExecutable(executable));
	}

	/// Adding a function the caller starts calling should recompile exactly those two:
	/// the new function and the changed caller, leaving `other` cached.
	void incrementalFunctionSetChange() {
		const std::string v1 = R"(
			fun other() -> i64 = { return 3; }
			fun main() -> i64 = { return 1; }
		)";
		const std::string v2 = R"(
			fun other() -> i64 = { return 3; }
			fun extra() -> i64 = { return 41; }
			fun main() -> i64 = { return extra() + 1; }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_set.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(1), runExecutable(executable));

		pg::CompileStats rebuilt = compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(3), rebuilt.total_functions);
		ASSERT_EQUAL_PRINT(usize(2), rebuilt.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(42), runExecutable(executable));
	}

	/// Removing a constant makes its reader fail; re-adding it recovers exactly the
	/// reader, leaving the untouched functions cached.
	void incrementalConstRemovedThenReadded() {
		const std::string v1 = R"(
			const X: i64 = 5;
			fun reader() -> i64 = { return X; }
			fun other() -> i64 = { return 7; }
			fun main() -> i64 = { return reader() + other(); }
		)";
		const std::string v2 = R"(
			fun reader() -> i64 = { return X; }
			fun other() -> i64 = { return 7; }
			fun main() -> i64 = { return reader() + other(); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_const_removed.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(12), runExecutable(executable));

		try {
			compiler.update(fs::FileManager::createRandomVirtualFile(v2));
			ASSERT_TRUE(false);
		} catch (const std::runtime_error& e) { ASSERT_TRUE(std::string(e.what()).contains("X")); }

		pg::CompileStats restored = compiler.update(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(usize(3), restored.total_functions);
		ASSERT_EQUAL_PRINT(usize(1), restored.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(12), runExecutable(executable));
	}

	/// The same failing source twice: the second update rethrows the cached failure
	/// without recompiling; fixing the source afterwards recovers.
	void incrementalRepeatedFailureIsCached() {
		const std::string v1 = R"(
			fun other() -> i64 = { return 1; }
			fun main() -> i64 = { return 5; }
		)";
		const std::string v2 = R"(
			fun other() -> i64 = { return 1; }
			fun main() -> i64 = { return gmissing(1); }
		)";
		const std::string v3 = R"(
			fun other() -> i64 = { return 1; }
			fun gmissing(p: i64) -> i64 = { return p + 5; }
			fun main() -> i64 = { return gmissing(1); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_repeat_fail.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(5), runExecutable(executable));

		for (usize attempt = 0; attempt < 2; ++attempt) {
			try {
				compiler.update(fs::FileManager::createRandomVirtualFile(v2));
				ASSERT_TRUE(false);
			} catch (const std::runtime_error& e) {
				ASSERT_TRUE(std::string(e.what()).contains("gmissing"));
			}
		}

		pg::CompileStats fixed = compiler.update(fs::FileManager::createRandomVirtualFile(v3));
		ASSERT_EQUAL_PRINT(usize(3), fixed.total_functions);
		ASSERT_EQUAL_PRINT(usize(2), fixed.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(6), runExecutable(executable));
	}

	/// Changing a return type breaks the caller; reverting recovers exactly the
	/// function and its failed caller.
	void incrementalSignatureRevert() {
		const std::string v1 = R"(
			fun f() -> i64 = { return 1; }
			fun g() -> i64 = { return f() + 1; }
			fun other() -> i64 = { return 7; }
			fun main() -> i64 = { return g() + other(); }
		)";
		const std::string v2 = R"(
			fun f() -> bool = { return true; }
			fun g() -> i64 = { return f() + 1; }
			fun other() -> i64 = { return 7; }
			fun main() -> i64 = { return g() + other(); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_sig_revert.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(9), runExecutable(executable));

		try {
			compiler.update(fs::FileManager::createRandomVirtualFile(v2));
			ASSERT_TRUE(false);
		} catch (const std::runtime_error& e) {
			ASSERT_TRUE(std::string(e.what()).contains("operand"));
		}

		pg::CompileStats restored = compiler.update(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(usize(4), restored.total_functions);
		ASSERT_EQUAL_PRINT(usize(2), restored.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(9), runExecutable(executable));
	}

	/// Removing a class used by one function fails that function; re-adding the
	/// class recovers it alone.
	void incrementalClassRemovedThenReadded() {
		const std::string v1 = R"(
			class Point {
				var x: i64;
			}
			fun reader() -> i64 = {
				var p: Point;
				p.x = 3;
				return p.x;
			}
			fun other() -> i64 = { return 7; }
			fun main() -> i64 = { return reader() + other(); }
		)";
		const std::string v2 = R"(
			fun reader() -> i64 = {
				var p: Point;
				p.x = 3;
				return p.x;
			}
			fun other() -> i64 = { return 7; }
			fun main() -> i64 = { return reader() + other(); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_class_removed.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));

		try {
			compiler.update(fs::FileManager::createRandomVirtualFile(v2));
			ASSERT_TRUE(false);
		} catch (const std::runtime_error& e) {
			ASSERT_TRUE(std::string(e.what()).contains("Point"));
		}

		pg::CompileStats restored = compiler.update(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(usize(3), restored.total_functions);
		ASSERT_EQUAL_PRINT(usize(1), restored.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));
	}

	/// Renaming a function breaks its caller; renaming back recompiles the function
	/// and the caller, leaving the unrelated function cached.
	void incrementalFunctionRenameAndBack() {
		const std::string v1 = R"(
			fun helper() -> i64 = { return 10; }
			fun other() -> i64 = { return 1; }
			fun main() -> i64 = { return helper(); }
		)";
		const std::string v2 = R"(
			fun helper2() -> i64 = { return 10; }
			fun other() -> i64 = { return 1; }
			fun main() -> i64 = { return helper(); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_rename.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));

		try {
			compiler.update(fs::FileManager::createRandomVirtualFile(v2));
			ASSERT_TRUE(false);
		} catch (const std::runtime_error& e) {
			ASSERT_TRUE(std::string(e.what()).contains("helper"));
		}

		pg::CompileStats restored = compiler.update(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(usize(3), restored.total_functions);
		ASSERT_EQUAL_PRINT(usize(2), restored.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));
	}

	/// A multi-step editing session: per-symbol dependencies stay precise across a
	/// chain of body, constant and initializer edits.
	void incrementalMultiStepSession() {
		const std::string s1 = R"(
			const A: i64 = 1;
			const B: i64 = 2;
			var g: i64 = 3;
			fun fa() -> i64 = { return A; }
			fun fb() -> i64 = { return B; }
			fun fg() -> i64 = { return g; }
			fun main() -> i64 = { return fa() + fb() + fg(); }
		)";
		const std::string s2 = R"(
			const A: i64 = 10;
			const B: i64 = 2;
			var g: i64 = 3;
			fun fa() -> i64 = { return A; }
			fun fb() -> i64 = { return B; }
			fun fg() -> i64 = { return g; }
			fun main() -> i64 = { return fa() + fb() + fg(); }
		)";
		const std::string s3 = R"(
			const A: i64 = 10;
			const B: i64 = 2;
			var g: i64 = 30;
			fun fa() -> i64 = { return A; }
			fun fb() -> i64 = { return B; }
			fun fg() -> i64 = { return g; }
			fun main() -> i64 = { return fa() + fb() + fg(); }
		)";
		const std::string s4 = R"(
			const A: i64 = 10;
			const B: i64 = 2;
			var g: i64 = 30;
			fun fa() -> i64 = { return A; }
			fun fb() -> i64 = { return B + 1; }
			fun fg() -> i64 = { return g; }
			fun main() -> i64 = { return fa() + fb() + fg(); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_multi_step.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string() }
		);

		compiler.compile(fs::FileManager::createRandomVirtualFile(s1));
		ASSERT_EQUAL_PRINT(u64(6), runExecutable(executable));

		pg::CompileStats step2 = compiler.update(fs::FileManager::createRandomVirtualFile(s2));
		ASSERT_EQUAL_PRINT(usize(1), step2.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(15), runExecutable(executable));

		pg::CompileStats step3 = compiler.update(fs::FileManager::createRandomVirtualFile(s3));
		ASSERT_EQUAL_PRINT(usize(0), step3.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(42), runExecutable(executable));

		pg::CompileStats step4 = compiler.update(fs::FileManager::createRandomVirtualFile(s4));
		ASSERT_EQUAL_PRINT(usize(1), step4.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(43), runExecutable(executable));

		pg::CompileStats step5 = compiler.update(fs::FileManager::createRandomVirtualFile(s4));
		ASSERT_EQUAL_PRINT(usize(0), step5.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(43), runExecutable(executable));
	}

	/// Incremental updates also work in --sequential mode (hash diff without PSAC).
	void incrementalSequentialMode() {
		const std::string v1 = R"(
			fun helper() -> i64 = { return 10; }
			fun main() -> i64 = { return helper(); }
		)";
		const std::string v2 = R"(
			fun helper() -> i64 = { return 30; }
			fun main() -> i64 = { return helper(); }
		)";

		fs::File out_dir    = fs::FileManager::createRandomTempDirectory();
		fs::File executable = out_dir.createSubFile("", "incr_seq.out");

		pg::Compiler compiler(pg::CompileOptions{ .output_path = executable.getFilePath().string(),
		                                          .sequential  = true });

		compiler.compile(fs::FileManager::createRandomVirtualFile(v1));
		ASSERT_EQUAL_PRINT(u64(10), runExecutable(executable));

		pg::CompileStats incremental
			= compiler.update(fs::FileManager::createRandomVirtualFile(v2));
		ASSERT_EQUAL_PRINT(usize(1), incremental.recompiled_functions);
		ASSERT_EQUAL_PRINT(u64(30), runExecutable(executable));
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		for (const psack_corpus::SingleFileCase& corpus_case: psack_corpus::singleFileCases()) {
			addTest(
				static_cast<TestType>(&PsacklingerTest::singleFileCorpusTest),
				corpus_case.name,
				false
			);
		}
		for (const psack_corpus::MultiFileCase& corpus_case: psack_corpus::multiFileCases()) {
			addTest(
				static_cast<TestType>(&PsacklingerTest::multiFileCorpusTest), corpus_case.name, false
			);
		}

		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalBodyChange),
			"IncrementalBodyChange",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalGlobalsChange),
			"IncrementalGlobalsChange",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalGlobalVarChange),
			"IncrementalGlobalVarChange",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalFunctionSetChange),
			"IncrementalFunctionSetChange",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalConstOnlyReadersRecompile),
			"IncrementalConstOnlyReadersRecompile",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalArraySizeConstChange),
			"IncrementalArraySizeConstChange",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalParamRenameOnlySelf),
			"IncrementalParamRenameOnlySelf",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalClassFieldOnlyUsersRecompile),
			"IncrementalClassFieldOnlyUsersRecompile",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalFunctionRemoved),
			"IncrementalFunctionRemoved",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalMissingFunctionThenAdded),
			"IncrementalMissingFunctionThenAdded",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalConstRemovedThenReadded),
			"IncrementalConstRemovedThenReadded",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalRepeatedFailureIsCached),
			"IncrementalRepeatedFailureIsCached",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalSignatureRevert),
			"IncrementalSignatureRevert",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalClassRemovedThenReadded),
			"IncrementalClassRemovedThenReadded",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalFunctionRenameAndBack),
			"IncrementalFunctionRenameAndBack",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalMultiStepSession),
			"IncrementalMultiStepSession",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalSequentialMode),
			"IncrementalSequentialMode",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalImportedBodyChange),
			"IncrementalImportedBodyChange",
			false
		);
		addTest(
			static_cast<TestType>(&PsacklingerTest::incrementalImportedConstChange),
			"IncrementalImportedConstChange",
			false
		);
	}
};

TESTER_COMMON_MAIN("/src/playground/psac_stuff/psac_compiler/tests");
