#pragma once
#include "view_constructor.hpp"

#include <base/exceptions.hpp>

namespace dia_app {

	/**
	 * @brief Fetcher singleton.
	 *
	 * A layer of abstraction for the data transport between the compiler
	 * and the view manager.
	 */
	namespace fetcher {

		/**
		 * @brief Initialize the fetcher with the entire diagnostic file.
		 *
		 * @param diagnostic_file The diagnostic file in JSON format.
		 */
		void initialize(json&& diagnostic_file);

		/**
		 * @brief Get the number of info pointer_messages in the diagnostic file that
		 * the fetcher was initialized with.
		 *
		 * @return u32
		 */
		u32 getInfoGroupCount();

		/**
		 * @brief Create a new view constructor from the saved diagnostic file
		 * data at index `idx`.
		 *
		 * @param idx The index of the info group for this view constructor.
		 * @return ViewConstructor
		 */
		ViewConstructor generateViewConstructor(u32 idx);

		/**
		 * @brief Fetching related error.
		 */
		class FetchError: public base::Exception {
			std::string message;

		public:
			FetchError(std::string message);
			[[nodiscard]]
			const char* what() const noexcept override;
		};

		/**
		 * @brief Fetch the parametrization of the info identified by `id`.
		 *
		 * Throws FetchError upon failure.
		 *
		 * @param id The info ID.
		 * @return dia_file::InfoParams
		 */
		dia_file::InfoParams fetchInfo(InfoID id);

		/**
		 * @brief Fetch the entity identified by `id`.
		 *
		 * Throws FetchError upon failure.
		 *
		 * @param id The entity ID.
		 * @return dia_file::Entity
		 */
		dia_file::Entity fetchEntity(EntityID id);

		/**
		 * @brief Fetch the lazy element identified by `id`.
		 *
		 * Throws FetchError upon failure.
		 *
		 * @param id The element ID.
		 * @return dia_file::DisplayPtr
		 */
		dia_file::DisplayPtr fetchLazyElement(LazyDisplayID id);
	}
}
