use super::Compiler;
use crate::{
    DuckCtx, QuackResult, StrId, quackpack::core::compile::compiler_package::CompilerPackage,
};
pub struct DefaultCompiler<'duck> {
    pub duck_ctx: &'duck DuckCtx,
}

impl<'duck> Compiler for DefaultCompiler<'duck> {
    fn compile_package(
        &self,
        package: &CompilerPackage,
        dependencies: &[&CompilerPackage],
        profile: StrId,
    ) -> QuackResult<()> {
        todo!()
    }
}
