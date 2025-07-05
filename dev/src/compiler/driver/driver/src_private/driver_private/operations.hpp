#pragma once

#include <artifacts/artifacts.hpp>
#include <helios/hout/hout.hpp>

#include <base/box.hpp>
#include <base/ref.hpp>
#include <base/string_id.hpp>

#include <expected>
#include <driver/backend_type.hpp>

namespace driver {
    /**
     * @brief Compiles the HOUTUnit to the backend module.
     */
    void compileHOUTUnit(
        query::Context&              ctx,
        base::CRef<compiler::helios::HOUTUnit> hout_unit,
        base::StrID                  module_id,
        artifacts::FileArtifact      output_artifact,
        BackendType backend_type
    );


    // todo PR: move elsewhere
    /**
    * Compile builtin LLVM library into an object file.
    */
    artifacts::FileArtifact emitBuiltinObjectFile();
}