#pragma once

#include <base/unique_pointer.hpp>

namespace tokenizer {
	class TokenFile;
	using BorrowFile = base::borrow_ptr<TokenFile>;
	using OwnFile    = base::unique_ptr<TokenFile>;
}
