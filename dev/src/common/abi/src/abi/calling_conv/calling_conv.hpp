#include <abi/target.hpp>
#include <abi/type_system/type.hpp>

#include <variant>

/**
 * @brief Calling convention library.
 * Meant as our replacement of the
 * https://github.com/llvm/llvm-project/tree/main/llvm/include/llvm/ABI library. Once the LLVM
 * version supports for AARCH64 and Windows we could replace it. We don't support: C++ conventions,
 * unions, vector registers, win32, packed structs.
 */
namespace abi::calling_conv {
	enum class CallingConv { C };

	class ArgInfo {
	public:
		struct ByPointer {
			// If we want to pass the pointer with the `by_val` ptr attribute.
			// If not, we have to copy the
			bool by_val;
		};

		struct ByValue {
			type_system::AbiType coerce_to_type;
			bool                 sign_ext;
			bool                 zero_ext;
		};

		using Kind = std::variant<ByPointer, ByValue>;
		Kind kind;

		static ArgInfo byPointer(bool by_val);
		static ArgInfo byValue(
			type_system::AbiType type, bool sign_ext = false, bool zero_ext = false
		);
	};

	struct ArgEntry {
		ArgInfo                 info;
		type_system::AbiTypePtr original_type;
	};

	struct ReturnEntry {
		ArgInfo                 info;
		type_system::AbiTypePtr original_type;
		bool                    passed_as_param;
	};

	class FunctionInfo {
		ReturnEntry           return_info;
		std::vector<ArgEntry> param_info;
	};

	class FunctionType {
		type_system::AbiTypePtr              return_type;
		std::vector<type_system::AbiTypePtr> param_types;
	};

	class TargetInfo {
	public:
		virtual ~TargetInfo()                                                            = default;
		[[nodiscard]] virtual const TargetABI& myTargetABI() const                       = 0;
		[[nodiscard]] virtual FunctionInfo     computeInfo(const FunctionType& ft) const = 0;
	};

	class X86_64ABIInfo final: public TargetInfo {
		[[nodiscard]] const TargetABI& myTargetABI() const final { return x86_64Linux(); }

		[[nodiscard]] FunctionInfo computeInfo(const FunctionType& ft) const final;
	};

	class AArch64ABIInfo final: public TargetInfo {
		[[nodiscard]] const TargetABI& myTargetABI() const final { return aarch64Linux(); }

		[[nodiscard]] FunctionInfo computeInfo(const FunctionType& ft) const final;
	};
}
