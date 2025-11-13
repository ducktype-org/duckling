import random
from code_generator import LogicGenerator
from duckling_generator import DucklingCodeGenerator
from cpp_generator import CppCodeGenerator

if __name__ == "__main__":
    duck_path = "results/generated_code.duck"
    cpp_path = "results/generated_code.cpp"
    seed = random.randint(0, 1000000)
    
    print(f"Using seed: {seed}")
    
    duck_generator = LogicGenerator(DucklingCodeGenerator, duck_path, length=50, seed=seed)
    duck_generator.generate_code()
    
    cpp_generator = LogicGenerator(CppCodeGenerator, cpp_path, length=50, seed=seed)
    cpp_generator.generate_code()

