#include <dlfcn.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <os_utils/dynamic_library.hpp>
#include <os_utils/exec_self.hpp>
#include <os_utils/executable_path.hpp>
#include <os_utils/memory.hpp>
#include <os_utils/terminal.hpp>
#include <os_utils/timed_mutex_recovery.hpp>
#include <tester/tester.hpp>

#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#if !defined(_WIN32)
	#if defined(__APPLE__)
		#include <util.h>
	#else
		#include <pty.h>
	#endif
namespace {
	// Master/slave pseudo-terminal pair for headless terminal tests.
	struct PtyPair {
		int master = -1;
		int slave  = -1;

		PtyPair() = default;

		static PtyPair create() {
			PtyPair pty;
			if (openpty(&pty.master, &pty.slave, nullptr, nullptr, nullptr) == -1) {
				pty.master = -1;
				pty.slave  = -1;
			}
			return pty;
		}

		void closeMaster() {
			if (master != -1) {
				close(master);
				master = -1;
			}
		}

		PtyPair(PtyPair&& other) noexcept: master{ other.master }, slave{ other.slave } {
			other.master = -1;
			other.slave  = -1;
		}

		~PtyPair() {
			if (slave != -1) close(slave);
			closeMaster();
		}

		PtyPair(const PtyPair&)            = delete;
		PtyPair& operator=(const PtyPair&) = delete;
	};

	// Temporarily replaces fd `target` with `replacement`, restoring it on destruction.
	struct FdRedirect {
		FdRedirect(int target, int replacement): m_target{ target }, m_saved{ dup(target) } {
			if (m_saved != -1 && replacement != -1) dup2(replacement, target);
		}

		~FdRedirect() {
			if (m_saved != -1) {
				dup2(m_saved, m_target);
				close(m_saved);
			}
		}

		FdRedirect(const FdRedirect&)            = delete;
		FdRedirect& operator=(const FdRedirect&) = delete;

	private:
		int m_target = -1;
		int m_saved  = -1;
	};
}
#endif

class OSUtilsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS OSUtilsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(pageSizeTest);
		TESTER_ADD_TEST(allocateFreePagesTest);
		TESTER_ADD_TEST(allocatePagesFailureTest);
		TESTER_ADD_TEST(markExecutableTest);
		TESTER_ADD_TEST(markExecutableNullptr);
		TESTER_ADD_TEST(openCloseLibraryTest);
		TESTER_ADD_TEST(findSymbolTest);
		TESTER_ADD_TEST(openLibraryFromMemoryTest);
		TESTER_ADD_TEST(openLibraryFromMemoryEmptyBuffer);
		TESTER_ADD_TEST(openLibraryFromMemoryCorruptBuffer);
		TESTER_ADD_TEST(executablePathTest);
		TESTER_ADD_TEST(openLibraryBadPath);
		TESTER_ADD_TEST(findSymbolBadName);
		TESTER_ADD_TEST(closeLibraryDefaultConstructed);
		TESTER_ADD_TEST(freeNullPages);
#if !defined(_WIN32)
		TESTER_ADD_TEST(writeStrWritesToStdout);
		TESTER_ADD_TEST(writeCharWritesToStdout);
		TESTER_ADD_TEST(readCharReadsFromStdin);
		TESTER_ADD_TEST(readCharEofTest);
		TESTER_ADD_TEST(clearScreenEmitsEscapeSequence);
		TESTER_ADD_TEST(rawTerminalModeSetsAndRestoresTermios);
		TESTER_ADD_TEST(rawTerminalModeFailsOnNonTty);
		TESTER_ADD_TEST(rawTerminalModeMoveAssignment);
		TESTER_ADD_TEST(rawTerminalModeExplicitRestore);
		TESTER_ADD_TEST(execSelfSuccessPath);
#endif
		TESTER_ADD_TEST(execSelfErrorPath);
		TESTER_ADD_TEST(clearAbandonedLockTest);
	}

private:
	void pageSizeTest() {
		auto result = os_utils::getPageSize();
		assertTrue(result.has_value(), "getPageSize should succeed");
		assertTrue(*result > 0, "Page size should be positive");
	}

	void allocateFreePagesTest() {
		auto page_size = os_utils::getPageSize();
		assertTrue(page_size.has_value(), "getPageSize should succeed");

		auto memory = os_utils::allocatePages(*page_size);
		assertTrue(memory.has_value(), "allocatePages should succeed");
		assertTrue(*memory != nullptr, "Allocated memory should not be null");

		// allocatePages documents zero-initialization; verify it before writing.
		bool zeroed = true;
		for (usize i = 0; i < *page_size; ++i) {
			if ((*memory)[i] != byte{ 0 }) {
				zeroed = false;
				break;
			}
		}
		assertTrue(zeroed, "Allocated memory should be zero-initialized");

		// Write to verify the memory is usable.
		(*memory)[0] = byte{ 42 };

		os_utils::freePages(*memory, *page_size);
	}

	void allocatePagesFailureTest() {
		// Requesting an impossibly large allocation must return an error, not
		// crash or return nullptr.
		auto memory = os_utils::allocatePages(std::numeric_limits<usize>::max());
		assertTrue(!memory.has_value(), "allocatePages(SIZE_MAX) should fail");
	}

	void markExecutableTest() {
		auto page_size = os_utils::getPageSize();
		assertTrue(page_size.has_value(), "getPageSize should succeed");

		auto memory = os_utils::allocatePages(*page_size);
		assertTrue(memory.has_value(), "allocatePages should succeed");

		// Write a 'ret' instruction (0xC3 on x86-64) before marking it
		// executable; mprotect removes PROT_WRITE afterwards.
		(*memory)[0] = byte{ 0xC3 };

		auto result = os_utils::markExecutable(*memory, *page_size);
		assertTrue(result.has_value(), "markExecutable should succeed");

		// NOTE: actually calling the code (cast to function pointer) is the
		// definitive test, but hardened kernels (SELinux deny_execmem) may
		// block execution from anonymous mappings even after a successful
		// mprotect, causing a segfault. The mprotect return value is the
		// best portable signal we have.

		os_utils::freePages(*memory, *page_size);
	}

	void markExecutableNullptr() {
		auto page_size = os_utils::getPageSize();
		assertTrue(page_size.has_value(), "getPageSize should succeed");

		// mprotect on a null address is invalid; the function must fail, not crash.
		auto result = os_utils::markExecutable(nullptr, *page_size);
		assertTrue(!result.has_value(), "markExecutable(nullptr) should fail");
	}

	void openCloseLibraryTest() {
		auto lib = os_utils::openLibrary(nullptr);
		assertTrue(lib.has_value(), "openLibrary(nullptr) should succeed on all POSIX systems");
		assertTrue((*lib).handle != nullptr, "Library handle should be non-null");
		os_utils::closeLibrary(*lib);
	}

	void findSymbolTest() {
		auto lib = os_utils::openLibrary(nullptr);
		assertTrue(lib.has_value(), "openLibrary(nullptr) should succeed");

		auto sym = os_utils::findSymbol(*lib, "strlen");
		assertTrue(sym.has_value(), "findSymbol should succeed");
		assertTrue(*sym != nullptr, "strlen symbol should be found");

		// Verify the symbol is callable and correct.
		using StrlenFunc = unsigned long (*)(const char*);
		auto strlen_func = reinterpret_cast<StrlenFunc>(*sym);
		ASSERT_EQUAL(5UL, strlen_func("hello"));

		os_utils::closeLibrary(*lib);
	}

	void openLibraryFromMemoryTest() {
		// 1. Use dladdr to find the absolute path to libc on THIS platform.
		//    strlen is in libc, which is loaded into every process.
		auto main_handle = os_utils::openLibrary(nullptr);
		assertTrue(main_handle.has_value(), "main program handle should be valid");
		auto strlen_addr = os_utils::findSymbol(*main_handle, "strlen");
		assertTrue(strlen_addr.has_value(), "strlen must be locatable in main program");
		os_utils::closeLibrary(*main_handle);

		Dl_info info{};
		int     ret = dladdr(*strlen_addr, &info);
		assertTrue(ret != 0, "dladdr must succeed for strlen");
		const char* libc_path = info.dli_fname;

		// 2. Read the library file into memory.
		std::ifstream file(libc_path, std::ios::binary | std::ios::ate);
		assertTrue(file.is_open(), "must be able to open the detected libc");
		auto file_size = static_cast<usize>(file.tellg());
		file.seekg(0, std::ios::beg);
		std::vector<byte> buffer(file_size);
		file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size));
		assertTrue(static_cast<usize>(file.gcount()) == file_size, "must read entire libc file");

		// 3. Load the library from memory (the function under test).
		auto lib = os_utils::openLibraryFromMemory(buffer);
		assertTrue(lib.has_value(), "openLibraryFromMemory should succeed");

		// 4. Look up a symbol; dlsym must work on memory-loaded libraries.
		auto sym = os_utils::findSymbol(*lib, "strlen");
		assertTrue(sym.has_value(), "strlen should be found in memory-loaded lib");
		assertTrue(*sym != nullptr, "strlen symbol should not be null");

		// 5. Call the symbol to prove the loaded code is executable.
		using StrlenFunc = unsigned long (*)(const char*);
		auto func        = reinterpret_cast<StrlenFunc>(*sym);
		ASSERT_EQUAL(5UL, func("hello"));

		// 6. Close; cleanup must not crash.
		os_utils::closeLibrary(*lib);
	}

	void openLibraryFromMemoryEmptyBuffer() {
		// An empty buffer cannot be a shared library; must return an error, not crash.
		std::vector<byte> empty{};
		auto              lib = os_utils::openLibraryFromMemory(empty);
		assertTrue(!lib.has_value(), "openLibraryFromMemory(empty) should fail");
	}

	void openLibraryFromMemoryCorruptBuffer() {
		// Valid writable bytes that are not a shared library; dlopen must reject them.
		std::vector<byte> garbage(4'096, byte{ 0xAB });
		auto              lib = os_utils::openLibraryFromMemory(garbage);
		assertTrue(!lib.has_value(), "openLibraryFromMemory(garbage) should fail");
	}

	void executablePathTest() {
		auto path = os_utils::getExecutablePath();
		assertTrue(!path.empty(), "Executable path should not be empty");
		assertTrue(path.isAbsolute(), "Executable path should be absolute");
		assertTrue(path.exists(), "Executable path should point to an existing file");
	}

	void openLibraryBadPath() {
		auto lib = os_utils::openLibrary("/nonexistent/path.so");
		assertTrue(!lib.has_value(), "openLibrary with bad path should fail");
	}

	void findSymbolBadName() {
		auto lib = os_utils::openLibrary(nullptr);
		assertTrue(lib.has_value(), "openLibrary(nullptr) should succeed");

		auto sym = os_utils::findSymbol(*lib, "nonexistent_symbol_xyzzy");
		assertTrue(!sym.has_value(), "findSymbol with bad name should fail");

		os_utils::closeLibrary(*lib);
	}

	void closeLibraryDefaultConstructed() {
		// Must not crash or assert.
		os_utils::NativeLibrary empty{};
		os_utils::closeLibrary(empty);
	}

	void freeNullPages() {
		// The implementation guards against nullptr. Must not crash.
		os_utils::freePages(nullptr, 4'096);
	}

#if !defined(_WIN32)
	void writeStrWritesToStdout() {
		auto pty = PtyPair::create();
		assertTrue(pty.master != -1 && pty.slave != -1, "PTY creation should succeed");
		FdRedirect redirect(STDOUT_FILENO, pty.slave);

		os_utils::writeStr("abc");

		std::array<char, 16> buffer{};
		ssize_t              n = read(pty.master, buffer.data(), buffer.size());
		assertTrue(n == 3, "writeStr should write exactly 3 bytes");
		assertTrue(std::string_view(buffer.data(), 3) == "abc", "writeStr output should be 'abc'");
	}

	void writeCharWritesToStdout() {
		auto pty = PtyPair::create();
		assertTrue(pty.master != -1 && pty.slave != -1, "PTY creation should succeed");
		FdRedirect redirect(STDOUT_FILENO, pty.slave);

		os_utils::writeChar('z');

		std::array<char, 16> buffer{};
		ssize_t              n = read(pty.master, buffer.data(), buffer.size());
		assertTrue(n == 1, "writeChar should write exactly 1 byte");
		ASSERT_EQUAL('z', buffer[0]);
	}

	void readCharReadsFromStdin() {
		auto pty = PtyPair::create();
		assertTrue(pty.master != -1 && pty.slave != -1, "PTY creation should succeed");
		FdRedirect redirect(STDIN_FILENO, pty.slave);

		// Canonical mode buffers input until a newline; readChar then returns the first char.
		ASSERT_EQUAL(2, write(pty.master, "x\n", 2));

		char c = '\0';
		assertTrue(os_utils::readChar(c), "readChar should succeed with pending input");
		ASSERT_EQUAL('x', c);
	}

	void readCharEofTest() {
		auto pty = PtyPair::create();
		assertTrue(pty.master != -1 && pty.slave != -1, "PTY creation should succeed");
		FdRedirect redirect(STDIN_FILENO, pty.slave);

		// Closing the master makes slave-side reads return EOF.
		pty.closeMaster();

		char c = '\0';
		assertTrue(!os_utils::readChar(c), "readChar should return false on EOF");
	}

	void clearScreenEmitsEscapeSequence() {
		auto pty = PtyPair::create();
		assertTrue(pty.master != -1 && pty.slave != -1, "PTY creation should succeed");
		FdRedirect redirect(STDOUT_FILENO, pty.slave);

		os_utils::clearScreen();

		constexpr std::string_view EXPECTED = "\033c\033[H\033[2J\033[0m";
		std::array<char, 32>       buffer{};
		ssize_t                    n = read(pty.master, buffer.data(), buffer.size());
		assertTrue(n == static_cast<ssize_t>(EXPECTED.size()), "clearScreen output size mismatch");
		assertTrue(
			std::string_view(buffer.data(), static_cast<usize>(n)) == EXPECTED,
			"clearScreen should emit the ANSI clear sequence"
		);
	}

	void rawTerminalModeSetsAndRestoresTermios() {
		auto pty = PtyPair::create();
		assertTrue(pty.master != -1 && pty.slave != -1, "PTY creation should succeed");
		FdRedirect redirect(STDIN_FILENO, pty.slave);

		termios original{};
		ASSERT_EQUAL(0, tcgetattr(STDIN_FILENO, &original));

		{
			auto guard = os_utils::RawTerminalMode::create();
			assertTrue(guard.has_value(), "RawTerminalMode::create should succeed on a TTY");

			termios raw{};
			ASSERT_EQUAL(0, tcgetattr(STDIN_FILENO, &raw));
			assertTrue(
				(raw.c_lflag & (ECHO | ICANON)) == 0, "raw mode should clear ECHO and ICANON"
			);
			ASSERT_EQUAL(1, raw.c_cc[VMIN]);
			ASSERT_EQUAL(0, raw.c_cc[VTIME]);
		}

		termios restored{};
		ASSERT_EQUAL(0, tcgetattr(STDIN_FILENO, &restored));
		assertTrue(
			std::memcmp(&restored, &original, sizeof(termios)) == 0,
			"RawTerminalMode destruction should restore the original settings"
		);
	}

	void rawTerminalModeFailsOnNonTty() {
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
		int devnull = open("/dev/null", O_RDONLY);
		assertTrue(devnull != -1, "opening /dev/null should succeed");
		FdRedirect redirect(STDIN_FILENO, devnull);
		close(devnull);

		auto guard = os_utils::RawTerminalMode::create();
		assertTrue(!guard.has_value(), "RawTerminalMode::create should fail on a non-TTY stdin");
	}

	void rawTerminalModeMoveAssignment() {
		auto pty = PtyPair::create();
		assertTrue(pty.master != -1 && pty.slave != -1, "PTY creation should succeed");
		FdRedirect redirect(STDIN_FILENO, pty.slave);

		termios original{};
		ASSERT_EQUAL(0, tcgetattr(STDIN_FILENO, &original));

		{
			auto first = os_utils::RawTerminalMode::create();
			assertTrue(first.has_value(), "first create should succeed");

			// A second guard created while raw mode is active captures raw as its
			// baseline; moving the first (clean) guard into it must transfer the
			// clean baseline and keep raw mode on.
			auto second = os_utils::RawTerminalMode::create();
			assertTrue(second.has_value(), "second create should succeed");
			*second = std::move(*first);

			termios raw{};
			ASSERT_EQUAL(0, tcgetattr(STDIN_FILENO, &raw));
			assertTrue(
				(raw.c_lflag & (ECHO | ICANON)) == 0,
				"raw mode should remain active after move assignment"
			);
		}

		termios restored{};
		ASSERT_EQUAL(0, tcgetattr(STDIN_FILENO, &restored));
		assertTrue(
			std::memcmp(&restored, &original, sizeof(termios)) == 0,
			"the moved-into guard should restore the original settings"
		);
	}

	void rawTerminalModeExplicitRestore() {
		auto pty = PtyPair::create();
		assertTrue(pty.master != -1 && pty.slave != -1, "PTY creation should succeed");
		FdRedirect redirect(STDIN_FILENO, pty.slave);

		termios original{};
		ASSERT_EQUAL(0, tcgetattr(STDIN_FILENO, &original));

		{
			auto guard = os_utils::RawTerminalMode::create();
			assertTrue(guard.has_value(), "create should succeed on a TTY");

			// restore() succeeds and puts the settings back.
			auto res = guard->restore();
			assertTrue(res.has_value(), "restore should succeed");
			termios after{};
			ASSERT_EQUAL(0, tcgetattr(STDIN_FILENO, &after));
			assertTrue(
				std::memcmp(&after, &original, sizeof(termios)) == 0,
				"explicit restore should restore the original settings"
			);

			// Calling restore() again does nothing.
			assertTrue(guard->restore().has_value(), "second restore should be a no-op");
		}

		// restore() reports an error when stdin is gone.
		auto guard = os_utils::RawTerminalMode::create();
		assertTrue(guard.has_value(), "create should succeed on a TTY");
		close(STDIN_FILENO);
		assertTrue(!guard->restore().has_value(), "restore should fail when stdin is closed");
	}

	void execSelfSuccessPath() {
		pid_t pid = fork();
		assertTrue(pid >= 0, "fork should succeed");
		if (pid == 0) {
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
			int devnull = open("/dev/null", O_WRONLY);
			if (devnull != -1) {
				dup2(devnull, STDOUT_FILENO);
				dup2(devnull, STDERR_FILENO);
			}
			// Re-exec with a filter matching no tests: the child runs the suite
			// with zero tests and exits 0.
			std::vector<std::string> argv{
				os_utils::getExecutablePath().genericString(),
				"no_such_test_xyzzy",
			};
			os_utils::execSelf(argv);
			_exit(1);  // execSelf must not return on success
		}

		int status = 0;
		ASSERT_EQUAL(pid, waitpid(pid, &status, 0));
		assertTrue(WIFEXITED(status), "re-executed process should exit normally");
		ASSERT_EQUAL(0, WEXITSTATUS(status));
	}
#endif

	void execSelfErrorPath() {
		std::vector<std::string> argv{ "/nonexistent/duckling_binary_xyzzy", "" };
		auto                     result = os_utils::execSelf(argv);
		assertTrue(
			result.status == os_utils::ExecSelfStatus::Error,
			"execSelf with a nonexistent path should return an error"
		);
		assertTrue(result.error_code != 0, "errno should be set on execSelf failure");
	}

	void clearAbandonedLockTest() {
		std::timed_mutex mutex;
		os_utils::clearAbandonedLock(mutex);  // unlocked; must not crash
		mutex.lock();
		os_utils::clearAbandonedLock(mutex);  // no-op on Linux, clears the lock on macOS
#ifndef __APPLE__
		mutex.unlock();                       // on Linux the lock is still held by us
#endif
	}
};

TESTER_COMMON_MAIN("/src/common/os_utils/tests/")
