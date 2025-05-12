#pragma once

#include "profiler/profiler.hpp"
#include "reference_counter/reference_counter.hpp"
#include "services.hpp"

#include <base/optional.hpp>

#include <tuple>

namespace vm {
	class VMProcess;

	/**
	 * @brief Container for all services used by the VM.
	 *
	 * The services are initialized in the constructor and can be accessed using the `get` method.
	 *
	 * @tparam DynamicServices external, non-core services that can be added and removed.
	 */
	template<class... ServiceTypes>
	class ServiceManagerDef {
	private:
		using Services = std::tuple<base::Optional<ServiceTypes>...>;

		Services services;

	public:
		ServiceManagerDef(): services(base::Optional<ServiceTypes>()...) {}

		template<class T>
		T& get() {
			return std::get<base::Optional<T&>>(services).value();
		}

		/**
		 * @brief Checks if a service is enabled.
		 */
		template<class T>
		[[nodiscard]]
		bool isEnabled() const {
			return get<T>().hasValue();
		}

		/**
		 * @brief Enables (instantiates) a given service.
		 */
		template<class T>
		void enable() {
			auto&& service = get<T>();
			if (!service.has_value()) {
				service.emplace();
				service->init(*this);
			}
		}

		/**
		 * @brief Enables a (instantiates) given service with from a given object.
		 */
		template<class T>
		void enable(T&& srv) {
			auto&& service = get<T>();
			if (!service.has_value()) {
				service.emplace(std::forward<T>(srv));
				service->init(*this);
			}
		}

		/**
		 * @brief Disables (deletes) a given service.
		 */
		template<class T>
		void disable() {
			auto& service = get<T>();
			if (service.has_value()) service.reset();
		}
	};
}
