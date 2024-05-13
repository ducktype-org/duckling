#pragma once

#include "message.hpp"

namespace dia {
	class DiagnosticToUserConverter {
	public:
		/**
		 * @brief Convert a Message to a printer::PrinterContent sequence ready to be printed for
		 * the user to view.
		 * @param message_ptr The Message to convert.
		 * @param detailed Whether to include more details than the diagnostic minimum. These
		 * details may include extra information about the source or nature of the error and may be
		 * used to explain the error to beginners.
		 * @return A printer::PrinterContentsSeq ready to be printed for the user.
		 */
		[[nodiscard]]
		static printer::PrinterContentsSeq
			toPrinterContents(base::c_borrow_ptr<Message> message_ptr, bool detailed);

		/**
		 * @brief Convert a Note to a printer::PrinterContent sequence ready to be printed for
		 * the user to view.
		 * @param note_ptr The Note to convert.
		 * @param detailed Whether to include more details than the diagnostic minimum. These
		 * details may include extra information about the source or nature of the error and may be
		 * used to explain the error to beginners.
		 * @return A printer::PrinterContentsSeq ready to be printed for the user.
		 */
		[[nodiscard]]
		static printer::PrinterContentsSeq
			toPrinterContents(base::c_borrow_ptr<Note> note_ptr, bool detailed);
	};

	static_assert(DiagnosticToStringConverter<DiagnosticToUserConverter>);
}
