#include "exceptions.hpp"

namespace vm::exceptions {
    RunTimeException::RunTimeException(const std::string& message): message(message) {}

    const char* RunTimeException::what() const noexcept {
        return message.c_str();
    }

    NullPointerAccessException::NullPointerAccessException():
        RunTimeException("Tried to access a null pointer") {}

    NullPointCopyException::NullPointCopyException():
        RunTimeException("Tried to copy a null pointer") {}

    OutOfBlockBoundsException::OutOfBlockBoundsException():
        RunTimeException("Tried to access memory out of block bounds") {}

    DoubleFreeException::DoubleFreeException():
        RunTimeException("Tried to free a block that was already freed") {}
}
