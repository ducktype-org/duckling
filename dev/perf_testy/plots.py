import argparse
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import os.path

def plot_regression_line(df, output):
    tokens_cpp, tokens_duck = df.cpp.to_numpy(), df.duck.to_numpy()
    regression_coef = np.linalg.lstsq(tokens_cpp[:, np.newaxis], tokens_duck)[0][0]
    
    fig, ax = plt.subplots(figsize=(12, 10))
    ax.grid()

    ax.plot([0, np.max(tokens_cpp)], [0, 0.7*np.max(tokens_cpp)], c='tab:gray', label=f'Target coeficcient: {0.7:.1%}')
    ax.plot([0, np.max(tokens_cpp)], [0, 1*np.max(tokens_cpp)], c='tab:gray', linestyle='--', label=f'Base coeficcient: {1:.1%}')
    ax.plot([0, np.max(tokens_cpp)], [0, regression_coef*np.max(tokens_cpp)], c='tab:red', label=f'Current coeficcient: {regression_coef:0.1%}')
    
    ax.scatter(tokens_cpp, tokens_duck, c='tab:blue')
    
    ax.set_title("Comparison of codelength (in tokens)\nC++ vs Duckling")
    ax.set_xlabel("Tokens in C++")
    ax.set_ylabel("Tokens in Duckling")
    ax.legend()
    
    plt.savefig(output)

def plot_percentage_distribution(df, output):
    percentages = (df.duck/df.cpp).to_numpy()
    
    fig, ax = plt.subplots(figsize=(12, 10))
    
    counts, _, _ = ax.hist(percentages, edgecolor='black')
    ax.axvline(x=np.mean(percentages), color='tab:red', label=f'Average: {np.mean(percentages):.1%}')
    ax.axvline(x=np.median(percentages), linestyle='--', color='tab:red', label=f'Median: {np.median(percentages):.1%}')
    
    ax.set_xlabel('Ratio of tokens needed in Duckling vs C++')
    ax.set_ylabel('Number of cases')
    ax.set_title('Summary of codelength ratio\nC++ vs Duckling')
    ax.legend()
    
    plt.savefig(output)

def plot_box_categories(df, output):
    data = []
    for category in df.category.unique():
        tmp = df[df.category == category]
        data.append((tmp.duck/tmp.cpp).to_numpy(dtype=np.float32))
        
    fig, ax = plt.subplots(figsize=(12, 10))

    ax.boxplot(data, tick_labels=df.category.unique(),
            medianprops=dict(linewidth=1.5, linestyle='-', color='black'))

    ax.axhline(y=0.7, color='tab:red', linestyle='--', label='Target')
    ax.axhline(y=(df.duck/df.cpp).mean(), color='tab:gray', linestyle='--', label='Mean')

    for i, points, category in zip(range(1, len(data)+1), data, df.category.unique()):
        ax.scatter(np.random.normal(i, 0.05, len(points)), points, alpha=0.6, color='tab:blue')
        

    ax.set_xlabel('Category of the example')
    ax.set_ylabel('Ratio of tokens needed in Duckling vs C++')
    ax.set_title('Codelength ratio by category\nC++ vs Duckling')
    ax.legend()
    
    plt.savefig(output)

def main(data_file, output_dir):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    try:
        df = pd.read_csv(data_file)
        df.rename(columns={
            "File": "file",
            "Category": "category",
            "C++ Tokens": "cpp",
            "Duck Tokens": "duck"
        }, inplace=True)
        df = df[["cpp", "duck", "file", "category"]]
    except Exception as e:
        print(e)
    
    plot_regression_line(df, os.path.join(output_dir, 'regression_line.png'))
    plot_percentage_distribution(df, os.path.join(output_dir, 'percentages.png'))
    plot_box_categories(df, os.path.join(output_dir, 'categories.png'))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Plot the results of perf tests.')
    parser.add_argument('data_file', type=str, help='The file containing the data to plot')
    parser.add_argument('output_dir', type=str, help='The directory to output the plots to')
    args = parser.parse_args()
    main(args.data_file, args.output_dir)

    