from dataclasses import dataclass
from click import option

@dataclass
class LLVMTool:
    name: str

    def default(self, version: str | None = None):
        no_prefix = ["opt"]

        suffix = f"-{version}" if version != None else ""
        prefix = "llvm-" if self.name not in no_prefix else ""
        return f"{prefix}{self.name}{suffix}"

    def macro(self):
        return "LLVM_" + self.name.upper()

    def param(self):
        return "llvm_" + self.name

    def option(self):
        return "--llvm-" + self.name

    def python_args(tool_paths):
        option_list = [
            f"{tool.option()}={tool_path}"
            for (tool, tool_path) in zip(LLVM_TOOLS, tool_paths)
        ]
        return " ".join(option_list)


LLVM_TOOLS: list[LLVMTool] = [
    LLVMTool(tool_name) for tool_name in ["opt", "link", "nm", "cxxfilt", "extract"]
]

def llvm_tools_version_options(func):
    """Dynamically adds a family of --llvm-<tool>-version options."""
    for tool in LLVM_TOOLS:
        func = option(
            f"{tool.option()}",
            type=str,
            metavar="PATH",
            help=f"Path to {tool.default()}"
        )(func)
    return func
