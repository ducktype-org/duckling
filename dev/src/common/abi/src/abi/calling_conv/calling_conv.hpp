/**
 * @file calling_conv.hpp
 * @author Wojciech Rzeplinski
 */
#include <abi/target.hpp>
#include <abi/type_system/type.hpp>

#include <variant>

/**
 * @brief Calling convention library for the C/C++ calling convention for the LLVM.
 * Meant as our replacement of the
 * https://github.com/llvm/llvm-project/tree/main/llvm/include/llvm/ABI library. Once the LLVM
 * version supports for AARCH64 and Windows we could replace it. We don't support: C++ conventions,
 * unions, vector registers, win32, packed structs (but the logic is written in a way the
 * packed struct are supported very easily).
 */
namespace abi::calling_conv {
	class ArgInfo {
	public:
		struct ByPointer {
			// If we want to pass the pointer with the `by_val` ptr attribute.
			// If not, we have to copy the
			bool by_val;
		};

		struct ByValue {
			types::AbiType coerce_to_type;
			bool           sign_ext;
			bool           zero_ext;
		};

		using Kind = std::variant<ByPointer, ByValue>;
		Kind kind;

		static ArgInfo byPointer(bool by_val);
		static ArgInfo byValue(types::AbiType type, bool sign_ext = false, bool zero_ext = false);

		template<typename Alternative>
		base::Optional<CRef<Alternative>> getKind() {
			if (auto value = std::get_if<Alternative>(&kind)) return { CRef<Alternative>(value) };
			return {};
		}
	};

	struct ArgEntry {
		ArgInfo            info;
		types::AbiTypeCRef original_type;
	};

	struct ReturnEntry {
		ArgInfo            info;
		bool               passed_as_param;
		types::AbiTypeCRef original_type;
	};

	struct FunctionInfo {
		ReturnEntry           return_info;
		std::vector<ArgEntry> param_info;
	};

	struct FunctionType {
		types::AbiTypeCRef              return_type;
		std::vector<types::AbiTypeCRef> param_types;
	};

	class TargetInfo {
	public:
		virtual ~TargetInfo()                                                            = default;
		[[nodiscard]] virtual const TargetABI& myTargetABI() const                       = 0;
		[[nodiscard]] virtual FunctionInfo     computeInfo(const FunctionType& ft) const = 0;
	};

	class X86_64ABIInfo final: public TargetInfo {
	public:
		[[nodiscard]] const TargetABI& myTargetABI() const final { return x86_64Linux(); }

		[[nodiscard]] FunctionInfo computeInfo(const FunctionType& ft) const final;
	};

	class AArch64ABIInfo final: public TargetInfo {
	public:
		[[nodiscard]] const TargetABI& myTargetABI() const final { return aarch64Linux(); }

		[[nodiscard]] FunctionInfo computeInfo(const FunctionType& ft) const final;
	};

	/**
	 * @brief Main entry point. Calculate the calling convention info about
	 * a function based on the target and ABI types of a function.
	 */
	FunctionInfo computeCallingConv(const TargetABI& target, const FunctionType& ft);
}
