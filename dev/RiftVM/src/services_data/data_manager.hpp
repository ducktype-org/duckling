#pragma once

#include "memory/memory.hpp"
#include "type_metadata/type_metadata.hpp"

#include <base/ints.hpp>
#include <base/option.hpp>
#include <concepts>
#include <tuple>

namespace vm {
	template<class... DynamicData>
	class DataManagerDef {
	private:
		template<class T>
		using Dynamic = std::pair<option<T>, u64>;
		template<class T>
		using DynamicRef         = std::pair<option<T>&, u64&>;
		using DynamicDataStorage = std::tuple<Dynamic<DynamicData>...>;

		template<class T>
		using IsCoreData = std::integral_constant<bool, std::is_same_v<Memory, T>
		                                                    || std::is_same_v<TypeMetadata, T>>;

		Memory       memory;
		TypeMetadata typeMetadata;

		DynamicDataStorage dynamicData;

		template<class T>
		requires(!IsCoreData<T>::value)
		DynamicRef<T> get() {
			Dynamic<T>& data = std::get<Dynamic<T>>(dynamicData);
			return DynamicRef(data.first, data.second);
		}

	public:
		DataManagerDef(): dynamicData(Dynamic(none<DynamicData>(), 0)...) {}

		template<class T>
		requires std::is_same_v<Memory, T>
		T& get() {
			return memory;
		}

		template<class T>
		requires std::is_same_v<TypeMetadata, T>
		T& get() {
			return typeMetadata;
		}

		/**
		 * Get a dynamic data of type T. If dynamic data does not exist, it creates one first.
		 * Each call to require should be followed by exactly one call to release.
		 */
		template<class T>
		requires(!IsCoreData<T>::value)
		T& require() {
			auto [data, references] = get<T>();
			if (references == 0) {
				data = some<T>(T());
			}
			references++;
			return data.value();
		}

		/**
		 * Release a dynamic data of type T. If dynamic data does not have any more references, it
		 * is removed.
		 */
		template<class T>
		requires(!IsCoreData<T>::value)
		void release() {
			auto [data, references] = get<T>();
			if (references == 1) {
				data = none<T>();
			}
			if (references > 0) {
				references--;
			}
		}
	};
}
