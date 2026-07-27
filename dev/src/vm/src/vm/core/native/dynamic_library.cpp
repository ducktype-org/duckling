#include "dynamic_library.hpp"

#include <base/except/exceptions.hpp>

namespace vm::native {

	std::expected<DynamicLibrary, std::string> DynamicLibrary::tryFromFile(const char* path) {
		return os_utils::openLibrary(path).and_then(
			[](os_utils::NativeLibrary lib) -> std::expected<DynamicLibrary, std::string> {
				return DynamicLibrary{ lib };
			}
		);
	}

	std::expected<DynamicLibrary, std::string> DynamicLibrary::fromMemory(
		std::span<const byte> library_bytes
	) {
		return os_utils::openLibraryFromMemory(library_bytes)
		    .and_then([](os_utils::NativeLibrary lib) -> std::expected<DynamicLibrary, std::string> {
				return DynamicLibrary{ lib };
			});
	}

	DynamicLibrary::DynamicLibrary(DynamicLibrary&& dynlib) noexcept: lib{ dynlib.lib } {
		dynlib.lib = { .handle = nullptr, .fd = -1 };
	}

	DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& dynlib) noexcept {
		lib        = dynlib.lib;
		dynlib.lib = { .handle = nullptr, .fd = -1 };
		return *this;
	}

	DynamicLibrary::~DynamicLibrary() noexcept { os_utils::closeLibrary(lib); }

	std::byte* DynamicLibrary::findSymbol(const char* name) const {
		auto result = os_utils::findSymbol(lib, name);
		CORE_ASSERT(result.has_value(), "dlsym failed: ", result.error());
		return reinterpret_cast<std::byte*>(*result);
	}

	base::Optional<std::byte*> DynamicLibrary::maybeFindSymbol(const char* name) const {
		auto result = os_utils::findSymbol(lib, name);
		if (result.has_value())
			return reinterpret_cast<std::byte*>(*result);
		else
			return std::nullopt;
	}
}
