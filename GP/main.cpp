#include <cassert>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "FullGrowTree.h"
#include "csv-parser/include/internal/csv_reader.hpp"

using namespace std;
using namespace csv;

namespace {

void parseCSV() {
  CSVReader reader("dataset/Dataset.csv");

  for (auto& row : reader) {
    // Note: Can also use index of
    // column with [] operator
    cout << row["utc_timestamp"].get<string>() << ", "
	 << row["Electricity_load"].get<string>() << ", "
	 << row["Residential_"
		"electricity_price"]
		.get<double>()
	 << ", "
	 << row["Residential_solar_"
		"generation"]
		.get<double>()
	 << ", "
	 << row["Residential_wind_"
		"generation"]
		.get<double>()
	 << ", " << row["Temperature"].get<double>() << ", "
	 << row["Relative Humidity"].get<double>() << endl;
  }
}

}  // namespace

enum class Action { GROW, EVALUATE, PRINT };

void grow(int startInd, int endIndExclusive, vector<FullGrowTree>& population,
	  int depth, int numVars, double chooseConstantProbability) {
  // do the action on all the indices
  for (int i = startInd; i < endIndExclusive; i++) {
    population[i] = FullGrowTree(depth, numVars, chooseConstantProbability);
    population[i].grow();
  }
}

string doubleVectorToString(const vector<double>& toPrint) {
  stringstream res;

  res << "[ ";

  for (const auto& p : toPrint) {
    res << p << " ";
  }
  res << "]";

  return res.str();
}

// INFO: inputs is a 2d vector that
// represent the variable inputs of the
// tree targets is a 1d vector which
// each tree is trying to evaluate to
// (given each set of inputs) results is
// a 1d vector storing the values of the
// evaluations
void evaluate(int startInd, int endIndExclusive,
	      vector<FullGrowTree>& population,
	      const vector<vector<double>>& inputs,
	      const vector<double>& targets, vector<double>& results) {
  assert(population.size() == inputs.size() &&
	 "population inputs size "
	 "mismatch");
  assert(inputs.size() == targets.size() && "targets inputs size mismatch");
  assert(targets.size() == results.size() && "targets results size mismatch");

  string str = "============== evaluate =============\n";

  // do the action on all the indices
  for (size_t popInd = startInd; popInd < endIndExclusive; popInd++) {
    double errorSum = 0;

    for (size_t currTargetInd = 0; currTargetInd < targets.size();
	 currTargetInd++) {
      str +=
	  "TREE: " + population[popInd].toString(inputs[currTargetInd]) + "\n";

      double value = population[popInd].evaluate(inputs[currTargetInd]);
      double currError = value - targets[currTargetInd];

      errorSum += (currError * currError);

      str += "<" + doubleVectorToString(inputs[currTargetInd]) +
	     "> [ target: " + to_string(targets[currTargetInd]) +
	     ", got: " + to_string(value) +
	     ", error squared: " + to_string(currError * currError) + "]\n";
    }

    results[popInd] = errorSum;
  }

  cout << str << endl;
}

// INFO: [start, end)
tuple<int, int> getIndices(int threadIndex, int populationSize, int chunkSize) {
  int start = threadIndex * chunkSize;

  // exclusive end
  int end = min(populationSize, start + chunkSize);

  return make_tuple(start, end);
}

int cielInt(int num, int denom) { return (num + denom - 1) / denom; }

struct Config {
  int populationSize;
  int numThreads;
  int depth;
  double chooseConstantProbability;
};

void epoch(vector<FullGrowTree>& population,
	   const vector<vector<double>>& inputs, const vector<double>& targets,
	   vector<double>& results, Config& conf) {
  // list of threads
  vector<thread> threads;
  const int CHUNK_SIZE =
      cielInt(static_cast<int>(population.size()), conf.numThreads);
  vector<tuple<int, int>> indices;

  for (int i = 0; i < conf.numThreads; i++) {
    indices.push_back(
	getIndices(i, static_cast<int>(population.size()), CHUNK_SIZE));
  }

  // INFO: -------- 1A) grow all the trees -------------
  for (int i = 0; i < conf.numThreads; i++) {
    auto [start, end] = indices[i];

    // this will grow full grow trees in
    // the buffer pass targets by
    // reference to the thread
    threads.emplace_back(&grow, start, end, ref(population), conf.depth,
			 inputs[0].size(), conf.chooseConstantProbability);
  }

  // INFO: ---------------- 1B) join ------------------
  for (auto& currThread : threads) {
    currThread.join();
  }
  threads.clear();

  // -----------------------------------------------------

  // INFO: 2A) spawn 3 threads to evaluate the whole buffer
  for (int i = 0; i < conf.numThreads; i++) {
    auto [start, end] = indices[i];

    // instantiate the fullgrowtree
    // buffer action this will grow full
    // grow trees pass the inputs and
    // targets by reference to the
    // thread
    /*
     int startInd, int endIndExclusive,
	      vector<FullGrowTree>&
     population, const
     vector<vector<double>>& inputs,
	      const vector<double>&
     targets, double& result */
    threads.emplace_back(&evaluate, start, end, ref(population), cref(inputs),
			 cref(targets), ref(results));
  }

  // INFO: 2B) join
  for (auto& currThread : threads) {
    currThread.join();
  }
  threads.clear();

  // INFO: 3A) crossover (single threaded) INFO: 3B) join
  // INFO: 4A) spawn 3 threads to mutate the whole buffer INFO: 4B) join
  // INFO: 5A) selection (single threaded, just return indices)
  // INFO: 5B) join
}

void smallTest() {
  Config conf = {.populationSize = 6,
		 .numThreads = 2,
		 .depth = 3,
		 .chooseConstantProbability = 0.5};

  vector<FullGrowTree> population(conf.populationSize);

  // equation 3a + 2b
  vector<vector<double>> inputs = {{1.0, 1.0},	{4.0, 2.0},  {0.0, 5.0},
				   {-2.0, 1.0}, {3.0, -3.0}, {5.0, 0.0}};
  vector<double> targets = {1.0, 8.0, -10.0, -8.0, 15.0, 15.0};
  vector<double> results = {0, 0, 0, 0, 0, 0};

  epoch(population, inputs, targets, results, conf);
}

int main() {
  smallTest();

  return 0;
}
