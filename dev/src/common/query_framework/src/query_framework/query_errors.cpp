#include "query_errors.hpp"

#include <query_framework/internal/query_errors.hpp>

namespace query {
	void throwFailed(std::string_view reason) { throw internal::QueryFailedException(reason); }
}
