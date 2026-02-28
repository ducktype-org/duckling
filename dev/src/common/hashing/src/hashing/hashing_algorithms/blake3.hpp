#pragma once

#include <base/pointers/box.hpp>

namespace llvm {
	class BLAKE3;
}

DEFAULT_BOX_PTR_DELETER_DECLARATION(llvm::BLAKE3);

#include "../hash_algorithm_utils.hpp"

#include <base/types/bit256.hpp>

namespace hashing {
	class Blake3_256 final {
		Box<llvm::BLAKE3> state;

	public:
		using result_type = base::Bit256;

		Blake3_256();

		Blake3_256(const Blake3_256&);

		Blake3_256(Blake3_256&&) noexcept;

		Blake3_256& operator=(const Blake3_256&);

		Blake3_256& operator=(Blake3_256&&) noexcept;

		~Blake3_256() = default;


		Blake3_256& operator()(std::span<const std::byte> data);

		Blake3_256& operator()(internal::span_of_bytes auto data) { return (*this)(data); }

		[[nodiscard]] result_type finalize();
	};
}
