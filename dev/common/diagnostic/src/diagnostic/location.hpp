#pragma once

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
		virtual Ref<tokenizer::TokenFile> getSource() const
			= 0;

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
		Ref<tokenizer::TokenFile> getSource() const override;

		[[nodiscard]]
		fs::FilePath getSourceFile() const override;

		FileLocation(Ref<tokenizer::TokenFile> file, fs::FilePath path):
			  file(file),
			  path(std::move(path)) {}

	private:
		Ref<tokenizer::TokenFile> file;  ///< source file
		fs::FilePath              path;
	};

	/**
	 * @brief Macro expansion file location
	 */
	class MacroLocation final: public Location {
	protected:
		void printSuffixInfo(printer::PrinterOStream&) const override;

	public:
		[[nodiscard]]
		Ref<tokenizer::TokenFile> getSource() const override;

		fs::FilePath getSourceFile() const override;

		MacroLocation(const SourcePosition& parent, Ref<tokenizer::TokenFile> file);

	private:
		SourcePosition            parent;
		Ref<tokenizer::TokenFile> file;  ///< source file.
		fs::FilePath              path;
	};

	class FakeLocation final: public Location {
	private:
		FakeLocation() = default;

		static FakeLocation instance;

	protected:
		void printPrefixInfo(printer::PrinterOStream&) const override;

	public:
		[[nodiscard]]
		Ref<tokenizer::TokenFile> getSource() const override;

		fs::FilePath getSourceFile() const override;

		void printMessage(
			printer::PrinterOStream&,
			const SourcePosition&,
			const printer::PrinterContentsSeq& reason
		) const override;

		static Ref<FakeLocation> getInstance();
	};
}
