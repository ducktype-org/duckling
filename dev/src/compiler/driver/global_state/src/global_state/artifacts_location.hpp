#pragma once

#include <artifacts/artifacts.hpp>

namespace global_state {

	/**
	 * Returns the root collection of artifacts for the given compilation process.
	 * It is currently assumes that there no more then one main collection.
	 */
	Ref<artifacts::ArtifactCollection> getRootCollection();

	/**
	 * Returns true if the root collection has been set.
	 */
	bool hasRootCollection();

	namespace setters {
		/**
		 * This should only be called by the driver.
		 */
		void setRootCollection(Box<artifacts::ArtifactCollection>);
	}
}
