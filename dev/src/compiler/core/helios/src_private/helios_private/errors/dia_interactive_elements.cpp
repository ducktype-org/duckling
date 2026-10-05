// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "dia_interactive_elements.hpp"

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/method.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/function.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/function_decl.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/selector_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/using.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/pst_symbol_data.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <diagnostic/core/diagnostic_arguments.hpp>
#include <diagnostic/placeholder.hpp>
#include <diagnostic/source_position.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>

namespace compiler::helios {
	using namespace dia;

	class IsAliasNote: public MessageBase {
		Metadata getMetadata() const final {
			return {
				.template_type = "message", .type = "note", .family = "lookup", .name = "is_alias"
			};
		}

	public:
		IsAliasNote(std::string alias_name, std::string underlying_name): MessageBase() {
			addArgument<TextArgument>("alias_name", std::move(alias_name));
			addArgument<TextArgument>("underlying_name", std::move(underlying_name));
		}
	};

	class IsAliasCodeNote: public MessageWithCodeFragmentAndCause {
		Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "lookup",
				     .name          = "is_alias_code" };
		}

	public:
		IsAliasCodeNote(dia::StablePosition pos, std::string alias_name, std::string underlying_name):
			  MessageWithCodeFragmentAndCause(pos) {
			addArgument<TextArgument>("alias_name", std::move(alias_name));
			addArgument<TextArgument>("underlying_name", std::move(underlying_name));
		}
	};

	/**
	 * @brief Adds the "... is alias of ..." notes for all the aliases in the chain.
	 * @param ctx Query context.
	 * @param[out] linked_messages The resulting linked messages we should add to the message.
	 * @param elem PST element of the original type identifier in the parse tree.
	 */
	void checkForAliases(
		query::Context&                                    ctx,
		base::HashMap<std::string, Box<dia::MessageBase>>& linked_messages,
		pst::Access<pst::LangElement>                      elem
	) {
		auto ident_opt = elem.dynamicCast<pst::expr::IdentifierLiteral>();
		if (!ident_opt.has_value()) return;
		auto       ident          = ident_opt.value();
		const auto scope          = ctx.query<QueryPrimaryCodeScopeFor>({ ident });
		const auto lookup_qresult = HInterface::ofScopeWithParents(scope).lookup(
			ctx, ident->getName().unlock(ctx)->unwrap()
		);
		if (lookup_qresult->hasFailed()) return;
		CRef<LookupResult> lookup_result = &lookup_qresult->valueOrThrow();

		std::function<void(const LookupResult&, const std::string&)> emit_alias_note =
			[&](const LookupResult& current, const std::string& alias_name) {
				if (current.children.size() != 1) return;

				auto nested = current.children[0];
				if (kind(nested.node) == SymbolKind::Alias) {
					// `using a.b as c;`: the underlying chain is the `a.b` part.
					auto alias_stmt = getSymRef(nested.node)
				                          ->maybePstElement()
				                          .value()
				                          .unlock(ctx)
				                          .dynamicCast<pst::Using>()
				                          .value();
					auto selector = (*alias_stmt->getSelectors().unlock(ctx)->begin()).unlock(ctx);
					std::string underlying_chain;
					for (usize i = 0; i < selector->numberOfNames(); i++) {
						if (i > 0) underlying_chain += ".";
						underlying_chain += selector->getNameByIndex(i).unlock(ctx)->unwrap().str();
					}

					auto id = MessageBase::getUniqueID();
					linked_messages.put(
						id,
						makeBox<IsAliasCodeNote>(
							alias_stmt->getStablePosition(), alias_name, underlying_chain
						)
					);

					emit_alias_note(nested.inner, underlying_chain);
				};
			};

		emit_alias_note(*lookup_result, ident->getName().unlock(ctx)->unwrap().str());
	}

	InteractiveType::InteractiveType(
		query::Context&                               ctx,
		tsh::SymbolType<>                             symbol_type,
		base::Optional<pst::Access<pst::LangElement>> pst_expr
	):
		  symbol_type(symbol_type),
		  pst_expr(std::move(pst_expr)) {
		if (pst_expr.has_value()) {
			this->displayed_name = pst_expr.value()->getSourcePosition().illegalAccess().content(
			);  // Here we should use illegalAccess, maybe serialize the PST
			checkForAliases(ctx, this->linked_messages, pst_expr.value());
		} else
			this->displayed_name = symbol_type.toString();
	}

	Box<dia_args::Component> InteractiveType::getValue(MessageBase& msg) {
		// In the future, we should divide this method into two methods,
		// first to add the linked messages,
		// second to create the link component.

		std::vector<std::string> linked_messages_ids;
		for (auto& [id, linked_msg]: this->linked_messages) {
			msg.addLinkedMessage(id, std::move(linked_msg));
			linked_messages_ids.push_back(id);
		}

		msg.addEntity<TextBasedEntity>(linked_messages_ids, this->displayed_name);
		auto content = makeBox<dia_args::TextComponent>(this->displayed_name);
		auto link
			= makeBox<dia_args::LinkComponent>(std::move(linked_messages_ids), std::move(content));
		return link;
	}

	class FunctionDeclaredHereNote final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "type_check",
				     .name          = "function_declared_here" };
		}

	public:
		FunctionDeclaredHereNote(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	/**
	 * @brief Get source position from a PST element of a function-like character
	 * (pst of a function, function declaration or class method). We want only the
	 * name and parameters to be included in the source position. @TODO: #2521 fix this
	 */
	dia::StablePosition getFunctionLikeSourcePosition(
		query::Context& ctx, pst::Access<pst::LangElement> function_like
	) {
		switch (function_like->getElementKind()) {
		case pst::ElementKind::Fun: {
			auto fun = function_like.dynamicCast<pst::Fun>().value();
			return fun->getParams().unlock(ctx)->getStablePosition();
		}
		case pst::ElementKind::FunDecl: {
			auto fun_decl = function_like.dynamicCast<pst::FunDecl>().value();
			return fun_decl->getParams().unlock(ctx)->getStablePosition();
		}
		case pst::ElementKind::ClassMethod: {
			auto class_method = function_like.dynamicCast<pst::Method>().value();
			return class_method->getParams().unlock(ctx)->getStablePosition();
		}
		default:
			CORE_PANIC(
				"Expected a function, function-like or a method expected while getting function "
				"source position."
			);
		}
	}

	InteractiveFunction::InteractiveFunction(
		query::Context&                               ctx,
		SymID                                         function_symbol,
		base::Optional<pst::Access<pst::LangElement>> pst_expr
	):
		  function_symbol(function_symbol),
		  pst_expr(std::move(pst_expr)) {
		if (getSymRef(function_symbol)->isPstImplemented()) {
			auto position = getFunctionLikeSourcePosition(
				ctx, getSymRef(function_symbol)->maybePstElement().value().unlock(ctx)
			);
			auto id = MessageBase::getUniqueID();
			this->linked_messages.put(std::move(id), makeBox<FunctionDeclaredHereNote>(position));
		}
		this->displayed_name = name(function_symbol).str();
	}

	Box<dia_args::Component> InteractiveFunction::getValue(MessageBase& msg) {
		std::vector<std::string> linked_messages_ids;
		for (auto& [id, linked_msg]: this->linked_messages) {
			msg.addAttachedMessage(id, std::move(linked_msg));
			linked_messages_ids.push_back(id);
		}

		msg.addEntity<TextBasedEntity>(linked_messages_ids, this->displayed_name);

		auto content = makeBox<dia_args::TextComponent>(this->displayed_name);
		auto link
			= makeBox<dia_args::LinkComponent>(std::move(linked_messages_ids), std::move(content));
		return link;
	}

}
