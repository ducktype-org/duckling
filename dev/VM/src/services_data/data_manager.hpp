#pragma once

#include <tuple>
#include <base/ints.hpp>
#include <base/optional.hpp>
#include <core/process/memory/memory.hpp>
#include <core/process/type_metadata/type_metadata.hpp>

namespace vm {

	/**
	 * @brief Container for the data used by the VM services.
	 * @warning DEPRECATED
	 */
	template<class... DynamicData>
	class DataManagerDef {
	private:
		template<class T>
		using Dynamic = std::pair<base::Optional<T>, u64>;
		template<class T>
		using DynamicRef         = std::pair<base::Optional<T>&, u64&>;
		using DynamicDataStorage = std::tuple<Dynamic<DynamicData>...>;

		template<class T>
		using IsCoreData = std::
			integral_constant<bool, std::is_same_v<Memory, T> || std::is_same_v<TypeMetadata, T>>;

		Memory       memory;
		TypeMetadata typeMetadata;

		DynamicDataStorage dynamicData;

		template<class T>
		requires(!IsCoreData<T>::value) DynamicRef<T> get() {
			Dynamic<T>& data = std::get<Dynamic<T>>(dynamicData);
			return DynamicRef(data.first, data.second);
		}

	public:
		DataManagerDef(): dynamicData(Dynamic(base::Optional<DynamicData>(), 0)...) {}

		template<class T>
		requires std::is_same_v<Memory, T> T& get() {
			return memory;
		}

		template<class T>
		requires std::is_same_v<TypeMetadata, T> T& get() {
			return typeMetadata;
		}

		/**
		 * Get a dynamic data of type T. If dynamic data does not exist, it creates one first.
		 * Each call to require should be followed by exactly one call to release.
		 */
		template<class T>
		requires(!IsCoreData<T>::value) T& require() {
			auto [data, references] = get<T>();
			if (references == 0) data = some<T>(T());
			references++;
			return data.value();
		}

		/**
		 * Release a dynamic data of type T. If dynamic data does not have any more references, it
		 * is removed.
		 */
		template<class T>
		requires(!IsCoreData<T>::value) void release() {
			auto [data, references] = get<T>();
			if (references == 1) data = base::Optional<T>();
			if (references > 0) references--;
		}
	};
}
