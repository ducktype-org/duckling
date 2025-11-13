import random
import atexit

from typing import Type, List
from abc import ABC, abstractmethod
from contextlib import contextmanager
from dataclasses import dataclass, field

from copy import deepcopy
from textwrap import indent

from utils import PROB, random_identifier

@dataclass
class FunctionData:
    name: str
    args: int = 0

@dataclass
class ScopeData:
    vars: List[str] = field(default_factory=list)
    funcs: List[FunctionData] = field(default_factory=list)

    def copy(self):
        return deepcopy(self)

class Indenter:
    def __init__(self, generator: 'CodeGenerator'):
        self.level = 0
        self.content = ""
        self.fragment = ""

        self.generator = generator
        atexit.register(self.flush)
        

    def indent(self, text: str):
        for _ in range(self.level):
            text = indent(text, "\t")
        return text

    def add_text(self, text: str):
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
        self.indenter = Indenter(self)
        self.n_lines = 0

        # Clear the file.
        open(self.file_path, 'w').close()

    def line_break(self):
        self.indenter.add_text("\n")

    # Variables
    @abstractmethod
    def variable_declaration(self, var_name: str, scope: ScopeData):
        pass

    @abstractmethod
    def constant_declaration(self, var_name: str, scope: ScopeData):
        pass

    @abstractmethod
    def assignment(self, var_name: str, scope: ScopeData):
        pass
    
    # Functions
    @abstractmethod
    def function_definition(self, func_name: str, scope: ScopeData) -> FunctionData:
        pass
    
    @abstractmethod
    def argument_list(self, length: int) -> str:
        pass
    
    def return_statement(self, scope: ScopeData):
        with self.indenter:
            self.indenter.add_fragment("return ")
            self.logic_generator.generate_expression(scope, PROB)
            self.indenter.add_fragment(";\n")
            self.indenter.flush_fragment()

    # Control flow
    @abstractmethod
    def if_statement(self, scope: ScopeData):
        pass
    
    @abstractmethod
    def while_loop(self, scope: ScopeData):
        pass
    
    # Literals
    def int_literal(self, value: int):
        self.indenter.add_fragment(f"{value}")

    # Simple elements
    def operator(self, operator: str):
        self.indenter.add_fragment(f" {operator} ")
    
    def symbol(self, value: str):
        self.indenter.add_fragment(value)
    
    # Special elements
    @abstractmethod
    def print(self, scope: ScopeData):
        pass

    @abstractmethod
    @contextmanager
    def main_function(self):
        pass


class LogicGenerator:
    def __init__(self, generator: Type[CodeGenerator], file_path: str, length:int, seed: int):
        self.generator = generator(file_path, self)
        self.seed = seed
        self.length = length
        random.seed(seed)        

    def generate_non_control_flow(self, scope: ScopeData):
        action = random.choices(
            ['declaration', 'assignment', 'print'],
            weights=[40, 40, 20],
            k=1
        )[0]
        if action == 'declaration':
            self.generate_variable_declaration(scope)
        elif action == 'assignment':
            self.generate_assignment(scope)
        elif action == 'print':
            self.generate_print(scope)
            
    def generate_if_statement(self, scope: ScopeData):
        self.generator.if_statement(scope)
        
    def generate_while_loop(self, scope: ScopeData):
        self.generator.while_loop(scope)

    def generate_expression(self, scope: ScopeData, prob: int):
        op = False
        do = 0
        while do < prob/100 or not op:
            if op:
                self.generator.operator(random.choice(['+', '-', '*', '/']))
                op = False
            else:
                if random.random() < 0.5 and len(scope.vars) > 0:
                    self.generator.symbol(random.choice(scope.vars))
                else:
                    self.generator.int_literal(random.randint(0, 1000))
                op = True
            do = random.random()
    
    def generate_conditional_expression(self, scope: ScopeData, prob: int):
        self.generate_expression(scope, prob)
        self.generator.operator(random.choice(['==', '!=', '<', '>', '<=', '>=']))
        self.generate_expression(scope, prob)
    
    def generate_variable_declaration(self, scope: ScopeData):
        name = random_identifier(8)
        self.generator.variable_declaration(name, scope)
        scope.vars.append(name)
    
    def generate_constant_declaration(self, scope: ScopeData):
        name = random_identifier(8)
        self.generator.constant_declaration(name, scope)
        scope.vars.append(name)
        
    def generate_function_definition(self, scope: ScopeData):
        func_name = random_identifier(8)
        scope.funcs.append(self.generator.function_definition(func_name, scope))
        
    def generate_assignment(self, scope: ScopeData):
        if len(scope.vars) == 0:
            return
        name = random.choice(scope.vars)
        self.generator.assignment(name, scope)
        
    def generate_print(self, scope: ScopeData):
        self.generator.print(scope)
    
    def generate_return(self, scope: ScopeData):
        self.generator.return_statement(scope)
        
    def generate_code(self):
        scope = ScopeData()
        
        # Generate global symbols
        for _ in range(random.randint(1, 5)):
            action = random.choices(
                ['constant_declaration', 'function_definition'],
                weights=[50, 30],
                k=1
            )[0]
            if action == 'constant_declaration':
                self.generate_constant_declaration(scope)
            elif action == 'function_definition':
                self.generate_function_definition(scope)
                self.generator.line_break()
        
        # Generate main function
        with self.generator.main_function():
            while self.generator.n_lines < self.length:
                action = random.choices(
                    ['if_statement', 'while_loop', 'non_control_flow'],
                    weights=[15, 15, 70],
                    k=1
                )[0]
                if action == 'if_statement':
                    self.generate_if_statement(scope)
                elif action == 'while_loop':
                    self.generate_while_loop(scope)
                else:
                    self.generate_non_control_flow(scope)

