// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file coercion_tester.hpp
 * @brief A small framework for testing coercions.
 * ```
 * coerce("(1i32, move b)", "(i64, box i32)").succeeds().steps({ ELEMENTWISE }).part(1).steps({});
 * plan("u8", "u16 | u32").refuses().ambiguousBetween({ 0, 1 });
 * plan(type("ref i32"), immutable(type("ref i32"))).succeeds().steps({ MUTABILITY_CHANGE });
 * ```
 */

#pragma once

#include <helios/hout/elements/expr.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/coercions/coercion.hpp>
#include <helios/tsh/coercions/coercion_error.hpp>
#include <helios/tsh/coercions/coercion_node.hpp>
#include <helios/tsh/coercions/queries.hpp>
#include <helios/tsh/coercions/value_source.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/types.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/ref.hpp>
#include <base/pointers/shared_box.hpp>
#include <base/str/str_utils.hpp>

#include <filesystem/file.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

#include <algorithm>
#include <any>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace coercion_testing {
	using namespace compiler::tsh;
	using namespace compiler::tsh::coercions;

	class CoercionTestSuite;

	/**
	 * @brief What @p action computes in a query context of its own.
	 */
	template<typename Action>
	[[nodiscard]] auto inContext(const Action& action) {
		using Result = decltype(action(std::declval<query::Context&>()));
		return std::any_cast<Result>(query::utils::withContextCompute(
			[&](query::Context& ctx) -> std::any { return action(ctx); }
		));
	}

	/// @p type as a mutable value of its own, for a type Duckling cannot write, like `byte`.
	[[nodiscard]] inline SymbolType<> direct(const AbstractType& type) {
		return { type, ReferenceKind::Direct, Mutability::Mutable };
	}

	/// @p type seen as immutable, which a parameter does not keep yet (#1488).
	[[nodiscard]] inline SymbolType<> immutable(const SymbolType<>& type) {
		return { type.getType(), type.getRefKind(), Mutability::Immutable };
	}

	/// The variant of @p alternatives, for one Duckling cannot write, like a variant of tuples only.
	[[nodiscard]] inline SymbolType<> variantOf(const std::vector<SymbolType<>>& alternatives) {
		return direct(inContext([&](query::Context& ctx) {
			return AbstractType{ ctx.query<QueryVariantType>({ .underlying_types = alternatives }) };
		}));
	}

	/// The type of a function from @p parameter to @p result.
	[[nodiscard]] inline SymbolType<> functionOf(
		const SymbolType<>& parameter, const SymbolType<>& result
	) {
		return direct(inContext([&](query::Context& ctx) {
			return AbstractType{ ctx.query<QueryFunctionType>({
				.parameter_types = { parameter },
				.result_type     = result,
			}) };
		}));
	}

	inline constexpr std::string_view DEREF             = coercion_step::Deref::NAME;
	inline constexpr std::string_view HAND_OVER         = coercion_step::HandOver::NAME;
	inline constexpr std::string_view IMPLICIT_MOVE     = coercion_step::ImplicitMove::NAME;
	inline constexpr std::string_view MUTABILITY_CHANGE = coercion_step::MutabilityChange::NAME;
	inline constexpr std::string_view NUMERIC           = coercion_step::Numeric::NAME;
	inline constexpr std::string_view ZERO_CHECK        = coercion_step::ZeroCheck::NAME;
	inline constexpr std::string_view LIFT_TO_TYPE      = coercion_step::LiftToType::NAME;
	inline constexpr std::string_view RETYPE_VOID       = coercion_step::RetypeVoid::NAME;
	inline constexpr std::string_view ELEMENTWISE       = coercion_step::Elementwise::NAME;
	inline constexpr std::string_view VARIANT_PACK      = coercion_step::VariantPack::NAME;
	inline constexpr std::string_view USER_CONVERSION   = coercion_step::UserConversion::NAME;

	/**
	 * @brief The coercion a check is made on.
	 */
	struct Subject final {
		/// Which coercion this is, like "`(1i32, x)` into `(i64, i64)`".
		std::string description;

		/// What `debugPrint` wrote about the whole tree.
		std::string rendering;

		/// The types the coercion was asked about.
		SymbolType<> source;
		SymbolType<> target;

		/// The root of the tree, or the refusal.
		std::variant<CoercionNode, CoercionError> outcome;
	};

	/**
	 * @brief What every check is made on: one coercion, and the place in its tree it looks at.
	 */
	class Check {
	protected:
		Check(CoercionTestSuite& suite, CSharedBox<Subject> subject, std::string where):
			  suite(suite),
			  subject(std::move(subject)),
			  where(std::move(where)) {}

		/// Fails the test with @p what unless @p condition holds.
		void expect(bool condition, std::string_view what) const;

		CoercionTestSuite&  suite;
		CSharedBox<Subject> subject;
		std::string         where;
	};

	/**
	 * @brief Checks on one node of a coercion that succeeded.
	 */
	class NodeCheck final: public Check {
	public:
		NodeCheck(
			CoercionTestSuite&  suite,
			CSharedBox<Subject> subject,
			const CoercionNode& node,
			std::string         where
		):
			  Check(suite, std::move(subject), std::move(where)),
			  node(&node) {}

		// The checks chain, so the result of the last one of every chain is dropped on purpose.
		// NOLINTBEGIN(modernize-use-nodiscard)

		/// The steps of this node are exactly @p expected, named by the constants above, like
		/// `DEREF`.
		const NodeCheck& steps(const std::vector<std::string_view>& expected) const;

		/// The worst thing this node does, parts included, is @p rank.
		const NodeCheck& ranks(Rank rank) const;

		/// Nothing at all happens to the value at runtime.
		const NodeCheck& doesNothing() const;

		/// The value is packed into the alternative `SymbolType::toString` writes as
		/// @p alternative.
		const NodeCheck& packsInto(std::string_view alternative) const;

		// NOLINTEND(modernize-use-nodiscard)

		/// The node about the part @p index of the value, which this node takes apart.
		[[nodiscard]] NodeCheck part(usize index) const;

		/// The node itself, for what the checks above do not cover.
		[[nodiscard]]
		const CoercionNode& get() const noexcept {
			return *node;
		}

	private:
		base::CRef<CoercionNode> node;
	};

	/**
	 * @brief Checks on one level of a refusal.
	 */
	class RefusalCheck final: public Check {
	public:
		RefusalCheck(
			CoercionTestSuite&   suite,
			CSharedBox<Subject>  subject,
			const CoercionError& error,
			std::string          where
		):
			  Check(suite, std::move(subject), std::move(where)),
			  error(&error) {}

		// The checks chain, so the result of the last one of every chain is dropped on purpose.
		// NOLINTBEGIN(modernize-use-nodiscard)

		/// This level refused for the reason @p Reason.
		template<typename Reason>
		const RefusalCheck& because() const {
			expect(v_matches(error->reason, Reason), "the reason differs");
			return *this;
		}

		/// This level is an ambiguity between exactly the alternatives @p alternatives.
		const RefusalCheck& ambiguousBetween(const std::vector<usize>& alternatives) const;

		/// This level is about coercing @p source into @p target, as `SymbolType::toString` writes
		/// them.
		const RefusalCheck& about(std::string_view source, std::string_view target) const;

		/// This level is about exactly the two types the coercion was asked about.
		const RefusalCheck& aboutTheWholeCoercion() const;

		/// This level follows from exactly @p count refusals below it.
		const RefusalCheck& causes(usize count) const;

		// NOLINTEND(modernize-use-nodiscard)

		/// The refusal of the component @p index of a composite, below this one.
		[[nodiscard]]
		RefusalCheck component(const usize index) const {
			return cause(CoercionPath::In::Component, index);
		}

		/// The refusal of the alternative @p index of a variant, below this one.
		[[nodiscard]]
		RefusalCheck alternative(const usize index) const {
			return cause(CoercionPath::In::Alternative, index);
		}

	private:
		[[nodiscard]] RefusalCheck cause(CoercionPath::In in, usize index) const;

		base::CRef<CoercionError> error;
	};

	/**
	 * @brief A whole coercion, or a whole plan, and the checks on it.
	 */
	class CoercionCheck final: public Check {
	public:
		template<CoercionSource Source>
		CoercionCheck(
			CoercionTestSuite&          suite,
			const std::string&          description,
			const CoercionTree<Source>& tree,
			const SymbolType<>&         target
		):
			  Check(suite, subjectOf(std::move(description), tree, target), "") {}

		/// The coercion may be carried out. The checks go on on its root.
		[[nodiscard]] NodeCheck succeeds() const;

		/// The coercion is refused. The checks go on on the top level of the refusal.
		[[nodiscard]] RefusalCheck refuses() const;

	private:
		template<CoercionSource Source>
		[[nodiscard]] static CSharedBox<Subject> subjectOf(
			const std::string&          description,
			const CoercionTree<Source>& tree,
			const SymbolType<>&         target
		) {
			std::stringstream rendering;
			tree.debugPrint(rendering);

			const SymbolType<> source = [&]() -> SymbolType<> {
				if constexpr (std::is_same_v<Source, SymbolType<>>)
					return tree.getSource();
				else
					return tree.getSource().getSymbolType();
			}();

			return makeSharedBox<const Subject>(Subject{
				.description = std::move(description),
				.rendering   = std::move(rendering).str(),
				.source      = source,
				.target      = target,
				.outcome     = tree.isValid()
			                     ? std::variant<CoercionNode, CoercionError>{ tree.getRoot() }
			                     : std::variant<CoercionNode, CoercionError>{ tree.getError() },
			});
		}
	};

	/**
	 * @brief A test suite that tests coercions. Derive from it instead of `tester::TestSuite`.
	 *
	 * Every written case is compiled as a module of its own, after `PRELUDE`. A type written alone
	 * is compiled the same way, so a class in it is not the class of any other case.
	 */
	class CoercionTestSuite: public tester::TestSuite {
	public:
		/// What every check reports to, so that a failed check fails the test.
		void expect(const bool condition, const std::string_view what) {
			assertTrue(condition, what);
		}

		~CoercionTestSuite() override { fs::FileManager::deleteFolder(modules_root, true); }

	protected:
		/**
		 * @brief Declarations every written case may use.
		 */
		static constexpr std::string_view PRELUDE = R"(
# Owns heap storage, so it is copyable but not trivially copyable.
class Owner {
    b: box i32;
}

fun makeOwner() -> Owner = {
    var o: Owner;
    return o;
}

fun makeBox() -> box i32 = {
    return new 1;
}

fun makeBoxAndInt() -> (box i32, i32) = {
    return (new 1, 2);
}

fun makeBoxAndConstInt() -> (box i32, const i32) = {
    return (new 1, 2);
}

var global_box: box i32 = new 1;
var global_pair: (box i32, i32) = (new 1, 2);
)";

		/**
		 * @brief The locals every written value may read.
		 */
		static constexpr std::string_view LOCALS = R"(
    var x: i32 = 1;
    var b: box i32 = new 1;
    var t: (box i32, i32) = (new 1, 2);
    var r: ref (box i32, i32) = &t;
    var c: (box i32, const i32) = (new 1, 2);
    var o: Owner;
    let lb: box i32 = new 1;
    let lr: ref i32 = &x;
)";

		CoercionTestSuite(tester::TestConfig config, const std::string_view name):
			  tester::TestSuite(std::move(config), name),
			  modules_root(fs::FileManager::createRandomVirtualDirectory()) {}

		/**
		 * @brief The coercion of the value @p value, which may read `LOCALS`, into a location of
		 * the type @p into.
		 *
		 * A tuple literal is described part by part, like the caller of `coercionOf` describes it.
		 */
		[[nodiscard]] CoercionCheck coerce(
			const std::string_view value, const std::string_view into
		) {
			const std::string function = base::strConcat(
				"fun coercion_case(into: ",
				into,
				") = {",
				LOCALS,
				"    var value = ",
				value,
				";\n}\n"
			);
			return compiled(function, [&](const compiler::helios::ScopeID body) {
				using namespace compiler::helios::test_utils;

				const SymbolType<> target = getSymbolTypeOf("into", body);
				const ValueSource  source
					= describeValue(*getExprOfVariable(getChain("value", body).back()));
				return CoercionCheck{
					*this,
					base::strConcat("`", value, "` into `", into, "`"),
					inContext([&](query::Context& ctx) { return coercionOf(ctx, source, target); }),
					target,
				};
			});
		}

		/**
		 * @brief The coercion of a value described by hand, whose parts are parts of it.
		 */
		[[nodiscard]] CoercionCheck coerce(const ExpressionType<>& value, const SymbolType<>& into) {
			return {
				*this,
				base::strConcat(
					"`",
					value.getSymbolType().toString(),
					" [",
					value.getValueCategory().toString(),
					"]` into `",
					into.toString(),
					"`"
				),
				inContext([&](query::Context& ctx) {
					return coercionOf(ctx, ValueSource::singleValueSource(value), into);
				}),
				into,
			};
		}

		/**
		 * @brief The plan between the types @p from and @p into, written in one module.
		 */
		[[nodiscard]] CoercionCheck plan(const std::string_view from, const std::string_view into) {
			const std::string function
				= base::strConcat("fun coercion_case(from: ", from, ", into: ", into, ") = {\n}\n");
			return compiled(function, [&](const compiler::helios::ScopeID body) {
				using namespace compiler::helios::test_utils;

				return planned(
					getSymbolTypeOf("from", body),
					getSymbolTypeOf("into", body),
					base::strConcat("`", from, "` into `", into, "`")
				);
			});
		}

		/**
		 * @brief The plan between two types built by hand.
		 */
		[[nodiscard]] CoercionCheck plan(const SymbolType<>& from, const SymbolType<>& into) {
			return planned(
				from, into, base::strConcat("`", from.toString(), "` into `", into.toString(), "`")
			);
		}

		/**
		 * @brief The type @p written, to build by hand what Duckling cannot write out of it.
		 */
		[[nodiscard]] SymbolType<> type(const std::string_view written) {
			return compiled(
				base::strConcat("fun coercion_case(t: ", written, ") = {\n}\n"),
				[](const compiler::helios::ScopeID body) {
					return compiler::helios::test_utils::getSymbolTypeOf("t", body);
				}
			);
		}

	private:
		[[nodiscard]] CoercionCheck planned(
			const SymbolType<>& from, const SymbolType<>& into, const std::string& description
		) {
			return {
				*this,
				description,
				inContext([&](query::Context& ctx) {
					return SymbolTypeCoercion{ *ctx.query<QuerySymbolTypeCoercion>({ from, into }) };
				}),
				into,
			};
		}

		/**
		 * @brief The value @p expr, described the way its caller describes it to `coercionOf`: a
		 * tuple literal part by part, anything else as it is.
		 */
		[[nodiscard]] static ValueSource describeValue(const compiler::helios::code::Expr& expr) {
			const auto* tuple = dynamic_cast<const compiler::helios::code::TupleExpr*>(&expr);
			if (tuple == nullptr) return ValueSource::singleValueSource(expr.expression_type);

			std::vector<ValueSource> parts;
			parts.reserve(tuple->elements.size());
			for (const auto& element: tuple->elements) parts.push_back(describeValue(*element));
			return ValueSource::nestedValueSource(expr.expression_type, std::move(parts));
		}

		/**
		 * @brief Compiles @p function after `PRELUDE` as a module of its own and hands @p inspect
		 * the body scope of its `coercion_case` function.
		 *
		 * The module lives in the virtual filesystem, so nothing is left behind on disk. Anything
		 * that goes wrong while compiling prints the module, so that a typo in a case is easy to
		 * find.
		 */
		template<typename Inspect>
		[[nodiscard]] std::invoke_result_t<const Inspect&, compiler::helios::ScopeID> compiled(
			const std::string_view function, const Inspect& inspect
		) {
			using namespace compiler::helios::test_utils;

			const std::string               source = base::strConcat(PRELUDE, "\n", function);
			const std::string               name   = base::strConcat("coercion_case_", next_case++);
			const fs::File                  directory = modules_root.createSubDirectory(name);
			[[maybe_unused]] const fs::File file
				= directory.createSubFile(source, base::strConcat(name, ".dk"));

			try {
				auto [module, root_scope] = getModule(directory);
				return inspect(getFunctionBodyScope(getChain("coercion_case", root_scope).back()));
			} catch (const std::exception&) {
				message(base::strConcat("While compiling the coercion case:\n", source));
				throw;
			}
		}

		/// The virtual directory the modules of the written cases are put in.
		fs::File modules_root;

		/// The number of the next written case, which names its module.
		usize next_case = 0;
	};

	// ---------------------------------------------------------------------------------------------
	// The checks.
	// ---------------------------------------------------------------------------------------------

	inline void Check::expect(const bool condition, const std::string_view what) const {
		if (condition) return;
		suite.expect(
			false,
			base::strConcat(
				subject->description, where, ": ", what, "\nThe whole tree:\n", subject->rendering
			)
		);
	}

	inline const NodeCheck& NodeCheck::steps(const std::vector<std::string_view>& expected) const {
		std::string listed;
		for (const std::string_view step: expected) listed += base::strConcat("`", step, "` ");
		expect(
			std::ranges::equal(node->steps, expected, {}, &CoercionStep::name),
			base::strConcat("expected the steps ", listed)
		);
		return *this;
	}

	inline const NodeCheck& NodeCheck::ranks(const Rank rank) const {
		expect(node->rank.getRank() == rank, "the rank differs");
		return *this;
	}

	inline const NodeCheck& NodeCheck::doesNothing() const {
		expect(node->isNoOp(), "expected nothing to happen to the value");
		return *this;
	}

	inline const NodeCheck& NodeCheck::packsInto(const std::string_view alternative) const {
		expect(
			not node->steps.empty()
				&& v_matches(node->steps.back().kind, coercion_step::VariantPack),
			"expected the last step to pack the value into a variant"
		);

		const usize packed_as
			= v_get(node->steps.back().kind, coercion_step::VariantPack).alternative;
		const std::string packed_into
			= VariantAbstractType{ node->target.getType() }.getUnderlyingTypes()[packed_as].toString(
			);
		expect(
			packed_into == alternative,
			base::strConcat(
				"expected the value packed as `", alternative, "`, not `", packed_into, "`"
			)
		);
		return *this;
	}

	inline NodeCheck NodeCheck::part(const usize index) const {
		for (const CoercionStep& step: node->steps) {
			v_if_matches(step.kind, coercion_step::Elementwise, elementwise) {
				expect(
					index < elementwise->parts.size(), base::strConcat("expected a part ", index)
				);
				return {
					suite,
					subject,
					elementwise->parts[index],
					base::strConcat(where, ", component ", index),
				};
			}
		}

		expect(false, "expected the value to be taken apart");
		CORE_UNREACHABLE();
	}

	inline const RefusalCheck& RefusalCheck::ambiguousBetween(const std::vector<usize>& alternatives
	) const {
		expect(v_matches(error->reason, coercion_error::AmbiguousCoercion), "expected an ambiguity");
		expect(
			v_get(error->reason, coercion_error::AmbiguousCoercion).candidates == alternatives,
			"expected other alternatives to tie"
		);
		return *this;
	}

	inline const RefusalCheck& RefusalCheck::about(
		const std::string_view source, const std::string_view target
	) const {
		expect(
			error->source.toString() == source && error->target.toString() == target,
			base::strConcat("expected the refusal to be about `", source, "` into `", target, "`")
		);
		return *this;
	}

	inline const RefusalCheck& RefusalCheck::aboutTheWholeCoercion() const {
		expect(
			error->source == subject->source && error->target == subject->target,
			"expected the refusal to be about what was asked"
		);
		return *this;
	}

	inline const RefusalCheck& RefusalCheck::causes(const usize count) const {
		expect(
			error->causes.size() == count,
			base::strConcat("expected ", count, " causes, found ", error->causes.size())
		);
		return *this;
	}

	inline RefusalCheck RefusalCheck::cause(const CoercionPath::In in, const usize index) const {
		for (const CoercionError::Cause& cause: error->causes)
			if (cause.at.in == in && cause.at.index == index)
				return { suite, subject, cause.error, base::strConcat(where, ", cause ", index) };

		expect(false, base::strConcat("expected a cause at index ", index));
		CORE_UNREACHABLE();
	}

	inline NodeCheck CoercionCheck::succeeds() const {
		expect(v_matches(subject->outcome, CoercionNode), "expected the coercion to succeed");
		return { suite, subject, v_get(subject->outcome, CoercionNode), "" };
	}

	inline RefusalCheck CoercionCheck::refuses() const {
		expect(v_matches(subject->outcome, CoercionError), "expected the coercion to be refused");
		return { suite, subject, v_get(subject->outcome, CoercionError), "" };
	}
}
