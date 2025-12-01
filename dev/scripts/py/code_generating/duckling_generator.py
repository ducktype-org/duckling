import random
import random

from contextlib import contextmanager

from code_generator import CodeGenerator, ScopeData, FunctionData, ModuleData, ClassData
from utils import PROB, random_identifier

class DucklingCodeGenerator(CodeGenerator):

    def int_literal(self, value: int):
        self.indenter.add_fragment(f"{value}i64")

        
    # Variables
    def variable_declaration(self, var_name: str, scope: ScopeData):
        self.indenter.add_fragment(f"var {var_name}: i64 = ")
        self.logic_generator.generate_expression(scope, PROB)
        self.indenter.add_fragment(";\n")
        self.indenter.flush_fragment()
    
    def constant_declaration(self, var_name: str, scope: ScopeData):
        self.indenter.add_fragment(f"const {var_name}: i64 = ")
        self.logic_generator.generate_expression(scope, PROB)
        self.indenter.add_fragment(";\n")
        self.indenter.flush_fragment()
        
    def assignment(self, var_name: str, scope: ScopeData) -> str:
        self.indenter.add_fragment(f"{var_name} = ")
        self.logic_generator.generate_expression(scope, PROB)
        self.indenter.add_fragment(";\n")
        self.indenter.flush_fragment()
        
    # Functions
    def function_definition(self, func_name: str, scope: ScopeData) -> FunctionData:
        # Generate function signature
        self.indenter.add_fragment(f"fun {func_name}(")
        args = self.argument_list(random.randint(0, 3))                    
        self.indenter.add_fragment(") -> i64 = {\n")
        self.indenter.flush_fragment()
        
        inner_scope = scope.copy()
        inner_scope.vars.extend(args)
        
        # Generate code function body
        self.logic_generator.generate_function_body(inner_scope)

        
        self.indenter.add_text("}\n")
        
        return FunctionData(func_name, len(args))
    
    def argument_list(self, length: int) -> str:
        args = []
        for i in range(length):
            arg_name = random_identifier(8)
            if i < length - 1:
                self.indenter.add_fragment(f"{arg_name}: i64, ")
            else:
                self.indenter.add_fragment(f"{arg_name}: i64")
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
    
    # Classes
    def class_definition(self, class_name: str, scope: ScopeData) -> ScopeData:
        self.indenter.add_text(f"class {class_name}" + " {\n")
        class_data = ClassData(name=class_name)
        
        with self.indenter:
            class_data = self.logic_generator.generate_class_fields(class_data)
        
        self.indenter.add_text("}\n")
        return class_data
    
    def class_field(self, field_name: str, field_modifier: str) -> str:
        self.indenter.add_text(f"{field_modifier} var {field_name}: i64;\n")
    
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

    # Special elements
    def import_statement(self, module: ModuleData) -> ScopeData:
        module_name = module.path.split(".")[-2].split("/")[-1]
        self.indenter.add_text(f'import {module_name} as {module_name};\n')
        result_scope = module.symbols.copy()
        result_scope.put_in_dot(module_name)
        return result_scope
    
    def preambule(self) -> ScopeData:
        return ScopeData()
    
    def print(self, scope: ScopeData):
        self.indenter.add_fragment("builtin_output_i64(")
        self.logic_generator.generate_expression(scope, PROB)
        self.indenter.add_fragment(");\n")
        self.indenter.flush_fragment()

    @contextmanager
    def main_function(self):
        # Generate main signature
        self.indenter.add_text("fun main() -> i64 = {\n")
        
        # Generate main body elsewhere
        with self.indenter:
            yield        
            self.indenter.add_text("return 0i64;\n")
        
        # Close the function definition
        self.indenter.add_text("}\n")
