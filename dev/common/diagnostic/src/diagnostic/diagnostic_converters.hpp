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
		 * @param parent_message The parent message of the Note to convert.
		 * @param detailed Whether to include more details than the diagnostic minimum. These
		 * details may include extra information about the source or nature of the error and may be
		 * used to explain the error to beginners.
		 * @return A printer::PrinterContentsSeq ready to be printed for the user.
		 */
		[[nodiscard]]
		static printer::PrinterContentsSeq toPrinterContents(
			base::c_borrow_ptr<Note>    note_ptr,
			base::c_borrow_ptr<Message> parent_message,
			bool                        detailed
		);

		template<std::ranges::input_range R> 
		requires std::same_as<base::c_borrow_ptr<Message>, std::ranges::range_value_t<R>>
		[[nodiscard]]
		static printer::PrinterContentsSeq listToPrinterContents(
			R    range,
			bool detailed
		) {
			std::vector<printer::PrinterContentsSeq> res;
			for (base::c_borrow_ptr<Message> message: range) {
				res.push_back(toPrinterContents(message, detailed));
				res.push_back({{"\n\n"}});
			}
			auto view = std::ranges::join_view(res);
			return {view.begin(), view.end()};
		}

	};

	static_assert(DiagnosticToPrinterConverter<DiagnosticToUserConverter>);

	class DiagnosticToJSONConverter {
	public:
		/**
		 * @brief Convert a Message to a printer::PrinterContent sequence representing an
		 * LSP Diagnostic in JSON format, according to the specification here:
		 * https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnostic
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
		 * @brief Convert a Note to a printer::PrinterContent sequence representing an
		 * LSP Diagnostic in JSON format, according to the specification here:
		 * https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticRelatedInformation
		 * @param note_ptr The Note to convert.
		 * @param parent_message The parent message of the Note to convert.
		 * @param detailed Whether to include more details than the diagnostic minimum. These
		 * details may include extra information about the source or nature of the error and may be
		 * used to explain the error to beginners.
		 * @return A printer::PrinterContentsSeq ready to be printed for the user.
		 */
		[[nodiscard]]
		static printer::PrinterContentsSeq toPrinterContents(
			base::c_borrow_ptr<Note>    note_ptr,
			base::c_borrow_ptr<Message> parent_message,
			bool                        detailed
		);

		template<std::ranges::input_range R> 
		requires std::same_as<base::c_borrow_ptr<Message>, std::ranges::range_value_t<R>>
		[[nodiscard]]
		static printer::PrinterContentsSeq listToPrinterContents(
			R    range,
			bool detailed
		) {
			std::vector<printer::PrinterContentsSeq> res;
			res.push_back({{"{ \"messages\":[\n"}});
			bool first = true;
			for (base::c_borrow_ptr<Message> message: range) {
				if (first) first = false;
				else res.push_back({{",\n"}});
				res.push_back(toPrinterContents(message, detailed));
			}
			res.push_back({{"]}"}});
			auto view = std::ranges::join_view(res);
			return {view.begin(), view.end()};
		}
	};

	static_assert(DiagnosticToPrinterConverter<DiagnosticToJSONConverter>);
}
