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
		current_context = std::move(fallback_stack.back().saved_context);
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
		current_context = std::move(fallback_stack.back().saved_context);
		fallback_stack.pop_back();
		tokens().skip(base::safeIntConv<i64>(fwd));
		skip_till_fallback = false;
	}

	void LangParserState::setFallback(u64 length) {
		auto new_stream = ctokens().getSubstream(length);
		fallback_stack.emplace_back(Fallback{ .type          = NonRecursive,
		                                      .saved_stream  = std::move(current_stream),
		                                      .saved_context = std::move(current_context),
		                                      .post_jump     = length });
		current_stream = makeBox<TokenStream>(std::move(new_stream));
		variant_match(fallback_stack.back().saved_context) {
			variant_case(CRef<tpc::ParserContext>, ctx_ref) {
				current_context = ctx_ref;
			}
			variant_case(Box<tpc::ParserContext>, ctx_ref) {
				current_context = ctx_ref.ref();
			}
		}
	}

	void LangParserState::exitFallback() {
		CORE_ASSERT(
			fallback_stack.size() && fallback_stack.back().type == SubStreamType::NonRecursive,
			"No fallback token stream to go up from"
		);
		checkAllParsed();
		u64 fwd        = fallback_stack.back().post_jump;
		current_stream = std::move(fallback_stack.back().saved_stream);
		current_context = std::move(fallback_stack.back().saved_context);
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

	CRef<LangParserContext> LangParserState::getContext() const  {
		variant_match(current_context) {
			variant_case(CRef<tpc::ParserContext>, ref) {
				return {dynamic_cast<const LangParserContext*>(&*ref)};
			}
			variant_case(Box<tpc::ParserContext>, box) {
				return {dynamic_cast<const LangParserContext*>(&*box)};
			}
		}
		CORE_UNREACHABLE();
	}

	void LangParserState::copyOwnContext() {
		if (std::holds_alternative<CRef<tpc::ParserContext>>(current_context)) {
			current_context = std::get<CRef<tpc::ParserContext>>(current_context)->copy();
		}
	} 

	void LangParserState::setContextClassName(base::StrID name) {
		if (name == getContext()->class_name) return;
		copyOwnContext();
		dynamic_cast<LangParserContext*>(&*std::get<Box<tpc::ParserContext>>(current_context))->class_name = name;
	}
	
	void LangParserState::setConstextBlockOrdering(BlockOrderType type) {
		if (type == getContext()->block_order) return;
		copyOwnContext();
		dynamic_cast<LangParserContext*>(&*std::get<Box<tpc::ParserContext>>(current_context))->block_order = type;
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
