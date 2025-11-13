import random

from contextlib import contextmanager

from code_generator import CodeGenerator, ScopeData, FunctionData
from utils import PROB, random_identifier

class CppCodeGenerator(CodeGenerator):
    # Variables
    def variable_declaration(self, var_name: str, scope: ScopeData):
        self.indenter.add_fragment(f"int {var_name} = ")
        self.logic_generator.generate_expression(scope, PROB)
        self.indenter.add_fragment(";\n")
        self.indenter.flush_fragment()

    def constant_declaration(self, var_name: str, scope: ScopeData):
        self.indenter.add_fragment(f"const int {var_name} = ")
        self.logic_generator.generate_expression(scope, PROB)
        self.indenter.add_fragment(";\n")
        self.indenter.flush_fragment()

    def assignment(self, var_name: str, scope: ScopeData):
        self.indenter.add_fragment(f"{var_name} = ")
        self.logic_generator.generate_expression(scope, PROB)
        self.indenter.add_fragment(";\n")
        self.indenter.flush_fragment()
    
    # Functions
    def function_definition(self, func_name: str, scope: ScopeData) -> FunctionData:
        # Generate function signature
        self.indenter.add_fragment(f"int {func_name}(")
        args = self.argument_list(random.randint(0, 3))                    
        self.indenter.add_fragment(") {\n")
        self.indenter.flush_fragment()
        
        inner_scope = scope.copy()
        inner_scope.vars.extend(args)
        
        # Generate code function body
        with self.indenter:
            for _ in range(random.randint(1, 10)):
                self.logic_generator.generate_non_control_flow(inner_scope)
            self.return_statement(inner_scope)
        self.indenter.add_text("}\n")
        
        return FunctionData(func_name, len(args))
    
    def argument_list(self, length: int) -> str:
        args = []
        for i in range(length):
            arg_name = random_identifier(8)
            if i < length - 1:
                self.indenter.add_fragment(f"int {arg_name}, ")
            else:
                self.indenter.add_fragment(f"int {arg_name}")
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
    
    # Special elements
    def preambule(self) -> ScopeData:
        self.indenter.add_text("#include <iostream>\n\n")
        return ScopeData()
    
    def print(self, scope: ScopeData):
        self.indenter.add_fragment("std::cout << ")
        self.logic_generator.generate_expression(scope, PROB)
        self.indenter.add_fragment(";\n")
        self.indenter.flush_fragment()
        
    @contextmanager
    def main_function(self):
        # Generate main signature
        self.indenter.add_text("int main() {\n")
        
        # Generate main body elsewhere
        with self.indenter:
            yield        
            self.indenter.add_text("return 0;\n")
        
        # Close the function definition
        self.indenter.add_text("}\n")
