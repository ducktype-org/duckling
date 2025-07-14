#pragma once
#include <base/exceptions.hpp>

namespace vm::exceptions {
    class RunTimeException: public base::Exception {
        std::string message;
    public:
        explicit RunTimeException(const std::string& message);
		[[nodiscard]]
		const char* what() const noexcept override;
    };

    struct NullPointerAccessException: public RunTimeException {
        NullPointerAccessException();
    };

    struct NullPointCopyException: public RunTimeException {
        NullPointCopyException();
    };
    struct OutOfBlockBoundsException: public RunTimeException {
        OutOfBlockBoundsException();
    };
    struct DoubleFreeException: public RunTimeException {
        DoubleFreeException();
    };
}
