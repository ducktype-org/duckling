import random

from contextlib import contextmanager

from code_generator import CodeGenerator, ScopeData, FunctionData, ModuleData, Indenter, LogicGenerator
from utils import PROB, random_identifier

class CppCodeGenerator(CodeGenerator):
    def __init__(self, file_path: str, logic_generator: LogicGenerator, import_mode: bool = False):
        super().__init__(file_path, logic_generator)
        self.global_flag = True
        if import_mode:
            self.hpp_indenter = Indenter(file_path.replace(".cpp", ".hpp"))

    def line_break(self):
        super().line_break()
        if hasattr(self, 'hpp_indenter'):
            self.hpp_indenter.add_text("\n")

    def get_module_name(self):
        return super().get_module_name().replace(".cpp", ".hpp") if hasattr(self, 'hpp_indenter') else super().get_module_name()

    # Variables
    def variable_declaration(self, var_name: str, scope: ScopeData):
        self.indenter.add_fragment(f"long {var_name} = ")
        self.logic_generator.generate_expression(scope, PROB)
        self.indenter.add_fragment(";\n")
        self.indenter.flush_fragment()
        
        if self.global_flag and hasattr(self, 'hpp_indenter'):
            self.hpp_indenter.add_text(f"extern long {var_name};\n")

    def constant_declaration(self, var_name: str, scope: ScopeData):
        self.indenter.add_fragment(f"const long {var_name} = ")
        self.logic_generator.generate_expression(scope, PROB)
        self.indenter.add_fragment(";\n")
        self.indenter.flush_fragment()
        
        if self.global_flag and hasattr(self, 'hpp_indenter'):
            self.hpp_indenter.add_text(f"extern const long {var_name};\n")

    def assignment(self, var_name: str, scope: ScopeData):
        self.indenter.add_fragment(f"{var_name} = ")
        self.logic_generator.generate_expression(scope, PROB)
        self.indenter.add_fragment(";\n")
        self.indenter.flush_fragment()
    
    # Functions
    def function_definition(self, func_name: str, scope: ScopeData) -> FunctionData:
        # Generate function signature
        self.indenter.add_fragment(f"long {func_name}(")
        args = self.argument_list(random.randint(0, 3))                    
        self.indenter.add_fragment(") {\n")
        self.indenter.flush_fragment()
        
        if hasattr(self, 'hpp_indenter'):
            self.hpp_indenter.add_text(f"long {func_name}(")
            for i, arg in enumerate(args):
                if i < len(args) - 1:
                    self.hpp_indenter.add_fragment(f"long {arg}, ")
                else:
                    self.hpp_indenter.add_fragment(f"long {arg}")
            self.hpp_indenter.add_fragment(");\n")
            self.hpp_indenter.flush_fragment()
        
        inner_scope = scope.copy()
        inner_scope.vars.extend(args)
        
        # Generate code function body
        self.global_flag = False
        self.logic_generator.generate_function_body(inner_scope)

        self.indenter.add_text("}\n")
        self.global_flag = True
        
        return FunctionData(func_name, len(args))
    
    def argument_list(self, length: int) -> str:
        args = []
        for i in range(length):
            arg_name = random_identifier(8)
            if i < length - 1:
                self.indenter.add_fragment(f"long {arg_name}, ")
            else:
                self.indenter.add_fragment(f"long {arg_name}")
            args.append(arg_name)
        return args
    
    def function_call(self, func: FunctionData, scope: ScopeData):
        self.indenter.add_fragment(func.name)
        self.indenter.add_fragment("(")
        for i in range(func.args):
            self.logic_generator.generate_expression(scope, PROB)
            if i < func.args - 1:
                self.indenter.add_fragment(", ")
        self.indenter.add_fragment(")")
    
    # Control flow
    def if_statement(self, scope: ScopeData):
        # Generate if signature
        self.indenter.add_fragment("if (")
        self.logic_generator.generate_conditional_expression(scope, PROB)
        self.indenter.add_fragment(") {\n")
        self.indenter.flush_fragment()
        
        inner_scope = scope.copy()
        
        # Generate if body
        with self.indenter:
            for _ in range(random.randint(1, 10)):
                self.logic_generator.generate_non_control_flow(inner_scope)
        
        self.indenter.add_text("}\n")
    
    def while_loop(self, scope: ScopeData):
        self.indenter.add_fragment("while (")
        self.logic_generator.generate_conditional_expression(scope, PROB)
        self.indenter.add_fragment(") {\n")
        self.indenter.flush_fragment()
        inner_scope = scope.copy()
        
        # Generate code inside while
        with self.indenter:
            for _ in range(random.randint(1, 10)):
                self.logic_generator.generate_non_control_flow(inner_scope)
                
        self.indenter.add_text("}\n")
    
    # Literals
    def int_literal(self, value: int):
        self.indenter.add_fragment(f"{value}ll")
    
    # Special elements
    def import_statement(self, module: ModuleData) -> ScopeData:
        self.indenter.add_text(f'#include "{module.path}"\n')
        if hasattr(self, 'hpp_indenter'):
            self.hpp_indenter.add_text(f'#include "{module.path}"\n')
        return module.symbols.copy()
    
    def preambule(self) -> ScopeData:
        if hasattr(self, 'hpp_indenter'):
            self.indenter.add_text(f'#include "{self.hpp_indenter.file_path}"\n')
            
        # self.indenter.add_text("#include <iostream>\n\n")
        self.indenter.add_text("void print(long n);\n")
        
        return ScopeData()
    
    def print(self, scope: ScopeData):
        # print mode:
        self.indenter.add_fragment("print(")
        self.logic_generator.generate_expression(scope, PROB)
        self.indenter.add_fragment(");\n")
       
        # cout mode:
        # self.indenter.add_fragment("std::cout << ")
        # self.logic_generator.generate_expression(scope, PROB)
        # self.indenter.add_fragment(";\n")
       
        self.indenter.flush_fragment()
        
    @contextmanager
    def main_function(self):
        # Generate main signature
        self.indenter.add_text("int main() {\n")
        self.global_flag = False
        
        # Generate main body elsewhere
        with self.indenter:
            yield        
            self.indenter.add_text("return 0;\n")
        
        # Close the function definition
        self.indenter.add_text("}\n")
        self.global_flag = True
