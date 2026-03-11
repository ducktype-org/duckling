#pragma once

#include "debug_info.hpp"

#include <string>

namespace debug_info {

	// Forward-declared so FunctionBuilder can reference it before the full definition.
	class DebugInfoBuilder;

	// -------------------------------------------------------------------------

	/**
	 * @brief Accumulates instructions for a single function entry.
	 *
	 * Obtained by calling DebugInfoBuilder::beginFunction(). Instructions are
	 * added one by one (no need to re-specify the function name per call), and
	 * end() returns the parent builder so the two can be chained fluently:
	 *
	 * @code
	 *   builder
	 *     .beginFunction("Mangledfoo", "foo", pos)
	 *       .addInstruction(0, srcPos)
	 *       .addInstruction(4, srcPos2)
	 *     .end()
	 *     .beginFunction(...)
	 *     ...
	 *     .build();
	 * @endcode
	 *
	 * @note The FunctionBuilder holds a reference to its parent DebugInfoBuilder.
	 *       The parent must outlive all FunctionBuilders it creates.
	 */
	class FunctionBuilder {
	public:
		/**
		 * @brief Adds an instruction entry at @p offset with the given full metadata.
		 */
		FunctionBuilder& addInstruction(u64 offset, InstructionMetadata metadata);

		/**
		 * @brief Convenience overload: wraps @p position in an InstructionMetadata.
		 */
		FunctionBuilder& addInstruction(u64 offset, SourcePosition position);

		/**
		 * @brief Finalizes this function, writes it into the parent builder, and
		 *        returns the parent so further builder calls can be chained.
		 *
		 * The FunctionBuilder must not be used after calling end().
		 */
		DebugInfoBuilder& end();

	private:
		friend class DebugInfoBuilder;

		FunctionBuilder(
			DebugInfoBuilder& parent, std::string mangled_name, FunctionMetadata metadata
		);

		DebugInfoBuilder& parent;
		std::string       mangled_name;
		FunctionMetadata  metadata;
	};

	// -------------------------------------------------------------------------

	/**
	 * @brief Builds a DebugInfo object incrementally.
	 *
	 * Typical usage:
	 * @code
	 *   DebugInfo info = DebugInfoBuilder(Target::DBC, "mymodule.dmf",
	 *                                     SourcePositionsType::PstHash)
	 *     .addType("_TMyType", "MyType")
	 *     .beginFunction("foo", "foo", functionPos)
	 *       .addInstruction(0,  instrPos0)
	 *       .addInstruction(4,  instrPos4)
	 *     .end()
	 *     .build();
	 * @endcode
	 */
	class DebugInfoBuilder {
	public:
		DebugInfoBuilder(
			Target target, std::string module_path, SourcePositionsType source_positions_type
		);

		// ------------------------------------------------------------------
		// Types

		/**
		 * @brief Adds a type entry from a pre-built TypeMetadata.
		 */
		DebugInfoBuilder& addType(std::string mangled_name, TypeMetadata metadata);

		/**
		 * @brief Convenience overload: builds TypeMetadata{ .name = display_name }.
		 */
		DebugInfoBuilder& addType(std::string mangled_name, std::string display_name);

		// ------------------------------------------------------------------
		// Functions

		/**
		 * @brief Begins construction of a function entry from a pre-built FunctionMetadata.
		 *
		 * Instructions can be appended on the returned FunctionBuilder; call its
		 * end() to return here and continue building.
		 */
		FunctionBuilder beginFunction(std::string mangled_name, FunctionMetadata metadata);

		/**
		 * @brief Convenience overload: builds FunctionMetadata from name + position.
		 */
		FunctionBuilder beginFunction(
			std::string mangled_name, std::string function_name, SourcePosition position
		);

		// ------------------------------------------------------------------

		/**
		 * @brief Finalizes and returns the accumulated DebugInfo.
		 *
		 * The builder is left in a valid-but-empty state after this call;
		 * calling build() again will return a default-constructed DebugInfo.
		 * This allows build() to be used both on temporaries and on lvalue
		 * references (e.g. at the end of a chain through end()).
		 */
		DebugInfo build();

	private:
		friend class FunctionBuilder;

		/// Called by FunctionBuilder::end() to commit the finished function.
		void finalizeFunction(std::string mangled_name, FunctionMetadata metadata);

		DebugInfo info;
	};

}  // namespace debug_info
