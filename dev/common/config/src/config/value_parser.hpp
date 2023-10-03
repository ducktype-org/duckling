#pragma once

#include <any>
#include <base/exceptions.hpp>
#include <base/raw_view.hpp>
#include <base/unique_pointer.hpp>

namespace config {

	struct WrongParamValue: public base::LogicError {
		WrongParamValue(std::string message): LogicError(message){};
	};

	/**
	 * @brief This interface represent
	 * parsing behavior, and reality
	 * only hold v-pointer, which is
	 * a wrapper for passing functions
	 */
	class ValueParser {
	public:
		virtual std::any    parse(base::RawView data) = 0;
		virtual std::string helperMess()              = 0;
		virtual ~ValueParser()                        = default;
	};

	struct StringParser: public ValueParser {
		std::any    parse(base::RawView data) override;
		std::string helperMess() override;
	};

	struct IntParser: public ValueParser {
		std::any    parse(base::RawView data);
		std::string helperMess();
	};

	template<typename T>
	base::unique_ptr<T> makeParser() {
		return base::make_unique<T>();
	}

};  // namespace config
