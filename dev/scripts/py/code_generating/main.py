import random
import argparse
from code_generator import LogicGenerator
from duckling_generator import DucklingCodeGenerator
from cpp_generator import CppCodeGenerator


if __name__ == "__main__":

    # add cli arguments for paths, length, and seed
    parser = argparse.ArgumentParser(description="Generate code in Duckling and C++.")
    parser.add_argument("--duck_path", type=str, default="results/generated_code.duck", help
                        ="Path to save the generated Duckling code.")
    parser.add_argument("--cpp_path", type=str, default="results/generated_code.cpp", help
                        ="Path to save the generated C++ code.")
    parser.add_argument("--length", type=int, default=50, help="Length of the generated code.")
    parser.add_argument("--global_symbol_count", type=int, default=1, help="Number of global symbols to generate (apart from main).")
    parser.add_argument("--seed", type=int, default=None, help="Random seed for code generation.")
    args = parser.parse_args()

    duck_path = args.duck_path
    cpp_path = args.cpp_path
    length = args.length
    global_symbol_count = args.global_symbol_count
    seed = args.seed if args.seed is not None else random.randint(0, 1000000)
    
    print(f"Using seed: {seed}")
    
    duck_generator = LogicGenerator(DucklingCodeGenerator, duck_path, length=length, seed=seed)
    duck_generator.generate_code(global_symbol_count=global_symbol_count)
    
    cpp_generator = LogicGenerator(CppCodeGenerator, cpp_path, length=length, seed=seed)
    cpp_generator.generate_code(global_symbol_count=global_symbol_count)

