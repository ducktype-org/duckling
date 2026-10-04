#include "repl_symbols.hpp"

#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/hout/elements.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios_private/scopes/scopes.hpp>

#include <base/except/exceptions.hpp>

#include <algorithm>
#include <ranges>
#include <sstream>
#include <string_view>

namespace compiler::repl {
	namespace {
		/**
		 * @brief Convert a compiler symbol kind into the short label used in REPL output.
		 */
		std::string symbolKindLabel(helios::SymbolKind kind) {
			switch (kind) {
			case helios::SymbolKind::Namespace:
				return "namespace";
			case helios::SymbolKind::Function:
				return "function";
			case helios::SymbolKind::FunctionDeclaration:
				return "function-decl";
			case helios::SymbolKind::Const:
				return "const";
			case helios::SymbolKind::Class:
				return "class";
			case helios::SymbolKind::Alias:
				return "alias";
			case helios::SymbolKind::Using:
				return "using";
			case helios::SymbolKind::Variable:
				return "variable";
			case helios::SymbolKind::Import:
				return "import";
			case helios::SymbolKind::Parameter:
				return "parameter";
			case helios::SymbolKind::NamedCodeElement:
				return "code";
			case helios::SymbolKind::Method:
				return "method";
			case helios::SymbolKind::Field:
				return "field";
			case helios::SymbolKind::Constructor:
				return "constructor";
			case helios::SymbolKind::Destructor:
				return "destructor";
			case helios::SymbolKind::Template:
				return "template";
			case helios::SymbolKind::COUNT:
				break;
			}
			return "symbol";
		}

		/**
		 * @brief Collect the REPL module chain from oldest statement to the terminal module.
		 */
		std::vector<frontend::ModuleID> collectReplModuleChain(
			query::Context& ctx, frontend::ModuleID terminal_module_id
		) {
			std::vector<frontend::ModuleID> modules;
			auto                            current = terminal_module_id;

			while (true) {
				modules.emplace_back(current);
				auto parent = ctx.query<frontend::QueryReplModuleParent>(current);
				if (!parent.has_value()) break;
				current = parent.value();
			}

			std::ranges::reverse(modules);
			return modules;
		}

		/**
		 * @brief Format function parameters and return type for a function-like symbol.
		 */
		std::string queryFunctionDetails(query::Context& ctx, helios::SymID symbol) {
			auto declaration = ctx.query<helios::QueryDeclOfFun>(symbol);
			if (declaration->hasFailed()) return {};
			const auto& declaration_value = declaration->valueOrPanic();

			std::stringstream out;
			out << "(";
			bool first = true;
			for (const auto& param: declaration_value.parameters) {
				if (helios::kind(symbol) == helios::SymbolKind::Method
				    && param.name == base::StrID("self"))
					continue;
				if (!first) out << ", ";
				out << param.name.strView() << ": " << param.type.toString();
				first = false;
			}
			out << ") -> " << declaration_value.return_type.toString();
			return out.str();
		}

		/**
		 * @brief Query and format the type of a value-like symbol.
		 */
		std::string queryTypeDetails(query::Context& ctx, helios::SymID symbol) {
			auto type = ctx.query<helios::QueryTypeOfSymbol>(symbol);
			if (type->hasFailed()) return {};
			return type->valueOrPanic().toString();
		}

		/**
		 * @brief Return the compact signature/type suffix for symbols that have one.
		 */
		std::string querySymbolDetails(query::Context& ctx, helios::SymID symbol) {
			const auto kind = helios::kind(symbol);
			if (isFunctionLike(kind)) return queryFunctionDetails(ctx, symbol);

			switch (kind) {
			case helios::SymbolKind::Const:
			case helios::SymbolKind::Variable:
			case helios::SymbolKind::Parameter:
			case helios::SymbolKind::Field:
				return queryTypeDetails(ctx, symbol);
			default:
				return {};
			}
		}

		/**
		 * @brief Format one-line symbol output shared by lists and nested detail sections.
		 */
		std::string formatSymbolSummary(query::Context& ctx, helios::SymID symbol) {
			const auto sym_kind = helios::kind(symbol);

			std::stringstream out;
			out << symbolKindLabel(sym_kind) << " " << helios::name(symbol).strView();

			const auto details = querySymbolDetails(ctx, symbol);
			if (!details.empty()) {
				if (details.front() == '(')
					out << details;
				else
					out << " : " << details;
			}

			return out.str();
		}

		/**
		 * @brief Print a titled member list, using `none` for empty sections.
		 */
		void printMemberSection(
			query::Context&                   ctx,
			std::stringstream&                out,
			std::string_view                  title,
			const std::vector<helios::SymID>& symbols
		) {
			out << title << ":\n";
			if (symbols.empty()) {
				out << "  none\n";
				return;
			}

			for (auto symbol: symbols) out << "  - " << formatSymbolSummary(ctx, symbol) << "\n";
		}

		/**
		 * @brief Collect the symbols of the interface elements the given view yields.
		 * @note Taken by a forwarding reference, because a filtered view is not const-iterable.
		 */
		std::vector<helios::SymID> symbolsOf(auto&& elements) {
			return elements | std::views::transform([](const tsh::InterfaceElement& element) {
					   return element.getSymbol();
				   })
			     | std::ranges::to<std::vector>();
		}

		/**
		 * @brief Whether the interface element constructs a value of the type, rather than being
		 * a method called on one.
		 */
		bool isConstructorLike(const tsh::InterfaceElement& element) {
			using enum tsh::MemberSpecialKind;
			switch (element.specialKind()) {
			case Constructor:
			case ParameterlessConstructor:
			case CopyConstructor:
				return true;
			default:
				return false;
			}
		}

		/**
		 * @brief The constructors the class declares itself.
		 *
		 * The compiler-generated ones are left out, as they are reported separately as the
		 * implicit constructors, and they are told apart by having no PST element of their own.
		 */
		std::vector<helios::SymID> declaredConstructorSymbols(const tsh::TypeInterface& interface) {
			return symbolsOf(
				interface.getAnyMethodsView()
				| std::views::filter([](const tsh::InterfaceElement& element) {
					  return isConstructorLike(element)
				         and helios::maybeSymbolPst(element.getSymbol()).has_value();
				  })
			);
		}

		/**
		 * @brief Return symbols declared directly inside a namespace body.
		 */
		std::vector<helios::SymID> queryNamespaceMembers(query::Context& ctx, helios::SymID symbol) {
			auto stmt = helios::stmt(ctx, symbol);
			if (!stmt.has_value()) return {};

			auto scope = helios::queryBodyCodeScopeFor(ctx, stmt.value());
			return ctx.query<helios::QuerySymbolsInScope>(scope)->valueOrThrow();
		}

		/**
		 * @brief Print the field-based constructor available for class values in the REPL.
		 */
		void printImplicitFieldConstructor(
			query::Context& ctx, std::stringstream& out, const helios::ClassSymbolData& class_data
		) {
			out << "implicit constructors:\n";
			auto fields = class_data.declared_interface.getFieldsView();
			if (fields.begin() == fields.end()) {
				out << "  none\n";
				return;
			}

			out << "  - " << class_data.name.strView() << "(";
			bool first = true;
			for (const auto& field: fields) {
				if (!first) out << ", ";
				out << helios::name(field.getSymbol()).strView();
				const auto type = querySymbolDetails(ctx, field.getSymbol());
				if (!type.empty()) out << ": " << type;
				first = false;
			}
			out << ")\n";
		}

		/**
		 * @brief Print class-specific details such as fields, methods, etc.
		 */
		void printClassDetails(query::Context& ctx, std::stringstream& out, helios::SymID symbol) {
			auto class_data_result = ctx.query<helios::QueryClassSymbolData>(symbol);
			if (class_data_result->hasFailed()) {
				out << "members: unavailable\n";
				return;
			}

			const auto& class_data = class_data_result->valueOrPanic();
			if (class_data.base.has_value()) out << "base: " << class_data.base->toString() << "\n";

			if (!class_data.implements.empty()) {
				out << "implements:\n";
				for (const auto& interface: class_data.implements)
					out << "  - " << interface.toString() << "\n";
			}

			const auto& interface = class_data.declared_interface;

			printMemberSection(ctx, out, "fields", symbolsOf(interface.getAnyFieldsView()));
			printMemberSection(
				ctx, out, "declared constructors", declaredConstructorSymbols(interface)
			);
			printImplicitFieldConstructor(ctx, out, class_data);
			printMemberSection(
				ctx,
				out,
				"methods",
				symbolsOf(
					interface.getAnyMethodsView()
					| std::views::filter([](const tsh::InterfaceElement& element) {
						  return not isConstructorLike(element);
					  })
				)
			);

			out << "declared destructor:\n";
			const auto destructor
				= interface.getSpecialElement(tsh::MemberSpecialKind::UserDestructor);
			if (destructor.has_value())
				out << "  - " << formatSymbolSummary(ctx, destructor.value()->getSymbol()) << "\n";
			else
				out << "  none\n";
		}

		/**
		 * @brief Print namespace-specific details.
		 */
		void printNamespaceDetails(
			query::Context& ctx, std::stringstream& out, helios::SymID symbol
		) {
			printMemberSection(ctx, out, "members", queryNamespaceMembers(ctx, symbol));
		}
	}

	std::vector<ReplVisibleSymbol> queryVisibleReplSymbols(
		query::Context& ctx, frontend::ModuleID terminal_module_id
	) {
		CORE_ASSERT(
			frontend::getModuleRef(terminal_module_id)->isReplModule(),
			"queryVisibleReplSymbols expects a REPL module"
		);

		std::vector<ReplVisibleSymbol> output;

		for (auto module_id: collectReplModuleChain(ctx, terminal_module_id)) {
			auto scope   = helios::queryRootScopeOfMainModuleFile(ctx, module_id);
			auto symbols = ctx.query<helios::QuerySymbolsInScope>(scope)->valueOrThrow();

			for (auto symbol: symbols) {
				auto sym_kind = helios::kind(symbol);
				output.emplace_back(ReplVisibleSymbol{
					.symbol     = symbol,
					.module_id  = module_id,
					.kind       = sym_kind,
					.kind_label = symbolKindLabel(sym_kind),
					.name       = std::string(helios::name(symbol).strView()),
					.details    = querySymbolDetails(ctx, symbol),
				});
			}
		}

		return output;
	}

	std::string formatReplSymbolDetails(query::Context& ctx, helios::SymID symbol) {
		std::stringstream out;
		const auto        sym_kind = helios::kind(symbol);

		out << formatSymbolSummary(ctx, symbol) << "\n";
		out << "kind: " << symbolKindLabel(sym_kind) << "\n";
		out << "qualified name: " << helios::prettyDebugPrint(symbol, ctx) << "\n";

		switch (sym_kind) {
		case helios::SymbolKind::Class:
			printClassDetails(ctx, out, symbol);
			break;
		case helios::SymbolKind::Namespace:
			printNamespaceDetails(ctx, out, symbol);
			break;
		default:
			break;
		}

		return out.str();
	}
}  // namespace compiler::repl
