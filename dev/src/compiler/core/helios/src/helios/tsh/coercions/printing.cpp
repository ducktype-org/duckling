// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file printing.cpp
 * @brief Coercion printing.
 */

#include "coercion.hpp"
#include "coercion_error.hpp"
#include "coercion_node.hpp"
#include "coercion_path.hpp"
#include "coercion_rank.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <algorithm>
#include <ostream>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>

namespace compiler::tsh::coercions {
	namespace {
		/// Width the labels of the fields are padded to.
		constexpr usize LABEL_WIDTH = 10;

		/// A field label, indented and padded.
		std::string field(const std::string_view indent, const std::string_view label) {
			std::string out{ indent };
			out += "  ";
			out += label;
			out.append(LABEL_WIDTH - std::min(LABEL_WIDTH, label.size()), ' ');
			out += "  ";
			return out;
		}

		// Every piece of a coercion, written the way a debug print shows it.

		void print(std::ostream& out, const Rank rank) {
			switch (rank) {
			case Rank::Identity:
				out << "Identity";
				return;
			case Rank::MutabilityChange:
				out << "MutabilityChange";
				return;
			case Rank::Deref:
				out << "Deref";
				return;
			case Rank::Numeric:
				out << "Numeric";
				return;
			case Rank::VariantPack:
				out << "VariantPack";
				return;
			case Rank::LiftToType:
				out << "LiftToType";
				return;
			case Rank::UserConversion:
				out << "UserConversion";
				return;
			case Rank::ZeroCheck:
				out << "ZeroCheck";
				return;
			case Rank::RetypeVoid:
				out << "RetypeVoid";
				return;
			}

			CORE_UNREACHABLE();
		}

		void print(std::ostream& out, const CoercionStep& step) {
			out << step.name();
			v_if_matches(step.kind, coercion_step::VariantPack, packing) {
				out << " " << packing->alternative;
			}
		}

		void print(std::ostream& out, const CoercionFailure& failure) {
			using namespace coercion_error;

			variant_match(failure) {
				variant_case_novalue(IncompatibleTypes) { out << "incompatible types"; }
				variant_case_novalue(MutabilityMismatch) { out << "mutability mismatch"; }
				variant_case_novalue(SubPartRefused) { out << "a part does not fit"; }
				variant_case_novalue(NotImplemented) { out << "not implemented"; }
				variant_case(AmbiguousCoercion, ambiguous) {
					out << "ambiguous, alternatives "
						<< base::strJoin(
							   ambiguous.candidates
								   | std::views::transform([](const usize candidate) {
										 return base::toString(candidate);
									 }),
							   ", "
						   );
				}
				variant_case_novalue(TypeNotCopyable) { out << "type not copyable"; }
				variant_case_novalue(RequiresExplicitCopyMove) {
					out << "requires explicit copy or move";
				}
			}
		}

		void print(std::ostream& out, const CoercionPath path) {
			switch (path.in) {
			case CoercionPath::In::Component:
				out << "component " << path.index;
				return;
			case CoercionPath::In::Alternative:
				out << "alternative " << path.index;
				return;
			case CoercionPath::In::Parameter:
				out << "parameter " << path.index;
				return;
			case CoercionPath::In::Result:
				out << "result";
				return;
			}

			CORE_UNREACHABLE();
		}

		void print(std::ostream& out, const SymbolType<>& source) { out << source.toString(); }

		void print(std::ostream& out, const ExpressionType<>& source) {
			out << source.getSymbolType().toString() << " [" << source.getValueCategory().toString()
				<< "]";
		}

		void printRefusal(
			std::ostream& out, const std::string_view indent, const CoercionError& refusal
		) {
			out << field(indent, "refused");
			print(out, refusal.reason);
			out << "\n";

			const std::string cause_indent = field(indent, "");
			for (const auto& [index, cause]: std::views::zip(std::views::iota(0u), refusal.causes)) {
				out << field(indent, index == 0 ? "caused by" : "");
				print(out, cause.at);
				out << ": ";
				cause.error.debugPrint(out, cause_indent);
			}
		}
	}

	void CoercionError::debugPrint(std::ostream& out, const std::string_view indent) const {
		print(out, reason);
		out << ": " << source.toString() << " -> " << target.toString() << "\n";

		const std::string cause_indent = base::strConcat(indent, "  ");
		for (const Cause& cause: causes) {
			out << cause_indent;
			print(out, cause.at);
			out << ": ";
			cause.error.debugPrint(out, cause_indent);
		}
	}

	void CoercionStep::debugPrint(std::ostream& out, const std::string_view indent) const {
		print(out, *this);
		out << "  ->  " << result.toString() << "\n";

		v_if_matches(kind, coercion_step::Elementwise, elementwise) {
			const std::string part_indent = base::strConcat(indent, "  ");
			for (const auto& [index, part]:
			     std::views::zip(std::views::iota(0u), elementwise->parts)) {
				out << part_indent << "component " << index << ":  " << part.source.toString()
					<< "  ->  " << part.target.toString() << "\n";
				part.debugPrint(out, part_indent);
			}
		}
	}

	void CoercionNode::debugPrint(std::ostream& out, const std::string_view indent) const {
		out << field(indent, "rank");
		print(out, rank.getRank());
		out << "\n";

		if (steps.empty()) {
			out << field(indent, "steps") << "nothing\n";
			return;
		}

		const std::string step_indent = field(indent, "");
		for (const auto& [index, step]: std::views::zip(std::views::iota(0u), steps)) {
			out << field(indent, index == 0 ? "steps" : "");
			step.debugPrint(out, step_indent);
		}
	}

	template<CoercionSource Source>
	void CoercionTree<Source>::debugPrint(std::ostream& out, const std::string_view indent) const {
		constexpr std::string_view WHAT
			= std::is_same_v<Source, ExpressionType<>> ? "coercion" : "plan";

		out << WHAT << "  ";
		print(out, source);
		out << "  ->  " << getTarget().toString() << "\n";

		if (isRefused()) {
			printRefusal(out, indent, getError());
			return;
		}

		getRoot().debugPrint(out, indent);
	}

	// `debugPrint` is the only member of a tree that is not written in the header, so it is the
	// only one instantiated here.
	template void CoercionTree<SymbolType<>>::debugPrint(std::ostream&, std::string_view) const;
	template void CoercionTree<ExpressionType<>>::debugPrint(std::ostream&, std::string_view) const;
}
