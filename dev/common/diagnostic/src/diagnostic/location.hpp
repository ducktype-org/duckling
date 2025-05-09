#pragma once

#include "location_types.hpp"
#include "source_position.hpp"

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
	protected:
		/**
		 * @brief Information added at the begining of an error/message.
		 *
		 * Normally information about the source file
		 */
		virtual void printPrefixInfo(printer::PrinterOStream&) const;

		/**
		 * @brief Information added at the end.
		 *
		 * Normally nothing, may be expansion information.
		 */
		virtual void printSuffixInfo(printer::PrinterOStream&) const;

	public:
		/**
		 * @brief Returns the file connected to the location.
		 */
		[[nodiscard]]
		virtual fs::FilePath getSourceFile() const
			= 0;

		/**
		 * @brief Returns the file connected to the location.
		 */
		[[nodiscard]]
		virtual Ref<tokenizer::TokenSource> getSource() const
			= 0;

		virtual LocationType getLocationType() const = 0;

		/**
		 * @brief Generate an error message.
		 */
		virtual void printMessage(
			printer::PrinterOStream&,
			const SourcePosition&              pos,
			const printer::PrinterContentsSeq& reason
		) const;

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
		fs::FilePath getSourceFile() const override;

		[[nodiscard]]
		LocationType getLocationType() const override {
			return LocationType::FileLocationType;
		}

		FileLocation(Ref<tokenizer::TokenSource> source, fs::FilePath path):
			  source(source),
			  path(std::move(path)) {}

	private:
		Ref<tokenizer::TokenSource> source;  ///< Source of tokens.
		fs::FilePath              path; ///< Path to the original file.
	};

	/**
	 * @brief Macro expansion file location
	 */
	class MacroLocation final: public Location {
	protected:
		void printSuffixInfo(printer::PrinterOStream&) const override;

	public:
		[[nodiscard]]
		Ref<tokenizer::TokenSource> getSource() const override;

		[[nodiscard]]
		fs::FilePath getSourceFile() const override;

		[[nodiscard]]
		LocationType getLocationType() const override {
			return LocationType::MacroLocationType;
		}

		MacroLocation(const SourcePosition& parent, Ref<tokenizer::TokenSource> file);

	private:
		SourcePosition            parent;
		Ref<tokenizer::TokenSource> source;  ///< Source of tokens.
		fs::FilePath              path; ///< Path to original file
	};

	class FakeLocation final: public Location {
	private:
		FakeLocation() = default;

		static FakeLocation instance;

	protected:
		void printPrefixInfo(printer::PrinterOStream&) const override;

	public:
		[[nodiscard]]
		Ref<tokenizer::TokenSource> getSource() const override;

		[[nodiscard]]
		fs::FilePath getSourceFile() const override;

		[[nodiscard]]
		LocationType getLocationType() const override {
			return LocationType::FakeLocationType;
		}

		void printMessage(
			printer::PrinterOStream&,
			const SourcePosition&,
			const printer::PrinterContentsSeq& reason
		) const override;

		static Ref<FakeLocation> getInstance();
	};
}
