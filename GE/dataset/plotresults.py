import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

def generate_gp_analysis(file_path):
    # 1. Load the dataset
    try:
        df = pd.read_csv(file_path)
    except FileNotFoundError:
        print(f"Error: {file_path} not found.")
        return

    # 2. Data Pre-processing
    # Extract Algorithm Type from seed name
    df['Algorithm'] = df['seed'].apply(lambda x: 'Canonical' if 'canonical' in str(x).lower() else 'SBGP')

    # Assign the correct column for Runtime (runtimeS is the seconds column)
    df['ActualRuntime'] = df['runtimeS']

    # Sort the data by population size for clean plotting order
    df = df.sort_values('popSize')

    # 3. Plotting Configuration
    sns.set_theme(style="whitegrid")
    palette = {'Canonical': '#34495e', 'SBGP': '#e74c3c'}
    hue_order = ['Canonical', 'SBGP']

    # --- Figure 1: Test BEST MSE ---
    plt.figure(figsize=(10, 6))
    sns.barplot(data=df, x='popSize', y='testBestMSE', hue='Algorithm', 
                hue_order=hue_order, palette=palette, 
                errorbar='sd', capsize=.1)
    plt.title('Average Test BEST MSE', fontsize=14, fontweight='bold')
    plt.ylabel('Mean MSE (Error Bars: STDEV.S)')
    plt.xlabel('Population Size')

    # --- Figure 2: Test MEDIAN MSE (Log Scale) ---
    plt.figure(figsize=(10, 6))
    sns.barplot(data=df, x='popSize', y='testMedianMSE', hue='Algorithm', 
                hue_order=hue_order, palette=palette, 
                errorbar='sd', capsize=.1)
    plt.yscale('log')
    plt.title('Average Test MEDIAN MSE (Log Scale)', fontsize=14, fontweight='bold')
    plt.ylabel('Mean Median MSE (Error Bars: STDEV.S)')
    plt.xlabel('Population Size')

    # --- Figure 3: Average Runtime ---
    plt.figure(figsize=(10, 6))
    sns.barplot(data=df, x='popSize', y='ActualRuntime', hue='Algorithm', 
                hue_order=hue_order, palette=palette, 
                errorbar='sd', capsize=.1)
    plt.title('Average Runtime', fontsize=14, fontweight='bold')
    plt.ylabel('Mean Seconds (Error Bars: STDEV.S)')
    plt.xlabel('Population Size')

    # Final visual adjustments & display popups
    plt.tight_layout()
    plt.show()  # This command opens all windows at once on your local machine

    # Print summary table for verification
    summary = df.groupby(['Algorithm', 'popSize']).agg({
        'testBestMSE': ['mean', 'std'],
        'testMedianMSE': ['mean', 'std'],
        'ActualRuntime': ['mean', 'std']
    })
    print("\n--- Experiment Summary Statistics ---")
    print(summary)

# Run the analysis
if __name__ == "__main__":
    generate_gp_analysis('final_new.csv')
