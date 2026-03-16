import os
import csv

def analyze_gp_results(directory="./results"):
    all_data = []

    # Iterate through files in the results directory
    for filename in os.listdir(directory):
        if filename.endswith(".txt"):
            file_path = os.path.join(directory, filename)
            
            try:
                with open(file_path, mode='r', encoding='utf-8') as f:
                    # Use CSV DictReader to handle the header-column mapping
                    reader = csv.DictReader(f)
                    for row in reader:
                        # Store filename and the metrics we care about as floats
                        all_data.append({
                            'filename': filename,
                            'bestMSE': float(row['bestMSE']),
                            'avgMSE': float(row['avgMSE']),
                            'medianMSE': float(row['medianMSE'])
                        })
            except (ValueError, KeyError, IndexError) as e:
                print(f"Skipping {filename}: Error parsing data ({e})")

    if not all_data:
        print("No valid data found.")
        return

    # Helper function to get top 10 for a specific key
    def get_top_20(data_list, key):
        # Sort ascending (lower MSE is better)
        sorted_list = sorted(data_list, key=lambda x: x[key])
        return sorted_list[:20]

    top_best = get_top_20(all_data, 'bestMSE')
    top_avg = get_top_20(all_data, 'avgMSE')
    top_median = get_top_20(all_data, 'medianMSE')

    # Create a unique set of filenames to avoid duplicates if a file is top in multiple categories
    unique_files = set()

    print("--- TOP 10 BEST MSE ---")
    for item in top_best:
        print(f"{item['filename']}: {item['bestMSE']}")
        unique_files.add(item['filename'])

    print("\n--- TOP 10 AVG MSE ---")
    for item in top_avg:
        print(f"{item['filename']}: {item['avgMSE']}")
        unique_files.add(item['filename'])

    print("\n--- TOP 10 MEDIAN MSE ---")
    for item in top_median:
        print(f"{item['filename']}: {item['medianMSE']}")
        unique_files.add(item['filename'])

    print("\n" + "="*30)
    print(f"Total unique high-performing files: {len(unique_files)}")
    print(sorted(list(unique_files)))

if __name__ == "__main__":
    analyze_gp_results()
