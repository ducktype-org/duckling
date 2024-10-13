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
        fieldnames = ['File', 'C++ Tokens', 'Duck Tokens', 'Duck Percentage of C++']
        writer = csv.DictWriter(csv_file, fieldnames=fieldnames)
        writer.writeheader()

        for file_name in os.listdir(features_dir):
            if file_name.endswith(".cpp"):
                base_name = file_name[:-4]
                cpp_file = os.path.join(features_dir, f"{base_name}.cpp")
                duck_file = os.path.join(features_dir, f"{base_name}.duck")

                if os.path.exists(duck_file):
                    count_cpp = count_tokens(cpp_file, cpp_command)
                    count_duck = count_tokens(duck_file, duck_command)
                    percentage_diff = (count_duck / count_cpp) * 100 if count_cpp != 0 else 0

                    writer.writerow({
                        'File': base_name,
                        'C++ Tokens': count_cpp,
                        'Duck Tokens': count_duck,
                        'Duck Percentage of C++': f"{percentage_diff:.2f}%"
                    })
    print(f"Output has been written to {output_csv}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Compare C++ and Duck files in a directory.')
    parser.add_argument('features_dir', type=str, help='The directory containing the feature files')
    parser.add_argument('--mode', type=str, help='The mode to open the file in', default="w")
    args = parser.parse_args()
    main(args.features_dir, args.mode)