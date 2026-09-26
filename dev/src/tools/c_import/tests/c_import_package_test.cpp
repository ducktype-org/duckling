#include <c_import/options.hpp>
#include <c_import/package_writer.hpp>
#include <c_import/run.hpp>
#include <c_import/verify.hpp>

#include <tester/tester.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace c_import;

namespace {

	/**
	 * @brief Owns the argument strings for the lifetime of the pointers handed out.
	 *
	 * Returning the pointers from a function taking the strings by reference leaves them
	 * dangling the moment the caller's temporary dies, which reads as working on one platform
	 * and not on another.
	 */
	class Args final {
	public:
		explicit Args(std::vector<std::string> arguments): m_storage(std::move(arguments)) {
			m_pointers.reserve(m_storage.size());
			for (const auto& argument: m_storage) m_pointers.push_back(argument.c_str());
		}

		[[nodiscard]] int argc() const { return static_cast<int>(m_pointers.size()); }

		[[nodiscard]] const char* const* argv() const { return m_pointers.data(); }

	private:
		std::vector<std::string> m_storage;
		std::vector<const char*> m_pointers;
	};

	bool fileHolds(const std::filesystem::path& path, const std::string& expected) {
		std::ifstream file(path, std::ios::binary);
		if (!file) return false;
		const std::string contents(
			(std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()
		);
		return contents == expected;
	}

	PackageLayout sampleLayout() {
		return PackageLayout{ .package_name  = "mylib",
			                  .manifest_yaml = "metadata:\n",
			                  .root_module   = "# root\n",
			                  .modules
			                  = { PackageFile{ .name = "mylib", .contents = "# bindings\n" } } };
	}

}

class CImportPackageTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS CImportPackageTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(splitArgumentsTest);
		TESTER_ADD_TEST(writePackageTest);
		TESTER_ADD_TEST(forceTest);
		TESTER_ADD_TEST(missingCompilerTest);
		TESTER_ADD_TEST(runTest);
		TESTER_ADD_TEST(runRejectsBadInputTest);
	}

private:
	/** A fresh directory nobody else is using, removed when the test is done. */
	class TempDir final {
	public:
		TempDir():
			  m_path(
				  std::filesystem::temp_directory_path()
				  / ("duck_c_import_test_" + std::to_string(counter()))
			  ) {
			std::error_code error_code;
			std::filesystem::remove_all(m_path, error_code);
		}

		TempDir(const TempDir&)            = delete;
		TempDir& operator=(const TempDir&) = delete;
		TempDir(TempDir&&)                 = delete;
		TempDir& operator=(TempDir&&)      = delete;

		~TempDir() {
			std::error_code error_code;
			std::filesystem::remove_all(m_path, error_code);
		}

		[[nodiscard]] const std::filesystem::path& path() const { return m_path; }

	private:
		static int counter() {
			static int next = 0;
			return next++;
		}

		std::filesystem::path m_path;
	};

	void splitArgumentsTest() {
		// Everything after the first bare `--` belongs to clang.
		const Args arguments({ "duck_c_import", "--header", "a.h", "--", "-I/inc", "-DFOO=1" });
		const auto split = splitArguments(arguments.argc(), arguments.argv());

		ASSERT_EQUAL(std::size_t{ 3 }, split.own.size());
		ASSERT_EQUAL(std::string("--header"), split.own.at(1));
		ASSERT_EQUAL(std::size_t{ 2 }, split.clang.size());
		ASSERT_EQUAL(std::string("-I/inc"), split.clang.at(0));

		// A later `--` is a clang argument, not another separator.
		const Args nested({ "duck_c_import", "--", "-Xclang", "--", "-v" });
		const auto second = splitArguments(nested.argc(), nested.argv());
		ASSERT_EQUAL(std::size_t{ 1 }, second.own.size());
		ASSERT_EQUAL(std::size_t{ 3 }, second.clang.size());

		// No separator at all means no clang arguments.
		const Args plain({ "duck_c_import", "--header", "a.h" });
		const auto third = splitArguments(plain.argc(), plain.argv());
		ASSERT_EQUAL(std::size_t{ 3 }, third.own.size());
		ASSERT_TRUE(third.clang.empty());
	}

	void writePackageTest() {
		const TempDir directory;

		const auto written = writePackage(directory.path(), sampleLayout(), false);
		ASSERT_TRUE(written.error.empty());

		ASSERT_TRUE(fileHolds(directory.path() / "quackconfig.yaml", "metadata:\n"));
		ASSERT_TRUE(fileHolds(directory.path() / "src" / "src.dk", "# root\n"));
		ASSERT_TRUE(fileHolds(directory.path() / "src" / "mylib.dk", "# bindings\n"));

		// A directory holding something already is not overwritten by accident.
		const auto refused = writePackage(directory.path(), sampleLayout(), false);
		ASSERT_EQUAL(false, refused.error.empty());
	}

	void forceTest() {
		const TempDir directory;
		ASSERT_TRUE(writePackage(directory.path(), sampleLayout(), false).error.empty());

		// A module from an earlier run would still be compiled, so it has to go.
		std::filesystem::create_directories(directory.path() / "src");
		{
			std::ofstream stale(directory.path() / "src" / "stale.dk");
			stale << "# left over\n";
		}
		ASSERT_TRUE(std::filesystem::exists(directory.path() / "src" / "stale.dk"));

		ASSERT_TRUE(writePackage(directory.path(), sampleLayout(), true).error.empty());
		ASSERT_EQUAL(false, std::filesystem::exists(directory.path() / "src" / "stale.dk"));
		ASSERT_TRUE(fileHolds(directory.path() / "src" / "mylib.dk", "# bindings\n"));
	}

	void missingCompilerTest() {
		const TempDir directory;
		ASSERT_TRUE(writePackage(directory.path(), sampleLayout(), false).error.empty());

		// Nothing is known about the package when the compiler never ran, so this must not be
		// reported as a translation failure.
		const auto result
			= verifyPackage(directory.path(), "mylib", "duck_c_import_no_such_compiler");
		ASSERT_EQUAL(false, result.ok);
		ASSERT_TRUE(result.compiler_missing);
		ASSERT_EQUAL(false, result.command.empty());
	}

	Options baseOptions(const std::filesystem::path& out_dir) {
		Options options;
		options.package_name = "scalars";
		options.out_dir      = out_dir.string();
		options.version      = "1.0.0";
		options.std_flag     = "-std=c17";
		// Running the compiler is the itest's job; this is about the translator itself.
		options.verify = false;
		return options;
	}

	void runTest() {
		const TempDir directory;

		auto options = baseOptions(directory.path());
		options.headers.push_back(path("headers/scalars.h"));

		ASSERT_EQUAL(0, c_import::run(options));
		ASSERT_TRUE(std::filesystem::exists(directory.path() / "quackconfig.yaml"));
		ASSERT_TRUE(std::filesystem::exists(directory.path() / "src" / "src.dk"));
		ASSERT_TRUE(std::filesystem::exists(directory.path() / "src" / "scalars.dk"));

		// A second run into the same place needs --force.
		ASSERT_EQUAL(1, c_import::run(options));
		options.force = true;
		ASSERT_EQUAL(0, c_import::run(options));

		// Splitting puts each header in its own module.
		const TempDir split_directory;
		auto          split_options = baseOptions(split_directory.path());
		split_options.headers.push_back(path("headers/scalars.h"));
		split_options.headers.push_back(path("headers/records.h"));
		split_options.split = true;

		ASSERT_EQUAL(0, c_import::run(split_options));
		ASSERT_TRUE(std::filesystem::exists(split_directory.path() / "src" / "scalars.dk"));
		ASSERT_TRUE(std::filesystem::exists(split_directory.path() / "src" / "records.dk"));
	}

	void runRejectsBadInputTest() {
		const TempDir directory;

		// No headers at all.
		auto no_headers = baseOptions(directory.path());
		ASSERT_EQUAL(1, c_import::run(no_headers));

		// No package name.
		auto no_name = baseOptions(directory.path());
		no_name.headers.push_back(path("headers/scalars.h"));
		no_name.package_name.clear();
		ASSERT_EQUAL(1, c_import::run(no_name));

		// A header that does not parse.
		auto broken = baseOptions(directory.path());
		broken.headers.push_back(path("headers/this_header_does_not_exist.h"));
		ASSERT_EQUAL(1, c_import::run(broken));

		// A library path holding a space cannot survive the linker's one shell string.
		auto spaced = baseOptions(directory.path());
		spaced.headers.push_back(path("headers/scalars.h"));
		spaced.library_has_space = true;
		ASSERT_EQUAL(1, c_import::run(spaced));
	}
};

TESTER_COMMON_MAIN("/src/tools/c_import/tests/")
