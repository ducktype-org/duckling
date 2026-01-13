#include "lang_parser_state.hpp"

#include "utility.hpp"

namespace pst {
	bool LangParserState::isSkipping() const { return skip_till_fallback; }

	void LangParserState::addImport(const ImportType& import) { imports.push_back(import); }

	void LangParserState::goUp() {
		CORE_ASSERT(
			fallback_stack.size() && fallback_stack.back().type == SubStreamType::Recursive,
			"No recursive token stream to go up from"
		);
		checkAllParsed();
		current_stream = std::move(fallback_stack.back().saved_stream);
		fallback_stack.pop_back();
		skip_till_fallback = false;
	}

	void LangParserState::goUpAndSkip() {
		CORE_ASSERT(
			fallback_stack.size() && fallback_stack.back().type == SubStreamType::Recursive,
			"No recursive token stream to go up from"
		);
		checkAllParsed();
		u64 fwd        = fallback_stack.back().post_jump;
		current_stream = std::move(fallback_stack.back().saved_stream);
		fallback_stack.pop_back();
		tokens().skip(base::safeIntConv<i64>(fwd));
		skip_till_fallback = false;
	}

	void LangParserState::setFallback(u64 length) {
		auto new_stream = ctokens().getSubstream(length);
		fallback_stack.emplace_back(Fallback{
			.type = NonRecursive, .saved_stream = std::move(current_stream), .post_jump = length });
		current_stream = makeBox<TokenStream>(std::move(new_stream));
	}

	void LangParserState::exitFallback() {
		CORE_ASSERT(
			fallback_stack.size() && fallback_stack.back().type == SubStreamType::NonRecursive,
			"No fallback token stream to go up from"
		);
		checkAllParsed();
		u64 fwd        = fallback_stack.back().post_jump;
		current_stream = std::move(fallback_stack.back().saved_stream);
		fallback_stack.pop_back();
		tokens().skip(base::safeIntConv<i64>(fwd));
		skip_till_fallback = false;
	}

	void LangParserState::finalize() {
		finalized = true;
		checkAllParsed();
	}

	bool LangParserState::isFinalized() const { return finalized; }

	class NotAllParsedError;

	void LangParserState::checkAllParsed() {
		if (!isSkipping() && !empty()) {
			logInt(base::makeBox<NotAllParsedError>(dia::SourcePosition{
				getPosition(),
				ctokens()[base::safeIntConv<i64>(ctokens().size()) - 1].getPosition().getEnd() }));
		}
	}

	class NotAllParsedError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "not_all_tokens_parsed_error" };
		}

	public:
		NotAllParsedError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

}
