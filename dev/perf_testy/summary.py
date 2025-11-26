# read results.csv and summarize data:

import os
import sys
import csv



def summarize_results(file_path):
    summary = {}
    with open(file_path, 'r', newline='', encoding='utf-8') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            category = row.get('Category', '').strip()
            test_case = row.get('Test Case', '').strip()
            language = row.get('Language', '').strip().lower()
            time_field = row.get('Compilation Time (ms)', '').strip()

            if not category or not test_case or not language or not time_field:
                assert False

            try:
                time_ms = int(time_field)
            except ValueError:
                assert False

            if category not in summary:
                summary[category] = {}

            if test_case not in summary[category]:
                summary[category][test_case] = {}
                
            summary[category][test_case][language] = time_ms
            
    return summary


def print_summary_for_category(summary):
    print("Performance Summary:")
    print(f"{'Test Case':<30} {'Duck Time (ms)':<15} {'C++ Time (ms)':<15} {'Duck/C++':<10}")
    coeffs = []
    duck_times = []
    cpp_times = []

    for test_case, times in sorted(summary.items()):
        duck_time = times.get('duck')
        cpp_time = times.get('cpp')

        duck_display = duck_time if duck_time is not None else 'N/A'
        cpp_display = cpp_time if cpp_time is not None else 'N/A'
        coeff_display = 'N/A'

        if duck_time is not None and cpp_time is not None and cpp_time != 0:
            coeff = float(duck_time) / float(cpp_time)
            coeffs.append(coeff)
            coeff_display = f"{coeff:.2f}"

        print(f"{test_case:<30} {duck_display!s:<15} {cpp_display!s:<15} {coeff_display:<10}")

        if duck_time is not None:
            duck_times.append(duck_time)
        if cpp_time is not None:
            cpp_times.append(cpp_time)

    # Averages
    avg_duck = sum(duck_times) / len(duck_times)
    avg_cpp = sum(cpp_times) / len(cpp_times)
    avg_coeff = sum(coeffs) / len(coeffs)

    print("\nAverages:")
    avg_duck_display = f"{avg_duck:.2f}" if avg_duck is not None else 'N/A'
    avg_cpp_display = f"{avg_cpp:.2f}" if avg_cpp is not None else 'N/A'
    avg_coeff_display = f"{avg_coeff:.2f}" if avg_coeff is not None else 'N/A'
    print(f"{'Average':<30} {avg_duck_display:<15} {avg_cpp_display:<15} {avg_coeff_display:<10}")

    print(f"Sum Duck / Sum C++: {sum(duck_times) / sum(cpp_times):.2f}")

def print_summary(summary):
    for category, cat_summary in summary.items():
        print(f"\nCategory: {category}")
        print_summary_for_category(cat_summary)

if __name__ == "__main__":
    # read first cli argument to get file:
    results_file = sys.argv[1]
    if os.path.exists(results_file):
        summary = summarize_results(results_file)
        print_summary(summary)
    else:
        print(f"Results file '{results_file}' not found.")
