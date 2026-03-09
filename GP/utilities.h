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

void printTrees(vector<unique_ptr<Tree>>& population, vector<double>& inputs) {
  for (int i = 0; i < population.size(); i++) {
    cout << population[i]->toString(inputs) << endl;
  }
}

struct SelectionResult {
  vector<int> selectedIndices;
  size_t bestOverallIndex;
};

SelectionResult tournamentSelection(const vector<double>& errors,
				    int tournamentSize) {
  // preallocate a vector to store the selection indices
  vector<int> selectedIndices;
  // resize fills with 0s
  selectedIndices.resize(errors.size());

  size_t bestOverallIndex = 0;
  double bestOverallFitness = errors[0];

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

    // error is higher with the best individual
    if (bestOverallFitness > errors[bestIndividualIndex]) {
      bestOverallFitness = errors[bestIndividualIndex];
      bestOverallIndex = bestIndividualIndex;
    }
  }

  return {.selectedIndices = selectedIndices,
	  .bestOverallIndex = bestOverallIndex};
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

vector<tuple<int, int>> getThreadIndices(int populationSize, int numThreads) {
  const int CHUNK_SIZE = cielInt(static_cast<int>(populationSize), numThreads);
  vector<tuple<int, int>> indices;
  indices.reserve(numThreads);

  for (int i = 0; i < numThreads; i++) {
    indices.push_back(
	getIndices(i, static_cast<int>(populationSize), CHUNK_SIZE));
  }

  return indices;
}

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
