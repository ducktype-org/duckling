import random
from code_generator import LogicGenerator
from duckling_generator import DucklingCodeGenerator

if __name__ == "__main__":
    file_path = "generated_code.duck"
    seed = random.randint(0, 1000000)
    print(f"Using seed: {seed}")
    logic_generator = LogicGenerator(DucklingCodeGenerator, file_path, length=50, seed=0)
    logic_generator.generate_code()
