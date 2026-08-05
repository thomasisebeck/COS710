import os
import csv

def analyze_gp_results(directory="./results/validation"):
    all_data = []

    for filename in os.listdir(directory):
        if filename.endswith(".txt"):
            file_path = os.path.join(directory, filename)

            try:
                with open(file_path, mode='r', encoding='utf-8') as f:
                    reader = csv.DictReader(f)

                    for row in reader:

                        row_data = dict(row)

                        for key, value in row_data.items():
                            try:
                                row_data[key] = float(value)
                            except (ValueError, TypeError):
                                pass

                        row_data["filename"] = filename

                        all_data.append(row_data)

            except Exception as e:
                print(f"Skipping {filename}: {e}")

    if not all_data:
        print("No valid data found.")
        return

    # sort by validation best MSE
    top_runs = sorted(all_data, key=lambda x: x["testBestMSE"])[:20]

    print("\n===== TOP 20 BY TEST MSE =====\n")

    for run in top_runs:
        print(run)

if __name__ == "__main__":
    analyze_gp_results()
