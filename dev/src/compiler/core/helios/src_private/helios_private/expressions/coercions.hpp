#pragma once

#include <helios/hout/elements/expr.hpp>
#include <typesystem/higher/symbol_type.hpp>

#include <query_framework/context.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {
	struct InvalidCoercion final {};
	class Coercion;

	/**
	 * @brief Checks if a coercion from `from` to `to` is possible and returns
	 * a function performing the coercion if it is.
	 */
	query::QResult<Coercion, InvalidCoercion> canCoerceExpression(
		query::Context& ctx, CRef<code::Expr> from, tsh::SymbolType<> to
	);

	/**
	 * @brief This struct represents a function that performs a coercion from one expression to
	 * another. It was added to make sure that the coercion is always valid (by calling
	 * `canCoerceExpression` first), and it checks if the expression being coerced and desired type
	 * is the same as the one validated.
	 */
	class Coercion {
	public:
		/**
		 * Main function that creates a coerced expression from the old one.
		 */
		Box<code::Expr> operator()(Box<code::Expr> from) const;
		/**
		 * The expression that was validated to be coercible.
		 */
		const CRef<code::Expr> validated_source;
		/**
		 * The target type of the coercion.
		 */
		const tsh::SymbolType<> to;  // target type

		[[nodiscard]] bool isValidFor(CRef<code::Expr> expr) const noexcept {
			return expr.get() == validated_source.get();
		}

		friend query::QResult<Coercion, InvalidCoercion> canCoerceExpression(
			query::Context& ctx, CRef<code::Expr> from, tsh::SymbolType<> to
		);

	private:
		Coercion(CRef<code::Expr> validated_source, tsh::SymbolType<> to):
			  validated_source(validated_source),
			  to(to) {}
	};

}
