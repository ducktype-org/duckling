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
		 * @brief Convenience overload: wraps @p position in an InstructionMetadata.
		 */
		FunctionBuilder& addInstruction(u64 offset, SourcePosition position);

		/**
		 * @brief Convenience overload: builds VariableMetadata from name + position.
		 */
		FunctionBuilder& addVariableInit(
			u64 offset, std::string variable_name, base::Optional<SourcePosition> position
		);

		/**
		 * @brief Adds parameter metadata under a parameter index.
		 */
		FunctionBuilder& addParameter(
			u64 index, std::string parameter_name, base::Optional<SourcePosition> position
		);

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
	 *   DebugInfo info = DebugInfoBuilder(Target::DBC,
	 *                                     SourcePositionsType::PstHash)
	 *     .addType("_TMyType", "MyType")
	 *     .beginFunction("foo", "foo", functionPos)
	 *       .addParameter(0, "arg0", arg0Pos)
	 *       .addInstruction(0,  instrPos0)
	 *       .addVariableInit(0, "x", varPos0)
	 *       .addInstruction(4,  instrPos4)
	 *     .end()
	 *     .build();
	 * @endcode
	 */
	class DebugInfoBuilder {
	public:
		DebugInfoBuilder(Target target, SourcePositionsType source_positions_type);

		void setModulePath(std::string module_path) { info.module_path = std::move(module_path); }

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
		 * @brief Convenience overload: builds FunctionMetadata from name + position.
		 */
		FunctionBuilder beginFunction(
			std::string                    mangled_name,
			base::Optional<std::string>    function_name,
			base::Optional<SourcePosition> position
		);

		// ------------------------------------------------------------------

		/**
		 * @brief Finalizes and returns the accumulated DebugInfo.
		 */
		DebugInfo build();

	private:
		friend class FunctionBuilder;

		/// Called by FunctionBuilder::end() to commit the finished function.
		void finalizeFunction(std::string mangled_name, FunctionMetadata metadata);

		DebugInfo info;
	};

}  // namespace debug_info
