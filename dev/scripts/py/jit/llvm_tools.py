from dataclasses import dataclass
from click import option
import subprocess

@dataclass
class LLVMTool:
    name: str

    def default(self, version: str | None = None):
        no_prefix = ["opt"]

        suffix = f"-{version}" if version != None else ""
        prefix = "llvm-" if self.name not in no_prefix else ""
        return f"{prefix}{self.name}{suffix}"

    def macro(self):
        """The name of the parameter passed to CMake"""
        return "LLVM_" + self.name.upper()

    def param(self):
        """The name found in toolbox.py's kwargs"""
        return "llvm_" + self.name

    def option(self):
        """The name of the CLI option setting the tool"""
        return "--llvm-" + self.name

    def python_args(tool_paths):
        """This full list of options that would be passed to scripts.
           This makes it possible for any subsequent script to use any
           of the available tools, in the specified version."""
        option_list = [
            f"{tool.option()}={tool_path}"
            for (tool, tool_path) in zip(LLVM_TOOLS, tool_paths)
        ]
        return " ".join(option_list)


LLVM_TOOLS: list[LLVMTool] = [
    LLVMTool(tool_name) for tool_name in ["opt", "link", "nm", "cxxfilt", "extract", "readobj"]
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

def run_llvm_tool(tool: str, args: list[str], input: str | None = None, echo=False) -> str:
    if echo:
        print(tool, *args, sep=" ")
    result = subprocess.run([tool] + args, check=True, capture_output=True, text=True, input=input)
    return result.stdout
