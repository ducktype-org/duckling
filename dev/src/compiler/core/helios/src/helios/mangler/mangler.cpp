#include "mangler.hpp"

#include <concurrent/base/collections/hash_map.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/element_kind.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/scope_id.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_abi.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <tsh/types.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <hashing/hash.hpp>
#include <logger/logger.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <algorithm>
#include <string_view>

/**
 * This is the implementation of the mangling scheme according to mangling-scheme.md
 * If any changes are made to this file, please also update the mangling-scheme.md
 * That file provides a detailed description and motivation for some design choices made here
 */
namespace compiler::helios::mangler {

	void addToHash(hashing::hash_algorithm auto& h, const KeyOf_MangledSymbol& k) RELEASE_NOEXCEPT {
		addToHash(h, k.symbol_key.index());
		if (k.symbol_key.index() == 0)
			addToHash(h, std::get<0>(k.symbol_key));
		else if (k.symbol_key.index() == 1)
			addToHash(h, std::get<1>(k.symbol_key));
		else
			CORE_PANIC("KeyOf_MangledSymbol has an unexpected symbol_key index");

		addToHash(h, k.kind);
		addToHash(h, k.mangling_scheme_version);
		addToHash(h, k.additional_metadata.has_value());
		if (k.additional_metadata) addToHash(h, k.additional_metadata.value());
	}

	base::Bit256 KeyOf_MangledSymbol::queryUnstablePerfectHash() const {
		return hashing::justHash<hashing::SHA256>(*this);
	}

	namespace internal {
		/**
		 * @brief Check if the symbol should be mangled in the first place.
		 * @note: See mangling-scheme.md for details
		 */
		bool shouldMangle(query::Context& ctx, const auto& key) {
			if (key.kind != ManglingSymbolKind::Standard) {
				// Non-standard symbols can't have C mangling
				return true;
			}

			const auto sym_id = std::get<SymID>(key.symbol_key);
			if (auto abi = ctx.query<QuerySymbolABI>(sym_id); abi->hasValue()) {
				variant_match(abi->valueOrThrow()) {
					variant_case_novalue(CAbi) { return false; }
					variant_case_novalue(DefaultAbi) { return true; }
					variant_default { CORE_UNREACHABLE(); }
				}
			}

			return true;
		}

		/**
		 * @brief A shorter representation of a number in base-62, used to save space
		 * @note: See mangling-scheme.md for details
		 */
		std::string compactNumber(u64 number) {
			using namespace std::literals::string_view_literals;

			if (number == 0) return "_";

			static constexpr auto DIGITS
				= "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"sv;
			constexpr u64 BASE = DIGITS.size();

			std::string ret;
			--number;
			do {
				ret += DIGITS[number % BASE];
				number /= BASE;
			} while (number > 0);
			std::ranges::reverse(ret);
			ret += '_';
			return ret;
		}

		/**
		 * @brief Returns bare identifier of the symbol prefixed with its size
		 * If the name contains characters outside of the allowed set,
		 * it will be prefixed with 'U' and punnycode-encoded.
		 * @note: See mangling-scheme.md for details
		 */
		std::string identifier(std::string name) {
			// @future: use punnycode for unicode strings
			if (/* hasCharsOnlyFromAllowedCharacterSet */ true) {
				name = std::to_string(name.size()) + name;
				return name;
			} else {
				constexpr char UNICODE_PREFIX = 'U';
				std::string    punny_string   = name;  // @future: convert to punnycode
				return base::strConcat(UNICODE_PREFIX, punny_string.size(), punny_string);
			}
		}

		/**
		 * @brief Returns package/module/script prefix for the symbol
		 * @note: See mangling-scheme.md for details
		 */
		std::string pathPrefix(SymID symbol_id) {
			auto enclosing_scope  = scope(symbol_id);
			auto enclosing_module = module(enclosing_scope);

			// note: only the enclosing module is used for mangling. This is intentional,
			// as modules are supposed to be self-contained and this would make moving them
			// a more breaking (ABI-wise) change than it should be

			// "M" <module-name>                                 // standalone module
			// @future: templated modules
			if (/* standalone module */ true) {
				auto module_identifier = identifier(frontend::moduleName(enclosing_module).str());
				return base::strConcat("M", module_identifier);
			}

			// @future: add support for packages & scripts when they are implemented
			// "P" <package-name> <module-name>                  // module in a package
			// "S" <script-name>                                 // standalone script
			// "R" <package-name> <module-name> <script-name>    // script in a package

			// @todo: backreference -- this will be added in the next PR
		}

		std::string pathPrefix(special_symbol_keys::LIRModuleID mod_id) {
			// "M" <module-name>                                 // standalone module
			return base::strConcat("M", mod_id.id);

			// @future: add support for packages & scripts when they are implemented
		}

		/**
		 * @brief Returns the bare name of the symbol prefixed with its size
		 * @note: See mangling-scheme.md for details
		 */
		std::string unscopedName(SymID symbol_id) {
			return identifier(compiler::helios::name(symbol_id).str());
		}

		/**
		 * @brief Returns symbol name prefixed with all enclosing it scopes to uniquely identify it
		 * @note: See mangling-scheme.md for details
		 *
		 * @todo: This is still a little simplified, there should probably be at least an additional
		 * layer for things like macros and there will probably be other elements that create scopes.
		 */
		std::string symbolName(query::Context& ctx, SymID symbol_id) {
			auto scope_id = scope(symbol_id);

			if (scopeDepth(scope_id) == 1) {
				return "G" + unscopedName(symbol_id);
			} else {
				std::vector<std::string> path_parts;

				auto current_pst = symbolPst(symbol_id).value().unlock(ctx);
				while (true) {
					auto ancestor     = current_pst;
					auto ancestor_opt = ancestor->getParent();

					while (ancestor_opt) {
						ancestor = ancestor_opt.value().unlock(ctx);

						if (ancestor->getElementKind() == pst::ElementKind::Namespace) {
							auto nmsp = ancestor.dynamicCast<pst::Namespace>().value();
							path_parts.push_back(identifier(nmsp->getName().str()));
							current_pst = pst::Access<pst::LangElement>(ancestor);
							break;
						}
						if (ancestor->getElementKind() == pst::ElementKind::Class) {
							auto nmsp = ancestor.dynamicCast<pst::Class>().value();
							path_parts.push_back(identifier(nmsp->getName().str()));
							current_pst = pst::Access<pst::LangElement>(ancestor);
							break;
						}

						ancestor_opt = ancestor->getParent();
					}

					if (!ancestor_opt) break;
				}

				std::string ret = "N";
				for (auto&& it = path_parts.rbegin(); it != path_parts.rend(); ++it) ret += *it;

				ret += unscopedName(symbol_id);

				return ret + "E";
			}
		}

		/**
		 * @brief Returns the symbol's 'path' i.e. in which package/module/script it is defined
		 * and all its enclosing scopes (namespaces, classes, functions, etc.)
		 * @note: See mangling-scheme.md for details
		 */
		std::string path(query::Context& ctx, SymID symbol_id) {
			return base::strConcat(pathPrefix(symbol_id), symbolName(ctx, symbol_id));
		}

		/**
		 * @brief Returns mangled name of a function or method
		 * @note: See mangling-scheme.md for details
		 */
		std::string func(query::Context& ctx, SymID symbol_id) {
			std::stringstream ret;
			if (kind(symbol_id) == SymbolKind::Function
			    or kind(symbol_id) == SymbolKind::FunctionDeclaration
			    or kind(symbol_id) == SymbolKind::Method) {
				// @TODO: #2255 Function qualifiers?

				auto function_type = ctx.query<QueryTypeOfSymbol>(symbol_id)->valueOrThrow();
				ret << ctx.query<QueryMangledType>({ function_type })->valueOrThrow().str();

				const auto& fun_decl
					= ctx.query<compiler::helios::QueryDeclOfFun>(symbol_id).get()->valueOrPanic();

				for (const auto& param: fun_decl.parameters) ret << identifier(param.name.str());

				ret << "E";
			} else {
				CORE_USER_LOG("Tried to mangle non function-like symbol as a function-like.");
				CORE_UNREACHABLE();
			}

			return ret.str();
		}

		/**
		 * @brief Determines what type of symbol we are mangling to choose the right encoding
		 * @note: See mangling-scheme.md for details
		 */
		std::string symbolEncoding(query::Context& ctx, const SymID& symbol_id) {
			switch (kind(symbol_id)) {
			case SymbolKind::Variable:
			case SymbolKind::Field:
			case SymbolKind::Const:
				return path(ctx, symbol_id);
				break;

			case SymbolKind::Function:
			case SymbolKind::Method:
			case SymbolKind::FunctionDeclaration: {
				variant_match(getSymRef(symbol_id)->other) {
					variant_case_novalue(PstSymbolData) {
						// If the symbol originates from the PST, use its path.
						return path(ctx, symbol_id) + func(ctx, symbol_id);
					}
					variant_case_novalue(builtin::BuiltinFunctionData) {
						// Builtins have a C linkage (CAbi), so they are handled by the
						// `shouldMangle` check in `provide()`
						CORE_UNREACHABLE();
					}
					variant_case(defgen::GeneratedSymbolData, gen_data) {
						// If the symbol is generated, it has no path.
						variant_match(gen_data.data) {
							variant_case(defgen::GeneratedSymbolData::ImplicitConstructor, ctor) {
								const auto mangled_class = ctx.query<QueryMangledType>(
									tsh::SymbolType<>::withDefaults(ctor.class_type)
								);
								const auto ctor_suffix = "Hic" + func(ctx, symbol_id) + "E";
								return mangled_class->valueOrThrow().str() + ctor_suffix;
							}
							variant_case(
								defgen::GeneratedSymbolData::DefaultClassConstructor, ctor
							) {
								const auto path_to_class = path(ctx, ctor.class_symbol);
								const auto ctor_suffix   = "Hdc" + func(ctx, symbol_id) + "E";
								return path_to_class + ctor_suffix;
							}
							variant_case(
								defgen::GeneratedSymbolData::DefaultStaticArrayConstructor, ctor
							) {
								return "Hds"
								     + ctx.query<QueryMangledType>(
											  tsh::SymbolType<>::withDefaults(ctor.array_type)
									 )
								           ->valueOrThrow()
								           .str()
								     + "E";
							}
							variant_case(
								defgen::GeneratedSymbolData::ReplExpressionWrapper, repl_wrapper
							) {
								return base::strConcat("__repl_expr_wrapper_", repl_wrapper.counter);
							}
							variant_case(
								defgen::GeneratedSymbolData::ReplInstructionWrapper,
								repl_instr_wrapper
							) {
								return base::strConcat(
									"__repl_instr_wrapper_", repl_instr_wrapper.counter
								);
							}
							// Other cases of generated symbols cannot be functions.
						}
					}
				}
				CORE_UNREACHABLE();
			}
			case SymbolKind::Class: {
				return "C" + path(ctx, symbol_id);
			}
			default:
				throw base::LogicError{
					base::strConcat("Cannot mangle symbol of type: ", kind(symbol_id))
				};
				break;
			}
		}

		template<ManglingSymbolKind Kind, class SpecialSymbolKey>
		std::string specialSymbolEncoding(query::Context& ctx, SpecialSymbolKey key);

		template<>
		std::string specialSymbolEncoding<
			ManglingSymbolKind::ModuleConstructor,
			special_symbol_keys::LIRModuleID>(
			query::Context&, special_symbol_keys::LIRModuleID module_id
		) {
			// <encoding> ::= <path>
			// <path> ::= <path-prefix> <symbol-name>
			auto path_prefix = pathPrefix(module_id);

			// <symbol-name> ::= "G" <unscoped-name>
			// <unscoped-name> ::= <special-symbol-encoding>
			// <special-symbol-encoding> ::= "H" <special-symbol-name> "E"
			// <special-symbol-name> ::= "mc"
			auto symbol_name = "GHmcE";

			return base::strConcat(path_prefix, symbol_name);
		}

		template<>
		std::string specialSymbolEncoding<
			ManglingSymbolKind::ModuleDestructor,
			special_symbol_keys::LIRModuleID>(
			query::Context&, special_symbol_keys::LIRModuleID module_id
		) {
			auto path_prefix = pathPrefix(module_id);
			return base::strConcat(path_prefix, "GHmdE");
		}

		template<>
		std::string specialSymbolEncoding<ManglingSymbolKind::GlobalVariableConstructor>(
			query::Context& ctx, SymID symbol_id
		) {
			return symbolEncoding(ctx, symbol_id) + "gc";
		}

		template<>
		std::string specialSymbolEncoding<ManglingSymbolKind::GlobalVariableDestructor>(
			query::Context& ctx, SymID symbol_id
		) {
			return symbolEncoding(ctx, symbol_id) + "gd";
		}

		/**
		 * @brief Get the encoding of a symbol
		 * @note: See mangling-scheme.md for details
		 */
		std::string encoding(query::Context& ctx, const auto& key) {
			switch (key.kind) {
			case ManglingSymbolKind::Standard:
				return internal::symbolEncoding(ctx, std::get<SymID>(key.symbol_key));
				break;

			case ManglingSymbolKind::ModuleConstructor:
				return internal::specialSymbolEncoding<ManglingSymbolKind::ModuleConstructor>(
					ctx, std::get<special_symbol_keys::LIRModuleID>(key.symbol_key)
				);
				break;

			case ManglingSymbolKind::ModuleDestructor:
				return internal::specialSymbolEncoding<ManglingSymbolKind::ModuleDestructor>(
					ctx, std::get<special_symbol_keys::LIRModuleID>(key.symbol_key)
				);
				break;

			case ManglingSymbolKind::GlobalVariableConstructor:
				return internal::specialSymbolEncoding<ManglingSymbolKind::GlobalVariableConstructor>(
					ctx, std::get<SymID>(key.symbol_key)
				);
				break;

			case ManglingSymbolKind::GlobalVariableDestructor:
				return internal::specialSymbolEncoding<ManglingSymbolKind::GlobalVariableDestructor>(
					ctx, std::get<SymID>(key.symbol_key)
				);
				break;

			default:
				CORE_UNREACHABLE();
			}
		}

		/**
		 * @brief Returns formatted metadata that will be added to the mangled name
		 * @note: See mangling-scheme.md for details
		 */
		std::string optMetadata(base::Optional<std::string> metadata) {
			if (!metadata.has_value()) return "";
			return "$" + metadata.value();
		}

	}  // namespace internal

	struct IMPLEMENT_QUERY(QueryMangledSymbol, base::StrID) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			using namespace std::literals::string_view_literals;

			if (not internal::shouldMangle(ctx, key)) return name(std::get<SymID>(key.symbol_key));

			// note: global identifiers starting with underscore and a capital letter are
			// reserved in C. Q seems to be free and stands for both query and quack
			constexpr auto LANGUAGE_PREFIX = "_Q"sv;

			const auto mangling_scheme_version
				= internal::compactNumber(key.mangling_scheme_version);

			const std::string encoding = internal::encoding(ctx, key);

			const std::string metadata = internal::optMetadata(key.additional_metadata);

			std::string mangled_name
				= base::strConcat(LANGUAGE_PREFIX, mangling_scheme_version, encoding, metadata);

			return base::StrID{ mangled_name.c_str() };
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMangledSymbol);

	struct IMPLEMENT_QUERY(QueryMangledType, query::QResult<base::StrID>) {
		static std::string mangle(query::Context&, tsh::UnitAbstractType) { return "u"; }

		static std::string mangle(query::Context&, tsh::VoidAbstractType) { return "v"; }

		static std::string mangle(query::Context&, tsh::ByteAbstractType) { return "y"; }

		static std::string mangle(query::Context&, tsh::BoolAbstractType) { return "b"; }

		static std::string mangle(query::Context&, tsh::CharAbstractType) { return "c"; }

		static std::string mangle(query::Context&, tsh::IntegralAbstractType type) {
			if (type.getSignedness() == tsh::IntegralAbstractType::Signedness::Signed)
				return base::strConcat("i", type.getSize().asInt());
			else
				return base::strConcat("j", type.getSize().asInt());
		}

		static std::string mangle(query::Context&, tsh::FloatAbstractType type) {
			return base::strConcat("f", type.getSize().asInt());
		}

		static std::string mangle(query::Context&, tsh::RawPointerAbstractType) { return "p"; }

		static std::string mangle(query::Context& ctx, tsh::PointerAbstractType type) {
			return base::strConcat(
				"P", ctx.query<QueryMangledType>({ type.getPointee() })->valueOrThrow().str(), "E"
			);
		}

		static std::string mangle(query::Context&, tsh::StringAbstractType) { return "s"; }

		static std::string mangle(query::Context& ctx, tsh::FunctionAbstractType type) {
			std::stringstream res;
			res << "F"
				<< ctx.query<QueryMangledType>({ type.getResultType() })->valueOrThrow().str();
			// @TODO: #2255 Function qualifiers?
			for (auto& param: type.getParameterTypes())
				res << ctx.query<QueryMangledType>({ param })->valueOrThrow().str();
			res << "E";

			return res.str();
		}

		static std::string mangle(query::Context& ctx, tsh::DynamicArrayAbstractType type) {
			return base::strConcat(
				"D",
				ctx.query<QueryMangledType>({ type.getElementType() })->valueOrThrow().str(),
				"E"
			);
		}

		static std::string mangle(query::Context& ctx, tsh::StaticArrayAbstractType type) {
			return base::strConcat(
				"A",
				base::toString(type.getSize()),
				ctx.query<QueryMangledType>({ type.getElementType() })->valueOrThrow().str(),
				"E"
			);
		}

		static std::string mangle(query::Context& ctx, tsh::TupleAbstractType type) {
			std::stringstream res;
			res << "T";
			for (auto& elem: type.getComponents())
				res << ctx.query<QueryMangledType>({ elem })->valueOrThrow().str();
			res << "E";

			return res.str();
		}

		static std::string mangle(query::Context& ctx, tsh::VariantAbstractType type) {
			std::stringstream res;
			res << "V";
			for (auto& elem: type.getUnderlyingTypes())
				res << ctx.query<QueryMangledType>({ elem })->valueOrThrow().str();
			res << "E";

			return res.str();
		}

		static std::string mangle(query::Context& ctx, tsh::ClassAbstractType type) {
			return getSimpleMangledName(ctx, type.getSymbol()).str();
		}

		static std::string mangle(query::Context&, tsh::MetaAbstractType) { return "t"; }

		static query::QResult<std::string> mangle(query::Context& ctx, tsh::AbstractType type) {
			using enum tsh::Kind;
			switch (type.getKind()) {
			case Unit:
				return mangle(ctx, type.as<tsh::UnitAbstractType>());
			case Void:
				return mangle(ctx, type.as<tsh::VoidAbstractType>());
			case Byte:
				return mangle(ctx, type.as<tsh::ByteAbstractType>());
			case Bool:
				return mangle(ctx, type.as<tsh::BoolAbstractType>());
			case Char:
				return mangle(ctx, type.as<tsh::CharAbstractType>());
			case Integral:
				return mangle(ctx, type.as<tsh::IntegralAbstractType>());
			case Float:
				return mangle(ctx, type.as<tsh::FloatAbstractType>());
			case RawPointer:
				return mangle(ctx, type.as<tsh::RawPointerAbstractType>());
			case Pointer:
				return mangle(ctx, type.as<tsh::PointerAbstractType>());
			case String:
				return mangle(ctx, type.as<tsh::StringAbstractType>());
			case Function:
				return mangle(ctx, type.as<tsh::FunctionAbstractType>());
			case DynamicArray:
				return mangle(ctx, type.as<tsh::DynamicArrayAbstractType>());
			case StaticArray:
				return mangle(ctx, type.as<tsh::StaticArrayAbstractType>());
			case Tuple:
				return mangle(ctx, type.as<tsh::TupleAbstractType>());
			case Variant:
				return mangle(ctx, type.as<tsh::VariantAbstractType>());
			case Class:
				return mangle(ctx, type.as<tsh::ClassAbstractType>());
			case Meta:
				return mangle(ctx, type.as<tsh::MetaAbstractType>());
			default:
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat("Cannot mangle type of kind: ", type.getKind()), std::nullopt
				));
				return query::Failed();
			}
		}

		static auto provide(Context& ctx, const QKey& key) -> PResult {
			return base::StrID{ base::strConcat(
				key.getUniqueness() == tsh::Uniqueness::Unique ? "M" : "",
				key.getLeakage() == tsh::Leakage::Leaking ? "L" : "",
				key.getMutability() == tsh::Mutability::Mutable ? "" : "N",
				key.getRefKind() == tsh::ReferenceKind::Direct ? ""
				: key.getRefKind() == tsh::ReferenceKind::Box  ? "X"
															   : "R",
				mangle(ctx, key.getType()).valueOrThrow()
			) };
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMangledType);

	base::StrID getSimpleMangledName(query::Context& ctx, SymID sym_id) {
		return ctx.query<QueryMangledSymbol>(KeyOf_MangledSymbol{ .symbol_key = sym_id });
	}

	template<>
	base::StrID getSpecialMangledName<
		ManglingSymbolKind::ModuleConstructor,
		special_symbol_keys::LIRModuleID>(
		query::Context& ctx, special_symbol_keys::LIRModuleID mod_id
	) {
		return ctx.query<QueryMangledSymbol>(KeyOf_MangledSymbol{
			.symbol_key = mod_id, .kind = ManglingSymbolKind::ModuleConstructor });
	}

	template<>
	base::StrID getSpecialMangledName<
		ManglingSymbolKind::ModuleDestructor,
		special_symbol_keys::LIRModuleID>(
		query::Context& ctx, special_symbol_keys::LIRModuleID mod_id
	) {
		return ctx.query<QueryMangledSymbol>(KeyOf_MangledSymbol{
			.symbol_key = mod_id, .kind = ManglingSymbolKind::ModuleDestructor });
	}

	template<>
	base::StrID getSpecialMangledName<ManglingSymbolKind::GlobalVariableConstructor>(
		query::Context& ctx, SymID sym_id
	) {
		return ctx.query<QueryMangledSymbol>(KeyOf_MangledSymbol{
			.symbol_key = sym_id, .kind = ManglingSymbolKind::GlobalVariableConstructor });
	}

	template<>
	base::StrID getSpecialMangledName<ManglingSymbolKind::GlobalVariableDestructor>(
		query::Context& ctx, SymID sym_id
	) {
		return ctx.query<QueryMangledSymbol>(KeyOf_MangledSymbol{
			.symbol_key = sym_id, .kind = ManglingSymbolKind::GlobalVariableDestructor });
	}
}
