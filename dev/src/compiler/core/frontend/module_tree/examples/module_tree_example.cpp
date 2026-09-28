#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>

#include <init/init.hpp>
#include <query_framework/entry/with_context_do.hpp>

#include <iostream>

int main() {
	init::InitObject _;

	using compiler::frontend::getFileRef;
	using compiler::frontend::ModuleTree;

	// First argument is some kind of a path to a module we want to parse.
	// It returns a std::shared_ptr.
	Ref<ModuleTree> module_tree = compiler::frontend::ModuleTreeBuilder::create(
		fs::File("../tests/test_module"), base::StrID("test_package_id")
	);

	base::Optional<base::CRef<compiler::frontend::SourceFile>> main_source;
	std::vector<base::CRef<ModuleTree>>                        submodules;

	// Use a query context so that unlock() can register SideInputs in the dependency graph.
	query::utils::withContextDo([&](query::Context& ctx) {
		if (module_tree->hasMainSourceFile()) {
			const auto main_file_access = module_tree->getMainSourceFile().unlock(ctx);
			main_source                 = getFileRef(main_file_access.getID());
		}

		// Each unlock(ctx) emits a QueryModuleSideInput edge so incremental rebuilds know what changed.
		for (const auto& submodule_locked: module_tree->getSubmodules().unlock(ctx)) {
			const auto submodule_access = submodule_locked.unlock(ctx);
			submodules.emplace_back(getModuleRef(submodule_access.getID()));
		}
	});

	// Outside of the query context we can safely inspect the unlocked resources.
	if (main_source.has_value())
		std::cout << main_source.value()->getFileIllegalAccess().getContent().view().stringView()
				  << '\n';

	// Everything that is not the module's own source is kept by extension, outside the query
	// graph, so it needs no context to be read.
	for (const auto& [extension, files]: module_tree->getOtherFiles())
		for (const auto& file: files)
			std::cout << extension.strView() << ": " << file.getContent().view().stringView()
					  << '\n';

	for (const auto& submodule_ref: submodules)
		std::cout << submodule_ref->getName().strView() << " has "
				  << submodule_ref->getOtherFiles().size() << " kind(s) of other file" << '\n';
}
