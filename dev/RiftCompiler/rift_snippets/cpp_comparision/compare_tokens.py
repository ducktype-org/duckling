import os
import subprocess
import csv
import argparse

def count_tokens(file_path, command):
    result = subprocess.run([command, file_path], capture_output=True, text=True)
    return int(result.stdout.strip())

def main(features_dir, mode):
    cpp_command = "./bin/count_tokens_cpp"
    duck_command = "./bin/count_tokens_duckling"
    output_csv = "comparison_results.csv"

    with open(output_csv, mode=mode, newline='') as csv_file:
        fieldnames = ['File', 'Category', 'C++ Tokens', 'Duck Tokens', 'Duck Percentage of C++']
        writer = csv.DictWriter(csv_file, fieldnames=fieldnames)
        writer.writeheader()

        for root, _, files in os.walk(features_dir):
            for file_name in files:
                if file_name.endswith(".cpp"):
                    base_name = file_name[:-4]
                    cpp_file = os.path.join(root, f"{base_name}.cpp")
                    duck_file = os.path.join(root, f"{base_name}.duck")

                    if os.path.exists(duck_file):
                        count_cpp = count_tokens(cpp_file, cpp_command)
                        count_duck = count_tokens(duck_file, duck_command)
                        percentage_diff = (count_duck / count_cpp) * 100 if count_cpp != 0 else 0
                        innermost_folder = os.path.basename(os.path.dirname(cpp_file))

                        writer.writerow({
                            'File': base_name,
                            'Category': innermost_folder,
                            'C++ Tokens': count_cpp,
                            'Duck Tokens': count_duck,
                            'Duck Percentage of C++': f"{percentage_diff:.2f}%"
                        })

    print(f"Output has been written to {output_csv}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Compare C++ and Duck files in a directory.')
    parser.add_argument('features_dir', type=str, nargs='?', help='The directory containing the feature files, default = \"features/\"', default="features/")
    parser.add_argument('--mode', type=str, help='The mode to open the file in', default="w")
    args = parser.parse_args()
    main(args.features_dir, args.mode)