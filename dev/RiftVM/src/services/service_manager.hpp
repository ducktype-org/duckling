#pragma once

#include "profiler/profiler.hpp"
#include "reference_counter/reference_counter.hpp"
#include "services.hpp"
#include <base/option.hpp>
#include <tuple>

namespace vm {
	class VCPU;

	template<class... DynamicServices>
	class ServiceManagerDef {
	private:
		using DynamicServicesStorage = std::tuple<option<DynamicServices>...>;

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
			  dynamic_services(none<DynamicServices>()...) {}

		VCPU& getVCPU() { return vcpu; }

		template<class T>
		requires std::is_same_v<Allocator, T>
		T& get() {
			return allocator;
		}

		template<class T>
		requires std::is_same_v<StackAllocator, T>
		T& get() {
			return stackAllocator;
		}

		template<class T>
		requires std::is_same_v<Executor, T>
		T& get() {
			return executor;
		}

		template<class T>
		requires std::is_same_v<Preprocessor, T>
		T& get() {
			return preprocessor;
		}

		template<class T>
		requires(!IsCoreService<T>::value)
		T& get() {
			return std::get<option<T>>(dynamic_services).value();
		}

		// The following are mostly for dynamic services
		template<class T>
		requires IsCoreService<T>::value
		bool isAvailable() const {
			return true;
		}

		template<class T>
		requires(!IsCoreService<T>::value)
		bool isAvailable() const {
			return get<T>().hasValue();
		}

		template<class T>
		requires(!IsCoreService<T>::value)
		void enable() {
			auto& service = get<T>();
			if (!service.has_value()) {
				service.emplace();
				service->init(*this);
			}
		}

		template<class T>
		requires(!IsCoreService<T>::value)
		void disable() {
			auto& service = get<T>();
			if (service.has_value()) service.reset();
		}
	};
}
