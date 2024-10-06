#pragma once

#include <tuple>
#include <base/optional.hpp>
#include "services.hpp"
#include "reference_counter/reference_counter.hpp"
#include "profiler/profiler.hpp"

namespace vm {
	class VCPU;

	/**
	 * @brief Container for all services used by the VM.
	 * 
	 * The services are initialized in the constructor and can be accessed using the `get` method.
	 * 
	 * @tparam DynamicServices external, non-core services that can be added and removed.
	 */
	template<class... DynamicServices>
	class ServiceManagerDef {
	private:
		using DynamicServicesStorage = std::tuple<base::Optional<DynamicServices>...>;

		template<class T>
		using IsCoreService = std::integral_constant<
			bool,
			std::is_same_v<Allocator, T> || std::is_same_v<StackAllocator, T>
				|| std::is_same_v<Executor, T> || std::is_same_v<Preprocessor, T>>;

		VCPU& vcpu;

		Allocator      allocator;
		StackAllocator stackAllocator;
		Executor       executor;
		Preprocessor   preprocessor;

		DynamicServicesStorage dynamic_services;

	public:
		ServiceManagerDef(VCPU& _vcpu):
			  vcpu(_vcpu),
			  allocator(*this),
			  stackAllocator(*this),
			  executor(*this),
			  preprocessor(*this),
			  dynamic_services(base::Optional<DynamicServices>()...) {}

		VCPU& getVCPU() { return vcpu; }

		template<std::same_as<Allocator> T>
		T& get() {
			return allocator;
		}

		template<std::same_as<StackAllocator> T>
		T& get() {
			return stackAllocator;
		}

		template<std::same_as<Executor> T>
		T& get() {
			return executor;
		}

		template<std::same_as<Preprocessor> T>
		T& get() {
			return preprocessor;
		}

		template<class T>
		requires(!IsCoreService<T>::value) T& get() {
			return std::get<base::Optional<T&>>(dynamic_services).value();
		}

		// The following are mostly for dynamic services
		template<class T>
		requires IsCoreService<T>::value [[nodiscard]]
		bool isAvailable() const {
			return true;
		}

		template<class T>
		requires(!IsCoreService<T>::value) [[nodiscard]]
		bool isAvailable() const {
			return get<T>().hasValue();
		}

		template<class T>
		requires(!IsCoreService<T>::value) void enable() {
			auto& service = get<T>();
			if (!service.has_value()) {
				service.emplace();
				service->init(*this);
			}
		}

		template<class T>
		requires(!IsCoreService<T>::value) void disable() {
			auto& service = get<T>();
			if (service.has_value()) service.reset();
		}
	};
}
