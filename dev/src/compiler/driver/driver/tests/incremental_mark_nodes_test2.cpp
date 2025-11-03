#include <driver/initialize.hpp>
#include <frontend/module_tree/module_tree.hpp>

#include <artifacts/artifacts.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <driver/operations/generic_operations.hpp>
#include <tester/tester.hpp>
#include <filesystem>

using namespace compiler;

namespace { const char* k_artifacts_dir = "incremental_mark_nodes_artifacts"; }

class IncrementalMarkNodesTest2 final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS IncrementalMarkNodesTest2

public:
    TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(reinitAndVerifyGreen); }

private:
    void reinitAndVerifyGreen() {
        // Place artifacts under the build directory (CTest working dir)
        fs::FilePath artifacts_path = fs::FilePath(std::filesystem::current_path() / k_artifacts_dir);

        // Re-initialize compiler which will load the previous graph from artifacts
        compiler::driver::initializeTheCompiler(
            compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
                .main_package_info = {
                    .package_name = std::string("mark_nodes_test_package"),
                    .package_path = fs::FilePath(path("modules/functions_1")),
                },
                .compilation_artifacts = {.artifacts_path = artifacts_path},
                .debug_options         = {}
            }
        );

        // After initialization the previous graph (if present) should be loaded
        auto prev_opt = query::internal::ContextAccess::getState()->getPreviousGraph();
        assertTrue(prev_opt.has_value(), "Previous graph should be present after initialization");
        auto prev = prev_opt.value();

                // Verify node colors: previously-leaf nodes are green and dependency count checks hold
        auto prev_colors_opt = query::internal::ContextAccess::getState()->getPreviousNodeColors();
        ASSERT_TRUE(prev_colors_opt.has_value());
        auto prev_colors = prev_colors_opt.value();

        for (const auto& node: prev->getAllNodes()) {
            if (prev_colors->contains(node)) {
                ASSERT_TRUE(prev->getNodeDeps(node).size() == 1);
                ASSERT_TRUE(prev_colors->at(node) == query::internal::QueryState::PrevColor::Green);
            } else {
                ASSERT_TRUE(!node.q_id.hasStableHash() || prev->getNodeDeps(node).size() > 1);
            }
        }

        // Probably because of linker optimizations the INTERNAL_QUERY_IMPLEMENTATION_BOILERPLATE won't initialise without actually running a query
        auto module = frontend::createModuleTree(
            fs::File(path("modules/functions_2")), "mark_nodes_test_packag1e1e"
        );

        query::utils::withContextDo([&](query::Context& ctx) {
            (void) ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM });
        });

    }
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
