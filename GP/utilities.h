#include <cassert>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "DataProccessor.h"
#include "Tree.h"
using namespace std;

namespace utils {

vector<int> tournamentSelection(const vector<double>& errors,
				int tournamentSize) {
  // preallocate a vector to store the selection indices
  vector<int> selectedIndices;
  // resize fills with 0s
  selectedIndices.resize(errors.size());

  for (size_t i = 0; i < errors.size(); i++) {
    // assume this index wins
    int bestIndividualIndex = Tree::getRandomInt(0, errors.size() - 1);

    // challenge this individual with (tournamentSize - 1) other individuals
    for (int j = 1; j < tournamentSize; j++) {
      // get a challenger by selecting another random individual
      int challenger = Tree::getRandomInt(0, errors.size() - 1);

      // challenger outperforms the best individual
      if (errors[challenger] < errors[bestIndividualIndex]) {
	bestIndividualIndex = challenger;
      }
    }

    // add the best performing individual
    selectedIndices[i] = bestIndividualIndex;
  }

  return selectedIndices;
}

vector<Row> readDataToRows(const string& inputFile) {
  DataProcessor processor;
  processor.readCSV(inputFile);
  return processor.getRows();
}

template <typename T>
string vectorToString(const vector<T>& toPrint) {
  stringstream res;

  res << "[ ";

  for (const auto& p : toPrint) {
    res << p << " ";
  }
  res << "]";

  return res.str();
}

// INFO: [start, end)
tuple<int, int> getIndices(int threadIndex, int populationSize, int chunkSize) {
  int start = threadIndex * chunkSize;

  // exclusive end
  int end = min(populationSize, start + chunkSize);

  return make_tuple(start, end);
}

int cielInt(int num, int denom) { return (num + denom - 1) / denom; }

void readFile() {
  const string INPUT_FILE = "dataset/processed.csv";

  const auto rows = readDataToRows(INPUT_FILE);

  int max = 5;
  int curr = 0;

  cout << "first 5 lines read: " << endl;
  for (auto r : rows) {
    if (curr++ == max) break;

    cout << setw(5) << std::fixed << "ld: " << r.load << "\t N1: " << r.loadN1
	 << "\t N2: " << r.loadN2 << "\t minSin: " << r.minuteSin
	 << "\t minCos: " << r.minuteCos << "\t daySin: " << r.dayOfYearSin
	 << "\t dayCos: " << r.dayOfYearCos << endl;
  }
}
}  // namespace utils
