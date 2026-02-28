#include "blake3.hpp"

#include <llvm_helpers/llvm_helpers.hpp>

LLVM_INCLUDE_BEGIN()
#include <llvm/Support/BLAKE3.h>
LLVM_INCLUDE_END()

DEFAULT_BOX_PTR_DELETER_DEFINITION(llvm::BLAKE3);

#include <base/types/bit256.hpp>

namespace hashing {

	Blake3_256::Blake3_256(): state{ makeBox<llvm::BLAKE3>() } {}

	Blake3_256::Blake3_256(const Blake3_256& other): state{ makeBox<llvm::BLAKE3>(*other.state) } {}

	Blake3_256::Blake3_256(Blake3_256&& other) noexcept: state{ std::move(other.state) } {}

	Blake3_256& Blake3_256::operator=(const Blake3_256& other) {
		if (this != &other) state = makeBox<llvm::BLAKE3>(*other.state);
		return *this;
	}

	Blake3_256& Blake3_256::operator=(Blake3_256&& other) noexcept {
		if (this != &other) state = std::move(other.state);
		return *this;
	}

	Blake3_256& Blake3_256::operator()(std::span<const std::byte> data) {
		llvm::ArrayRef<uint8_t> bytes{ reinterpret_cast<const uint8_t*>(data.data()), data.size() };
		state->update(bytes);
		return *this;
	}

	[[nodiscard]] Blake3_256::result_type Blake3_256::finalize() { return { state->final() }; }

}
