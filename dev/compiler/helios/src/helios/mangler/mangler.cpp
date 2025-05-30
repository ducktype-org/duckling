#include "mangler.hpp"

#include <query_framework/query_impl.hpp>

#include <helios/scope_symbol_id.hpp>
#include <helios/symbols/simple.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include "../src_private/helios_private/symbols/symbols.hpp"
#include "../src_private/helios_private/scopes/scopes.hpp"
#include "../src_private/helios_private/symbols/symbol_data.hpp"
#include <string_view>
#include <frontend/module_tree/queries.hpp>
#include <algorithm>

namespace compiler::helios::mangler {

    namespace detail {

        bool isManglable(SymID symbol_id) {
            switch(kind(symbol_id)) {
                case SymbolKind::Variable:
                case SymbolKind::Field:
                case SymbolKind::Const:

                case SymbolKind::Function:
                case SymbolKind::Method:
                
                case SymbolKind::Constructor:
                case SymbolKind::Destructor:
                    return true;
                default:
                    return false;
            }
        }

        std::string compactNumber(u64 number) {
            using namespace std::literals::string_view_literals;

            if(number == 0) return "_";
            
            static constexpr auto digits = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"sv;
            constexpr u64 base = digits.size();
            
            std::string ret;
            --number;
            do {
                ret += digits[number % base];
                number /= base;
            } while(number > 0);
            std::reverse(ret.begin(), ret.end());
            ret += '_';
            return ret;
        };
        
        std::string identifier(std::string name) {
            // @future: use punnycode for unicode strings
            if(/*isUnicode*/ false) {
                constexpr char unicode_prefix = 'U';
                std::string punny_string;
                return base::strConcat(unicode_prefix, punny_string.size(), punny_string); 
            } else {
                name = std::to_string(name.size()) + name;
                return name;
            }
        };

        std::string path_prefix(SymID symbol_id) {
            auto enclosing_scope = scope(symbol_id);
            auto enclosing_module = module(enclosing_scope);
            
            // "M" <module-name>                                 // standalone module
            // @future: templated modules
            if(/* standalone module */ true) {
                auto module_identifier = identifier(frontend::moduleName(enclosing_module).str());
                return base::strConcat("M", module_identifier);
            }
            
            // @future: packages & scripts
            // "P" <package-name> <module-name>                  // module in a package
            // "S" <script-name>                                 // standalone script
            // "R" <package-name> <module-name> <script-name>    // script in a package

            // @todo: backreference
        }

        std::string unscoped_name(SymID symbol_id) {
            return identifier(compiler::helios::name(symbol_id).str());
        }

        std::string symbol_name(query::Context& ctx, SymID symbol_id) {
            std::cerr << __PRETTY_FUNCTION__ << '\n';

            std::cerr << "symbol_id: " << symbol_id.queryUnstablePerfectHash() << '\n';
            std::cerr << "symbol PstID: " << symbolPst(symbol_id).unlock(ctx)->getID().asInt() << '\n';

            auto scope_id = scope(symbol_id);

            if(scopeDepth(scope_id) == 1) {
                return "G" + unscoped_name(symbol_id);
            } else {
                std::vector<std::string> path_parts;

                auto current_scope = scope_id;
                auto current_pst_id = symbolPst(symbol_id).unlock(ctx)->getID();
                while(true) {
                    std::cerr << "while: scopeDepth: " << scopeDepth(current_scope) << '\n';
                    current_scope.debugPrintScopeAndParents();
                        
                    auto parent_scope = parent(current_scope);
                    if(!parent_scope.has_value()) break;
                    
                    auto symbols_in_parent = *ctx.query<compiler::helios::QuerySymbolsInScope>(parent_scope.value()).get();
                    for(const auto& sym : symbols_in_parent) {
                        auto pst_sym = symbolPst(sym).unlock(ctx);
                        
                        std::cerr << "\t" << sym.queryUnstablePerfectHash() << " - " << compiler::helios::name(sym).str() << '\t';
                        std::cerr << "elem type: " << symbolPst(sym).unlock(ctx)->elementType() << '\n';
                        
                        if(pst_sym->getElementKind() == pst::ElementKind::Namespace) {
                            auto nmsp = pst_sym.dynamicCast<pst::Namespace>().value();
                            auto body = nmsp->getBody().unlock(ctx);

                            for(auto&& child_lck : body->viewChildren()) {
                                auto child = child_lck.unlock(ctx);
                                std::cerr << "\t\tPstID: " << child->getID().asInt() << " elemType: " << child->elementType() << '\n';
                                
                                if(child->getID() == current_pst_id) {
                                    path_parts.push_back(identifier(nmsp->getName().str()));
                                    current_pst_id = pst_sym->getID();
                                }
                            }
                        } else if (pst_sym->getElementKind() == pst::ElementKind::Class) {
                            auto cls = pst_sym.dynamicCast<pst::Class>().value();
                            auto body = cls->getBody().unlock(ctx);
                            std::cerr << "\t\tClass: " << cls->getName().str() << '\n';
                            for(auto&& child_lck : body->viewChildren()) {
                                auto child = child_lck.unlock(ctx);
                                std::cerr << "\t\tPstID: " << child->getID().asInt() << " elemType: " << child->elementType() << '\n';
                                                                
                                if(child->getID() == current_pst_id) {
                                    path_parts.push_back(identifier(cls->getName().str()));
                                    current_pst_id = pst_sym->getID();
                                }
                            }
                        }
                    }
                    
                    current_scope = parent_scope.value();
                }
                    
                std::string ret = "N";
                for(auto&& it = path_parts.rbegin(); it != path_parts.rend(); ++it) {
                    ret += std::move(*it);
                }
                ret += unscoped_name(symbol_id) + "E";
                return ret;
            }
        }

        std::string path(query::Context& ctx, SymID symbol_id) {
            return base::strConcat(
                path_prefix(symbol_id),
                symbol_name(ctx, symbol_id)
            );
        }

        std::string func_type(query::Context& ctx, SymID symbol_id) {
            std::cerr << __PRETTY_FUNCTION__ << '\n';

            std::string ret;
            if(kind(symbol_id) == SymbolKind::Function) {
                ret = "F";
                auto type = ctx.query<QueryTypeOfSymbol>({ symbol_id }).get()->value().getType();
                auto fun_type = tsh::FunctionAbstractType(type);
                
                auto ret_type = fun_type.getResultType();
                ret += ret_type.toString();
                for(auto param : fun_type.getParameterTypes())
                    ret += param.toString();

                ret += "E";
            } else if(kind(symbol_id) == SymbolKind::Method) {
                // @future: methods
            }
            
            return ret;
        }

        std::string symbol_encoding(query::Context& ctx, SymID symbol_id) {
            switch (kind(symbol_id)) {
            case SymbolKind::Variable:
            case SymbolKind::Field: 
            case SymbolKind::Const:
                std::cerr << "Mangling data\n";
                return path(ctx, symbol_id);
                break;
            
            case SymbolKind::Function:
            case SymbolKind::Method:
                std::cerr << "Mangling function\n";
                return path(ctx, symbol_id) + func_type(ctx, symbol_id);
                break;

            case SymbolKind::Constructor:
            case SymbolKind::Destructor:
                // special symbols
                return ""; // todo
                break;

            default:
                return ""; // todo
                break;
            }
        }

        std::string opt_metadata(base::Optional<std::string> metadata) {
            if(!metadata.has_value()) return "";
            return "$" + metadata.value();
        }

    } // namespace detail

    struct IMPLEMENT_QUERY(QueryMangledSymbol, base::Optional<std::string>) {
    static auto provide(Context& ctx, QKey key) -> PResult {
        using namespace std::literals::string_view_literals;

        auto dealiased = ctx.query<helios::QueryDealias>(key.symbol);
        if(!dealiased.get()->hasValue() || dealiased.get()->value().size() != 1)
            return std::nullopt; // todo: ambiguous symbols
        auto symbol_id = dealiased.get()->value().front();

        if(!detail::isManglable(symbol_id)) return std::nullopt; // todo: wrong symbol kind
        
        constexpr auto language_prefix = "_Q"sv;
        const auto mangling_scheme_version = detail::compactNumber(key.mangling_scheme_version);
        std::string encoding = detail::symbol_encoding(ctx, symbol_id);
        std::string metadata = detail::opt_metadata(key.additional_metadata);

        std::string mangled_name = base::strConcat(
            language_prefix,
            mangling_scheme_version,
            encoding,
            metadata
        );
        
        return mangled_name;
    }

    QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMangledSymbol);
    
    
    
} // namespace compiler::helios::mangler
