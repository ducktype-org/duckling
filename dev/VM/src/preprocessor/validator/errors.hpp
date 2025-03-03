#include <base/str_utils.hpp>
#include <base/string_id.hpp>
#include <diagnostic/message.hpp>

namespace vm::validator {
	class NoMainError final: public dia::Error {
	public:
		constexpr static std::string_view ERR_MSG
			= "Provided program does not have `main` function.";

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return ERR_MSG.data();
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::StaticVerification;
		}

		NoMainError(dia::SourcePosition pos): dia::Error(pos) {}
	};
};
