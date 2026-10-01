/**
 * @file best_coercion.hpp
 * @brief The main "best coercion picking" logic. Used when performing overload resolution or when
 * picking the variant alternative to pack the value into.
 */

#pragma once

#include "coercion_rank.hpp"

#include <base/types/ints.hpp>

#include <span>
#include <variant>
#include <vector>

namespace compiler::tsh::coercions {
	/**
	 * @brief One coercion candidate and how good is it.
	 */
	struct Candidate final {
		/// The identification of the candidate (like the index of a variant alternative). The
		/// candidates that refused never get here, so it is not the index in a vector.
		usize id{ 0 };
		/// How good of a coercion this candidate is.
		CoercionRank rank;
	};

	namespace candidate_choice {
		/**
		 * @brief One candidate is better than every other.
		 */
		struct Chosen final {
			usize id;  ///< The id of the best candidate.
		};

		/**
		 * @brief Many candidates are equally good.
		 */
		struct Tied final {
			std::vector<usize> candidates;  ///< The ids of the candidates that tied.
		};

		/**
		 * @brief No candidate provided.
		 */
		struct NoCandidate final {};
	}

	using CandidateChoice
		= std::variant<candidate_choice::Chosen, candidate_choice::Tied, candidate_choice::NoCandidate>;

	/**
	 * @brief Chooses the best candidate out of given or tells why a best candidate couldn't be
	 * picked (a tie or no candidate was given).
	 */
	[[nodiscard]]
	inline CandidateChoice bestCoercion(const std::span<const Candidate> candidates) {
		if (candidates.empty()) return candidate_choice::NoCandidate{};

		CoercionRank       best = candidates.front().rank;
		std::vector<usize> tied{ candidates.front().id };

		for (const Candidate& candidate: candidates.subspan(1)) {
			if (candidate.rank.dominates(best)) {
				best = candidate.rank;
				tied = { candidate.id };
				continue;
			}
			if (candidate.rank.ties(best)) tied.push_back(candidate.id);
		}

		if (tied.size() > 1) return candidate_choice::Tied{ .candidates = std::move(tied) };
		return candidate_choice::Chosen{ .id = tied.front() };
	}
}
