/**
 * @file version.hpp
 * @brief Version and build information shared by every Duckling binary.
 *
 * The values come from the build system: the semantic version and the fixed build facts are
 * compile definitions on the `Version` module, the commit date is regenerated on every build
 * (see `GenerateCommitInfo.cmake`). Every getter returns a view into static storage, so nothing
 * here allocates and the returned views stay valid for the whole run.
 */

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace version {

	/**
	 * @brief One extra `label: value` row for renderVerbose().
	 *
	 * This is how a binary adds a fact only it knows about (duckc adds its LLVM version, for
	 * example) without the Version module having to depend on that fact.
	 */
	using ExtraField = std::pair<std::string_view, std::string_view>;

	/**
	 * @name Semantic version
	 * @{
	 */
	/** @brief The full semantic version, including the pre-release suffix, e.g. "0.0.2-alpha". */
	[[nodiscard]]
	std::string_view semver();
	/** @brief The major component of the semantic version. */
	[[nodiscard]]
	std::string_view semverMajor();
	/** @brief The minor component of the semantic version. */
	[[nodiscard]]
	std::string_view semverMinor();
	/** @brief The patch component of the semantic version. */
	[[nodiscard]]
	std::string_view semverPatch();
	/** @brief The pre-release suffix without its dash, e.g. "alpha". Empty on a final release. */
	[[nodiscard]]
	std::string_view prerelease();
	/** @brief Whether this is a pre-release build, i.e. whether prerelease() is non-empty. */
	[[nodiscard]]
	bool isPrerelease();
	/**@}*/

	/**
	 * @name Build identity
	 * @{
	 */
	/**
	 * @brief Date of the commit the binary was built from, as "YYYY-MM-DD".
	 * @return An empty view when the sources were not built from a git worktree.
	 */
	[[nodiscard]]
	std::string_view commitDate();
	/** @brief Whether tracked files were modified relative to that commit when the build ran. */
	[[nodiscard]]
	bool isDirty();
	/**@}*/

	/**
	 * @name Fixed build facts
	 * @{
	 */
	/** @brief The CMake build type the binary was configured with, e.g. "DevDebug". */
	[[nodiscard]]
	std::string_view buildType();
	/** @brief The operating system the binary was built on, e.g. "Linux". */
	[[nodiscard]]
	std::string_view hostSystem();
	/** @brief The processor the binary was built on, e.g. "x86_64". */
	[[nodiscard]]
	std::string_view hostProcessor();
	/** @brief The compiler the binary was built with, e.g. "GNU". */
	[[nodiscard]]
	std::string_view compilerId();
	/** @brief The version of that compiler, e.g. "14.2.0". */
	[[nodiscard]]
	std::string_view compilerVersion();
	/** @brief The name of the licence Duckling ships under. Empty until one is chosen. */
	[[nodiscard]]
	std::string_view licenseName();
	/** @brief The copyright line to show next to the licence. Empty until one is chosen. */
	[[nodiscard]]
	std::string_view copyrightLine();
	/**@}*/

	/**
	 * @brief Renders the single line printed by `--version`.
	 *
	 * The shape is `<tool_name> <semver> (<commit date>)`, for example
	 * `duckc 0.0.2-alpha (2026-09-15)`. The parenthesised group is left out when the commit date
	 * is unknown, and the date carries a `-dirty` suffix when the worktree was modified.
	 *
	 * @param tool_name The name of the binary, e.g. "duckc".
	 * @return The rendered line, without a trailing newline.
	 */
	[[nodiscard]]
	std::string renderShort(std::string_view tool_name);

	/**
	 * @brief Renders the block printed by `--version-verbose`.
	 *
	 * The first line is renderShort(), followed by one `label: value` line per known build fact
	 * and then one per entry of @p extra. All values are aligned to the same column, and fields
	 * with an empty value are left out.
	 *
	 * @param tool_name The name of the binary, e.g. "duckc".
	 * @param extra Binary-specific rows to append, in the order they should be printed.
	 * @return The rendered block, without a trailing newline.
	 */
	[[nodiscard]]
	std::string renderVerbose(std::string_view tool_name, std::span<const ExtraField> extra = {});

}  // namespace version
