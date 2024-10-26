import argparse
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import os.path

def plot_regression_line(tokens_cpp, tokens_duck, output):
    regression_coef = np.linalg.lstsq(tokens_cpp[:, np.newaxis], tokens_duck)[0][0]
    
    fig, ax = plt.subplots(figsize=(12, 10))
    ax.grid()
    
    ax.scatter(tokens_cpp, tokens_duck, c='tab:blue')
    ax.plot([0, np.max(tokens_cpp)], [0, 0.7*np.max(tokens_cpp)], c='tab:gray', label=f'Target coeficcient: {0.7:.1%}')
    ax.plot([0, np.max(tokens_cpp)], [0, 1*np.max(tokens_cpp)], c='tab:green', label=f'x=x')
    ax.plot([0, np.max(tokens_cpp)], [0, regression_coef*np.max(tokens_cpp)], c='tab:red', label=f'Current coeficcient: {regression_coef:0.1%}')
    
    ax.set_title("Comparison of codelength (in tokens)\nC++ vs Duckling")
    ax.set_xlabel("Tokens in C++")
    ax.set_ylabel("Tokens in Duckling")
    ax.legend()
    
    plt.savefig(output)

def plot_percentage_distribution(percentages, output):
    fig, ax = plt.subplots(figsize=(12, 10))
    
    counts, _, _ = ax.hist(percentages, edgecolor='black')
    ax.vlines(np.mean(percentages), 0, np.max(counts), colors='tab:red', label=f'Average: {np.mean(percentages):.1%}')
    ax.vlines(np.median(percentages), 0, np.max(counts), linestyle='--', colors='tab:red', label=f'Median: {np.median(percentages):.1%}')
    
    ax.set_xlabel('Ratio of tokens needed in Duckling vs C++')
    ax.set_ylabel('Number of cases')
    ax.set_title('Summary of codelength ratio\nC++ vs Duckling')
    ax.legend()
    
    plt.savefig(output)

def main(data_file, output_dir):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    try:
        df = pd.read_csv(data_file)
    except Exception as e:
        print(e)
        
    tokens_cpp = df.iloc[:, 2].to_numpy()
    tokens_duck = df.iloc[:, 3].to_numpy()
    percentages = tokens_duck/tokens_cpp
    
    plot_regression_line(tokens_cpp, tokens_duck, os.path.join(output_dir, 'regression_line.png'))
    plot_percentage_distribution(percentages, os.path.join(output_dir, 'percentages.png'))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Plot the results of tokens comparison.')
    parser.add_argument('data_file', type=str, help='The file containing the data to plot')
    parser.add_argument('output_dir', type=str, help='The directory to output the plots to')
    args = parser.parse_args()
    main(args.data_file, args.output_dir)
