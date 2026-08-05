import numpy as np
import pandas as pd
from sklearn.neighbors import NearestNeighbors

import pandas as pd
import numpy as np
from sklearn.neighbors import NearestNeighbors

def estimate_noise_floor(file_paths, k=30):
    results = {}

    for path in file_paths:
        print(f"\nProcessing file: {path}")

        df = pd.read_csv(path)
        df.columns = df.columns.str.strip()

        if df.shape[0] == 0:
            print("Dataset is empty, skipping.")
            results[path] = None
            continue


        X = df[
            [
                "load_n1",
                "load_n2",
                "load_n3",
                "load_n4",
                "load_n5",
                "load_n6",
                "load_prev_day",
            ]
        ].values

        y = df["load"].values

        nn = NearestNeighbors(n_neighbors=k)
        nn.fit(X)

        # get the indices of the nearest neighbors for each row (X)
        # these are all the rows where all input variables (load_n1, load_n2, etc) are similar in euclidian distance
        distances, indices = nn.kneighbors(X)

        variances = []

        # iterate over each row in the dataset
        for i in range(len(X)):
            # get the load values for each neighbor 
            neighbour_targets = y[indices[i]]

            # calculate the variance of all neighbors 1/n * SUM((mean - value)^2)
            variances.append(np.var(neighbour_targets))

        # noise floor is the mean of the variances (average variance)
        noise_floor = np.mean(variances)

        results[path] = noise_floor

    return results



files = [ 
   "dataset/training.csv",
   "dataset/validation.csv",
   "dataset/test.csv",
    "dataset/processed.csv"
]

results = estimate_noise_floor(["dataset/processed.csv"], k=30)

print("\nSummary:")
for f, v in results.items():
    print(f"{f} noise floor -> {v}")
    print(f"{f} target MSE -> {v * 1.15}")
