#pragma once

#include "diagnostic_interactive/stable_position.hpp"
#include "location_types.hpp"
#include "source_position.hpp"

#include <base/pointers/box.hpp>

#include <filesystem/file.hpp>
#include <printer/printer_content.hpp>

#include <utility>

namespace dia {
	/**
	 * @brief General location of a source position.
	 *
	 * For example may be file location or macro expand location.
	 */
	class Location {
	public:
		/**
		 * @brief Returns the file connected to the location.
		 */
		[[nodiscard]]
		virtual fs::File getSourceFile() const
			= 0;

		/**
		 * @brief Returns the file connected to the location.
		 */
		[[nodiscard]]
		virtual Ref<tokenizer::TokenSource> getSource() const
			= 0;

		[[nodiscard]]
		virtual LocationType getLocationType() const
			= 0;


		virtual ~Location() = default;
	};

	/**
	 * @brief Minimal file location
	 */
	class FileLocation final: public Location {
	public:
		[[nodiscard]]
		Ref<tokenizer::TokenSource> getSource() const override;

		[[nodiscard]]
		fs::File getSourceFile() const override;

		[[nodiscard]]
		LocationType getLocationType() const override {
			return LocationType::FileLocationType;
		}

		FileLocation(Ref<tokenizer::TokenSource> source, fs::File path):
			  source(source),
			  path(std::move(path)) {}

	private:
		Ref<tokenizer::TokenSource> source;  ///< Source of tokens.
		fs::File                    path;    ///< Path to the original file.
	};

	/**
	 * @brief Macro expansion file location
	 */
	class MacroLocation final: public Location {
	public:
		[[nodiscard]]
		Ref<tokenizer::TokenSource> getSource() const override;

		[[nodiscard]]
		fs::File getSourceFile() const override;

		[[nodiscard]]
		LocationType getLocationType() const override {
			return LocationType::MacroLocationType;
		}

		MacroLocation(const dia_int::StablePosition& parent, Ref<tokenizer::TokenSource> source);

	private:
		dia_int::StablePosition     parent;
		Ref<tokenizer::TokenSource> source;  ///< Source of tokens.
		fs::File                    path;    ///< Path to original file
	};

	class FakeLocation final: public Location {
	private:
		FakeLocation();

		fs::File                    virtual_file;
		Box<tokenizer::TokenSource> source;

	public:
		[[nodiscard]]
		Ref<tokenizer::TokenSource> getSource() const override;

		[[nodiscard]]
		fs::File getSourceFile() const override;

		[[nodiscard]]
		LocationType getLocationType() const override {
			return LocationType::FakeLocationType;
		}

		static Ref<FakeLocation> getInstance();
	};
}
