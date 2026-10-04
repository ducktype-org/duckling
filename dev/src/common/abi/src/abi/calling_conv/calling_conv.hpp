#pragma once
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
	class ArgInfo final {
	public:
		/**
		 * @brief We pass the value by pointer.
		 */
		struct ByPointer final {
			// If we want to pass the pointer with the `by_val` ptr attribute.
			// If not, we have to copy the value before the call and pass the pointer.
			bool by_val;
		};

		/**
		 * @brief We pass by value.
		 */
		struct ByValue final {
			/**
			 * @brief The type we coerce the original type to,
			 * can be the same as the original type.
			 */
			types::AbiType        coerce_to_type;
			[[maybe_unused]] bool sign_ext;
			[[maybe_unused]] bool zero_ext;
		};

		using Kind = std::variant<ByPointer, ByValue>;
		Kind kind;

		static ArgInfo byPointer(bool by_val);
		static ArgInfo byValue(types::AbiType type, bool sign_ext = false, bool zero_ext = false);

		template<typename Alternative>
		base::Optional<CRef<Alternative>> getKind() const {
			if (auto value = std::get_if<Alternative>(&kind)) return { CRef<Alternative>(value) };
			return {};
		}
	};

	struct ArgEntry final {
		ArgInfo            info;
		types::AbiTypeCRef original_type;
	};

	struct ReturnEntry final {
		ArgInfo info;

		/**
		 * @brief Whether the return value should be passed as first argument.
		 * Only relevant if a return value is passed by pointer.
		 * There may be more kind than just "ByPointer" in the future,
		 * so that's why it is here and in inside the "ByPointer" info.
		 */
		bool               passed_as_param;
		types::AbiTypeCRef original_type;
	};

	/**
	 * @brief The calling convention information
	 * to correctly construct LLVM C abi calls.
	 */
	struct FunctionInfo final {
		base::Optional<ReturnEntry> return_info;
		std::vector<ArgEntry>       param_info;

		/**
		 * @brief Number of fixed (non-variadic) parameters, empty optional for a
		 * non-variadic function. See `FunctionType::num_fixed_params`.
		 */
		base::Optional<usize> num_fixed_params;
	};

	/**
	 * @brief The function type.
	 */
	struct FunctionType final {
		/**
		 * @brief The return type, empty optional means function returns a void.
		 */
		base::Optional<types::AbiTypeCRef> return_type;
		std::vector<types::AbiTypeCRef>    param_types;

		/**
		 * @brief Number of fixed (non-variadic) parameters. An empty optional means the
		 * function is not variadic.
		 */
		base::Optional<usize> num_fixed_params{};
	};

	/**
	 * @brief Abstract class for constructing the `FunctionInfo`.
	 * Specialized by different architectures ABIs.
	 */
	class TargetInfo {
	public:
		virtual ~TargetInfo()                                                            = default;
		[[nodiscard]] virtual const TargetABI& myTargetABI() const                       = 0;
		[[nodiscard]] virtual FunctionInfo     computeInfo(const FunctionType& ft) const = 0;
	};

	/**
	 * @brief X86_64 ABI.
	 */
	class X86_64ABIInfo final: public TargetInfo {
	public:
		[[nodiscard]] const TargetABI& myTargetABI() const final { return x86_64Linux(); }

		[[nodiscard]] FunctionInfo computeInfo(const FunctionType& ft) const final;
	};

	/**
	 * @brief AArch64 ABI, as specified by AAPCS64 (aarch64-linux).
	 */
	class AArch64ABIInfo: public TargetInfo {
	public:
		[[nodiscard]] const TargetABI& myTargetABI() const override { return aarch64Linux(); }

		[[nodiscard]] FunctionInfo computeInfo(const FunctionType& ft) const final;

	protected:
		/**
		 * @brief Whether the caller has to sign/zero-extend sub-word integers to 32 bits.
		 */
		[[nodiscard]] virtual bool callerExtendsNarrowArgs() const { return false; }
	};

	/**
	 * @brief Apple arm64 ABI: AAPCS64 with Apple's deviations.
	 */
	class AArch64DarwinABIInfo final: public AArch64ABIInfo {
	public:
		[[nodiscard]] const TargetABI& myTargetABI() const final { return aarch64Darwin(); }

	protected:
		[[nodiscard]] bool callerExtendsNarrowArgs() const final { return true; }
	};

	/**
	 * @brief Main entry point. Calculate the calling convention info about
	 * a function based on the target and ABI types of a function.
	 */
	FunctionInfo computeCallingConv(const TargetABI& target, const FunctionType& ft);
}
