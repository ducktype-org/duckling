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
class ClassData:
    name: str
    private_fields: List[str] = field(default_factory=list)
    public_fields: List[str] = field(default_factory=list)
    
    def get_interface(self) -> 'ScopeData':
        scope = ScopeData()
        scope.vars = self.public_fields.copy()
        return scope

@dataclass
class ScopeData:
    vars: List[str] = field(default_factory=list)
    funcs: List[FunctionData] = field(default_factory=list)
    classes: List[ClassData] = field(default_factory=list)

    def __iadd__(self, other: 'ScopeData'):
        self.vars += other.vars
        self.funcs += other.funcs
        self.classes += other.classes
        return self
    
    def copy(self):
        return deepcopy(self)
    
    def put_in_dot(self, module_name: str):
        for i in range(len(self.vars)):
            self.vars[i] = f"{module_name}.{self.vars[i]}"

        # hotfix: avoid renaming the same function multiple times,
        # when it is duplicated in the list.
        was_already_present = set()
        for i in range(len(self.funcs)):
            if id(self.funcs[i]) in was_already_present:
                continue
            was_already_present.add(id(self.funcs[i]))
           
            # print(f"> {i} Renaming function {self.funcs[i].name} to {module_name}.{self.funcs[i].name}")
            self.funcs[i].name = f"{module_name}.{self.funcs[i].name}"
        for i in range(len(self.classes)):
            self.classes[i].name = f"{module_name}.{self.classes[i].name}"

@dataclass
class ModuleData:
    path: str
    symbols: ScopeData

class Indenter:
    def __init__(self, file_path: str, clear: bool = True):
        self.level = 0
        self.content = ""
        self.fragment = ""
        
        self.file_path = file_path
        self.n_lines = 0

        atexit.register(self.flush)
        
        if clear:
            open(self.file_path, 'w').close()

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
            with open(self.file_path, 'a') as f:
                f.write(self.content)
            self.n_lines += len(self.content.splitlines())
            self.content = ""
    
    def __enter__(self):
        self.flush()
        self.level += 1
        return self
    
    def __exit__(self, exc_type, exc_value, traceback):
        self.flush()
        self.level -= 1

class CodeGenerator(ABC):
    def __init__(self, file_path: str, logic_generator: 'LogicGenerator', **kwargs):
        self.file_path = file_path
        self.logic_generator = logic_generator
        self.indenter = Indenter(self.file_path)

    def line_break(self):
        self.indenter.add_text("\n")
    
    def get_n_lines(self) -> int:
        return self.indenter.n_lines
    
    def get_module_name(self) -> str:
        return self.file_path
    
    # Variables
    @abstractmethod
    def variable_declaration(self, var_name: str, scope: ScopeData):
        assert False, "Trying to call abstract method."

    @abstractmethod
    def constant_declaration(self, var_name: str, scope: ScopeData):
        assert False, "Trying to call abstract method."

    @abstractmethod
    def assignment(self, var_name: str, scope: ScopeData):
        assert False, "Trying to call abstract method."
    
    # Functions
    @abstractmethod
    def function_definition(self, func_name: str, scope: ScopeData) -> FunctionData:
        assert False, "Trying to call abstract method."
    
    @abstractmethod
    def argument_list(self, length: int) -> str:
        assert False, "Trying to call abstract method."
    
    def return_statement(self, scope: ScopeData):
        self.indenter.add_fragment("return ")
        self.logic_generator.generate_expression(scope, PROB)
        self.indenter.add_fragment(";\n")
        self.indenter.flush_fragment()
    
    @abstractmethod
    def function_call(self, func: FunctionData, scope: ScopeData):
        assert False, "Trying to call abstract method."

    # Classes
    @abstractmethod
    def class_definition(self, class_name: str, scope: ScopeData) -> ClassData:
        assert False, "Trying to call abstract method."
    
    @abstractmethod
    def class_field(self, field_name: str, field_modifier: str) -> str:
        assert False, "Trying to call abstract method."

    @abstractmethod
    def object_instantiation(self, object_name: str, scope: ScopeData, class_: ClassData):
        assert False, "Trying to call abstract method."
    
    # Control flow
    @abstractmethod
    def if_statement(self, scope: ScopeData):
        assert False, "Trying to call abstract method."
    
    @abstractmethod
    def while_loop(self, scope: ScopeData):
        assert False, "Trying to call abstract method."
    
    # Literals
    @abstractmethod
    def int_literal(self, value: int):
        assert False, "Trying to call abstract method."

    # Simple elements
    def operator(self, operator: str):
        self.indenter.add_fragment(f" {operator} ")
    
    def symbol(self, value: str):
        self.indenter.add_fragment(value)
    
    # Special elements
    @abstractmethod
    def import_statement(self, module: ModuleData) -> ScopeData:
        assert False, "Trying to call abstract method."
    
    @abstractmethod
    def preambule(self) -> ScopeData:
        assert False, "Trying to call abstract method."
            
    @abstractmethod
    def print(self, scope: ScopeData):
        assert False, "Trying to call abstract method."

    @abstractmethod
    @contextmanager
    def main_function(self):
        assert False, "Trying to call abstract method."


class LogicGenerator:
    def __init__(self, generator: Type[CodeGenerator], file_path: str, length:int, seed: int, **kwargs):
        self.generator_type = generator
        self.generator = self.generator_type(file_path, self, **kwargs)
        self.seed = seed
        self.length = length
        random.seed(seed)        

    # Utils
    def get_module_name(self) -> str:
        return self.generator.get_module_name()

    # Definitons and declaratioins
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
        func_data = self.generator.function_definition(func_name, scope)
        scope.funcs.append(func_data)
        
    def generate_class_definition(self, scope: ScopeData):
        class_name = random_identifier(8)
        class_data = self.generator.class_definition(class_name, scope)
        scope.classes.append(class_data)
        
    def generate_class_fields(self, class_data: ClassData) -> ClassData:
        for _ in range(random.randint(1, 5)):
            field_name = random_identifier(8)
            field_modifier = random.choice(['private', 'public'])
            self.generator.class_field(field_name, field_modifier)
            if field_modifier == 'private':
                class_data.private_fields.append(field_name)
            else:
                class_data.public_fields.append(field_name)
            
        return class_data
            
    def generate_object_instantiation(self, scope: ScopeData):
        object_name = random_identifier(8)
        class_ = random.choice(scope.classes)
        self.generator.object_instantiation(object_name, scope, class_)
        new_objects = class_.get_interface()
        new_objects.put_in_dot(object_name)
        scope += new_objects
    
    # Control flow
    def generate_if_statement(self, scope: ScopeData):
        self.generator.if_statement(scope)
        
    def generate_while_loop(self, scope: ScopeData):
        self.generator.while_loop(scope)
    
    # Operations and expressions
    def generate_function_call(self, scope: ScopeData):
        if len(scope.funcs) == 0:
            return
        self.generator.function_call(random.choice(scope.funcs), scope)   
    
    def generate_expression(self, scope: ScopeData, prob: int, allow_function_calls: bool = True):
        op = False
        do = 0
        while do < prob/100 or not op:
            if op:
                self.generator.operator(random.choice(['+', '-', '*', '/']))
                op = False
            else:
                action = random.choices(
                    ['symbol', 'literal', 'function_call'],
                    weights=[70, 20, 20*int(allow_function_calls)],
                    k=1
                )[0]
                if action == 'symbol' and len(scope.vars) > 0:
                    self.generator.symbol(random.choice(scope.vars))
                elif action == 'function_call' and len(scope.funcs) > 0:
                    self.generate_function_call(scope)
                else:
                    self.generator.int_literal(random.randint(0, 1000))
                op = True
            do = random.random()
    
    def generate_conditional_expression(self, scope: ScopeData, prob: int):
        self.generate_expression(scope, prob)
        self.generator.operator(random.choice(['==', '!=', '<', '>', '<=', '>=']))
        self.generate_expression(scope, prob)
        
    def generate_assignment(self, scope: ScopeData):
        if len(scope.vars) == 0:
            return
        name = random.choice(scope.vars)
        self.generator.assignment(name, scope)
        
    def generate_print(self, scope: ScopeData):
        self.generator.print(scope)
    
    def generate_return(self, scope: ScopeData):
        self.generator.return_statement(scope)
    
    # Generation logic
    def generate_non_control_flow(self, scope: ScopeData):
        action = random.choices(
            ['declaration', 'assignment', 'new_object', 'print'],
            weights=[30, 40, 10, 20],
            k=1
        )[0]
        if action == 'declaration':
            self.generate_variable_declaration(scope)
        elif action == 'assignment':
            self.generate_assignment(scope)
        elif action == 'new_object' and len(scope.classes) > 0:
            self.generate_object_instantiation(scope)
        elif action == 'print':
            self.generate_print(scope)
        else:
            self.generate_non_control_flow(scope)
            
    def generate_function_body(self, scope: ScopeData):
        with self.generator.indenter:
            for _ in range(random.randint(1, 15)):
                action = random.choices(
                    ['if_statement', 'while_loop', 'non_control_flow'],
                    weights=[10, 10, 80],
                    k=1
                )[0]
                if action == 'if_statement':
                    self.generate_if_statement(scope)
                elif action == 'while_loop':
                    self.generate_while_loop(scope)
                else:
                    self.generate_non_control_flow(scope)
            self.generator.return_statement(scope)
            
    def generate_imports(self, imports: List[ModuleData]) -> ScopeData:
        combined_scope = ScopeData()
        for module in imports:
            module_scope = self.generator.import_statement(module)
            combined_scope += module_scope
        self.generator.line_break()
        return combined_scope
    
    def generate_global_symbols(self, scope: ScopeData, global_symbol_count: int):
        for _ in range(global_symbol_count):
            action = random.choices(
                ['variable_declaration', 'function_definition', 'class_definition'],
                weights=[10, 70, 20],
                k=1
            )[0]
            if action == 'variable_declaration':
                self.generate_variable_declaration(scope)
            elif action == 'function_definition':
                self.generate_function_definition(scope)
                self.generator.line_break()
            elif action == 'class_definition':
                self.generate_class_definition(scope)
                self.generator.line_break()
        
        return scope

    def generate_file(self, global_symbol_count: int, imports: List[ModuleData]):
        scope = self.generator.preambule()
        scope += self.generate_imports(imports)
        scope += self.generate_global_symbols(scope, global_symbol_count)
        
        return ModuleData(self.generator.get_module_name(), scope)
    
    def generate_dependencies(self, imports_count: int) -> List[ModuleData]:
        dependencies = []
        
        for i in range(imports_count):
            path_fragments = self.generator.file_path.split('.')
            path_fragments[-2] += f"_import_{i}"
            
            tmp_generator = LogicGenerator(
                generator=self.generator_type,
                file_path='.'.join(path_fragments),
                length=self.length//2,
                seed=self.seed + i,
                import_mode=True)
            
            module = tmp_generator.generate_file(global_symbol_count=100, imports=[])
            dependencies.append(module)
        
        return dependencies
    
    def generate_main_file(self, global_symbol_count: int, imports_count: int):
        scope = self.generator.preambule()
        imports = self.generate_dependencies(imports_count)
        scope += self.generate_imports(imports)
        scope += self.generate_global_symbols(scope, global_symbol_count)
        
        # Generate main function
        with self.generator.main_function():
            while self.generator.get_n_lines() < self.length:
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

# TODOs:
# 1. Only assign to modifiable value
# 2. Declare vars without starting value as well
# 3. Classes
