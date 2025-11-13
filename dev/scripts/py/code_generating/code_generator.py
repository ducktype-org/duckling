import random

from typing import Literal, List
from abc import ABC, abstractmethod
from contextlib import contextmanager
from dataclasses import dataclass
from enum import Enum

from textwrap import indent

PROB = 30

def random_identifier(length: int) -> str:
    letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
    chars = "1234567890_"
    return random.choice(letters) + ''.join(random.choice(chars+letters) for _ in range(length-1))

@dataclass
class FunctionData:
    name: str
    args: int = 0

@dataclass
class ScopeData:
    vars: List[str] = []
    funcs: List[FunctionData] = []

class Indenter:
    def __init__(self, generator: 'CodeGenerator'):
        self.level = -1
        self.content = ""
        self.fragment = ""

        self.generator = generator

    def indent(self, text: str):
        for _ in range(self.level):
            text = indent(text, "\t")
        return text

    def add_full(self, text: str):
        self.content += self.indent(text)

    def add_fragment(self, text: str):
        self.fragment += text
    
    def flush_fragment(self):
        if len(self.fragment) > 0:
            self.content += self.indent(self.fragment)
            self.fragment = ""
    
    def flush(self):
        assert len(self.fragment) == 0, "Trying to flush text while writing a text fragment."
        if len(self.content) > 0:
            with open(self.generator.file_path, 'a') as f:
                f.write(self.content)
            self.generator.n_lines += len(self.content.splitlines())
            self.content = ""
    
    def __enter__(self):
        self.flush()
        self.level += 1
        return self
    
    def __exit__(self, exc_type, exc_value, traceback):
        self.flush()
        self.level -= 1

class CodeGenerator(ABC):
    def __init__(self, file_path: str, logic_generator: 'LogicGenerator'):
        self.file_path = file_path
        self.logic_generator = logic_generator
        self.indenter = self.Indenter(self)
        self.n_lines = 0

        # Clear the file.
        open(self.file_path, 'w').close()

    # Variables
    @abstractmethod
    def variable_declaration(self, var_name: str, scope: ScopeData, indent: int):
        pass

    @abstractmethod
    def constant_declaration(self, var_name: str, scope: ScopeData, indent: int):
        pass

    @abstractmethod
    def assignment(self, var_name: str, scope: ScopeData, indent: int):
        pass
    
    # Functions
    def return_(self, scope: ScopeData, indent:int):
        self.do_indent(indent)
        self.write("return ")
        self.logic_generator.generate_expression(scope, PROB)
        self.write(";\n")

    # Literals
    def int_literal(self, value: int):
        self.write(f"{value}")

    # Simple elements
    def operator(self, operator: str):
        self.write(f" {operator} ")
    
    # Special elements
    @abstractmethod
    def print(self, scope: ScopeData, indent: int):
        pass

    @abstractmethod
    @contextmanager
    def main_function(self):
        pass

    

class CppCodeGenerator(CodeGenerator):
    pass


class DucklingCodeGenerator(CodeGenerator):
    def variable_declaration(self, var_name: str, scope:list, indent: int):
        self.do_indent(indent)
        self.write(f"var {var_name}: i64 = ")
        self.logic_generator.generate_expression(scope, PROB)
        self.write(";\n")
        self.length += 1
    
    def constant_declaration(self, var_name: str, scope:list, indent: int) -> str:
        self.do_indent(indent)
        self.write(f"const {var_name}: i64 = ")
        self.logic_generator.generate_expression(scope, PROB)
        self.write(";\n")
        self.length += 1
        
    def assignment(self, var_name: str, scope:list, indent: int) -> str:
        self.do_indent(indent)
        self.write(f"{var_name} = ")
        self.logic_generator.generate_expression(scope, PROB)
        self.write(";\n")
        self.length += 1
        
    def print(self, scope:list, indent: int):
        self.do_indent(indent)
        self.write("builtin_output_i64(")
        self.logic_generator.generate_expression(scope, PROB)
        self.write(");\n")
        self.length += 1
    
    def return_(self, scope:list, indent:int) -> str:
        self.do_indent(indent)
        self.write("return ")
        self.logic_generator.generate_expression(scope, PROB)
        self.write(";\n")
        self.length += 1
    
    def generate_main_prefix(self):
        self.write("fun main() -> i64 = {\n")
            
    def generate_main_suffix(self, scope:list):
        self.generate_return(scope, 1)
        self.write("}\n")
            
    def generate_if_statement(self, scope:list, indent:int) -> str:
        self.do_indent(indent)
        self.write("if (")
        self.logic_generator.generate_conditional_expression(scope, PROB)
        self.write(") {\n")
        
        # Generate code inside if
        inner_scope = scope.copy()
        for _ in range(random.randint(1, 10)):
            self.logic_generator.generate_non_control_flow(inner_scope, indent+1)
        
        self.do_indent(indent)
        self.write("}\n")
        
        self.length += 2
        
    def generate_while_loop(self, scope:list, indent:int) -> str:
        
        self.do_indent(indent)
        self.write("while (")
        self.logic_generator.generate_conditional_expression(scope, PROB)
        self.write(") {\n")
        
        # Generate code inside while
        inner_scope = scope.copy()
        for _ in range(random.randint(1, 10)):
            self.logic_generator.generate_non_control_flow(inner_scope, indent+1)
            
        self.do_indent(indent)
        self.write("}\n")
        self.length += 2
    
    def generate_argument_list(self, length: int) -> str:
        args = []
        for i in range(length):
            arg_name = random_identifier(8)
            if i < length - 1:
                self.write(f"{arg_name}: i64, ")
            else:
                self.write(f"{arg_name}: i64")
            args.append(arg_name)
        return args
    
    def generate_function_definition(self, func_name: str, scope:list) -> str:
        self.write(f"fun {func_name}(")
        args = self.generate_argument_list(random.randint(0, 3))                    
        self.write(") -> i64 = {\n")
        # Generate code inside function
        inner_scope = scope.copy() + args
        for _ in range(random.randint(1, 10)):
            self.logic_generator.generate_non_control_flow(inner_scope, 1)
        self.generate_return(inner_scope, 1)
        self.write("}\n")
        self.length += 2
        
        return len(args)


class LogicGenerator:
    def __init__(self, generator: Literal["Duckling", "Cpp"], file_path: str, length:int, seed: int):
        self.generator = DucklingCodeGenerator(file_path, self) if generator == "Duckling" else CppCodeGenerator(file_path, self)
        self.seed = seed
        self.length = length
        self.functions = []
        random.seed(seed)        

    def generate_non_control_flow(self, scope: list, indent: int = 0):
        action = random.choices(
            ['declaration', 'assignment', 'print'],
            weights=[40, 40, 20],
            k=1
        )[0]
        if action == 'declaration':
            self.generate_variable_declaration(scope, indent)
        elif action == 'assignment':
            self.generate_assignment(scope, indent)
        elif action == 'print':
            self.generate_print(scope, indent)
            
    def generate_if_statement(self, scope: list, indent: int = 0):
        self.generator.generate_if_statement(scope, indent)
        
    def generate_while_loop(self, scope: list, indent: int = 0):
        self.generator.generate_while_loop(scope, indent)

    def generate_expression(self, scope: list, prob: int):
        op = False
        do = 0
        while do < prob/100 or not op:
            if op:
                self.generator.generate_operator(random.choice(['+', '-', '*', '/']))
                op = False
            else:
                if random.random() < 0.5 and len(scope) > 0:
                    self.generator.generate_symbol(random.choice(scope))
                else:
                    self.generator.generate_number_literal(random.randint(0, 1000))
                op = True
            do = random.random()
    
    def generate_conditional_expression(self, scope: list, prob: int):
        self.generate_expression(scope, prob)
        self.generator.generate_operator(random.choice(['==', '!=', '<', '>', '<=', '>=']))
        self.generate_expression(scope, prob)
    
    def generate_variable_declaration(self, scope: list, indent: int = 0):
        name = random_identifier(8)
        self.generator.variable_declaration(name, scope, indent)
        scope.append(name)
    
    def generate_constant_declaration(self, scope: list, indent: int = 0):
        name = random_identifier(8)
        self.generator.constant_declaration(name, scope, indent)
        scope.append(name)
        
    def generate_function_definition(self, scope: list):
        func_name = random_identifier(8)
        self.generator.generate_function_definition(func_name, scope)
        
    def generate_assignment(self, scope: list, indent: int = 0):
        if len(scope) == 0:
            return
        name = random.choice(scope)
        self.generator.generate_assignment(name, scope, indent)
        
    def generate_print(self, scope: list, indent: int = 0):
        self.generator.generate_print(scope, indent)
    
    def generate_return(self, scope: list, indent: int = 0):
        self.generator.generate_return(scope, indent)
        
    def generate_code(self):
        scope = []
        
        # Generate global symbols
        for _ in range(random.randint(1, 5)):
            action = random.choices(
                ['constant_declaration', 'function_definition'],
                weights=[50, 30],
                k=1
            )[0]
            if action == 'constant_declaration':
                self.generate_constant_declaration(scope, indent=0)
            elif action == 'function_definition':
                self.generate_function_definition(scope)
                self.generator.do_line_break()
        
        # Generate main function
        self.generator.do_line_break()
        self.generator.generate_main_prefix()
        while self.generator.length < self.length:
            action = random.choices(
                ['if_statement', 'while_loop', 'non_control_flow'],
                weights=[15, 15, 70],
                k=1
            )[0]
            if action == 'if_statement':
                self.generate_if_statement(scope, indent=1)
            elif action == 'while_loop':
                self.generate_while_loop(scope, indent=1)
            else:
                self.generate_non_control_flow(scope, indent=1)
        
        self.generator.generate_main_suffix(scope)
            

if __name__ == "__main__":
    file_path = "generated_code.duck"
    seed = random.randint(0, 1000000)
    print(f"Using seed: {seed}")
    logic_generator = LogicGenerator("Duckling", file_path, length=200, seed=seed)
    logic_generator.generate_code()
