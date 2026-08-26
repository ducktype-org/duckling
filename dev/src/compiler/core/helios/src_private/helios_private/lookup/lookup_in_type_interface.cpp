#include "lookup_in_type_interface.hpp"

#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/pst_layer/pst_parent.hpp>
#include <helios_private/scopes/scope_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {

	namespace {
		/**
		 * @brief The class whose body the given scope is inside of, if any.
		 *
		 * It is the class that decides which private and protected members are visible, so a
		 * scope outside of any class body sees only the public ones.
		 */
		base::Optional<tsh::ClassAbstractType> enclosingClassOf(
			query::Context& ctx, base::Optional<ScopeID> scope
		) {
			if_opt_none(scope) return {};

			auto related_element = ScopeAccess_Functor::get(scope.value())->relatedPSTElement();
			if_opt_none(related_element) return {};

			auto element = related_element.value().unlock(ctx);
			while (true) {
				if (element->getElementKind() == pst::ElementKind::Class) {
					auto class_symbol = ctx.query<QuerySymbolOfSTMT>(element).valueOrThrow();
					return ctx.query<tsh::QueryClassType>(class_symbol);
				}

				auto pst_parent = getPSTElementParent(ctx, element);
				if (!pst_parent.isLangElement()) return {};
				element = pst_parent.getAsLangElement().unlock(ctx);
			}
		}

		/**
		 * @brief Whether the accessing class is the declaring one, or inherits from it.
		 */
		bool derivesFrom(
			query::Context&        ctx,
			tsh::ClassAbstractType accessing_class,
			tsh::AbstractType      declaring_class
		) {
			base::Optional<tsh::ClassAbstractType> current = accessing_class;
			while (current.has_value()) {
				if (current.value() == declaring_class) return true;
				current = current.value().getBaseClassType(ctx);
			}
			return false;
		}

		/**
		 * @brief Whether the element can be used from the class the lookup is written in.
		 *
		 * The visibility is checked against the class that declares the element, which for an
		 * inherited member is not the class the lookup is performed on.
		 */
		bool isVisible(
			query::Context&                        ctx,
			const tsh::InterfaceElement&           element,
			base::Optional<tsh::ClassAbstractType> accessing_class
		) {
			switch (element.getVisibility()) {
			case tsh::MemberVisibility::Public:
				return true;
			case tsh::MemberVisibility::Protected:
				return accessing_class.has_value()
				   and derivesFrom(ctx, accessing_class.value(), element.getSource());
			case tsh::MemberVisibility::Private:
				return accessing_class.has_value()
				   and accessing_class.value() == element.getSource();
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Whether the element can be reached by the kind of access being performed.
		 *
		 * A field and a method need a value to be used on, so they are only reachable through an
		 * instance. Everything else belongs to the type itself and is reachable both ways.
		 */
		bool isReachable(TypeAccessMode mode, const tsh::InterfaceElement& element) {
			switch (mode) {
			case TypeAccessMode::Instance:
				return element.isAnyField() or element.isAnyMethod();
			case TypeAccessMode::Meta:
				return not element.isStaticField() and not element.isStaticMethod();
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Look a name up in the interface of a type, keeping the elements the given kind of
		 * access can reach, and splitting them by whether the lookup's scope may use them.
		 */
		LookupResult lookupInTypeInterface(query::Context& ctx, const KeyOf_LookupInType& key) {
			auto        interface = key.type.getInterface(ctx);
			const auto& elements  = interface->getElementsWithName(key.name);

			base::Optional<tsh::ClassAbstractType> accessing_class
				= enclosingClassOf(ctx, key.accessing_scope);

			LookupResult result;
			for (const auto& element: elements) {
				if (not isReachable(key.mode, element)) continue;

				if (isVisible(ctx, element, accessing_class))
					result.leaves.emplace_back(element.getSymbol());
				else
					result.inaccessible.emplace_back(element.getSymbol());
			}

			return result;
		}
	}

	struct IMPLEMENT_QUERY(QueryLookupInType, query::QResult<LookupResult>) {
		static auto provide(query::Context& ctx, const QKey& key) -> PResult {
			return lookupInTypeInterface(ctx, key);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryLookupInType);
}
