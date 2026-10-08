// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "printer_content.hpp"

namespace printer {
	/**
	 * @brief Simple stream printer for efficient building of printer contents.
	 */
	class PrinterOStream {
	private:
		std::vector<PrinterContent> contents;

	public:
		PrinterOStream() = default;

		PrinterOStream& operator<<(const PrinterContent& added) {
			contents.push_back(added);
			return *this;
		}

		PrinterOStream& add(const PrinterContent& added) {
			contents.push_back(added);
			return *this;
		}

		PrinterOStream& operator<<(const PrinterContentsSeq& added) {
			for (const auto& content: added) *this << content;
			return *this;
		}

		[[nodiscard]]
		std::vector<PrinterContent> getContents() const {
			return { contents.begin(), contents.end() };
		}
	};
}
