#include <cassert>
#include <functional>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "DataProccessor.h"
#include "FullGrowTree.h"

using namespace std;

std::vector<Row> readDataToRows(const string& inputFile) {
  DataProcessor processor;
  processor.readCSV(inputFile);
  return processor.getRows();
}

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

  // cout << str << endl;
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
  int generations;
  double chooseConstantProbability;
};

void generation(vector<FullGrowTree>& population,
		const vector<vector<double>>& inputs,
		const vector<double>& targets, vector<double>& results,
		Config& conf) {
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
  Config conf = {
      .populationSize = 250,
      .numThreads = 8,
      .depth = 5,
      .generations = 10,
      .chooseConstantProbability = 0.5,
  };

  vector<FullGrowTree> population(conf.populationSize);

  vector<vector<double>> inputs;
  vector<double> targets;
  vector<double> results;

  for (int i = 0; i < conf.populationSize; ++i) {  // 64 values
    double x = i;

    inputs.push_back({x, 0.0});	 // second column unused
    targets.push_back(2.0 * x + 5.0);

    // INFO: 0 for now, will populate later
    results.push_back(0.0);
  }

  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < conf.generations; i++) {
    cout << "Generation: " << i << endl;

    generation(population, inputs, targets, results, conf);
  }

  auto end = std::chrono::high_resolution_clock::now();

  std::cout << "Time: " << std::chrono::duration<double>(end - start).count()
	    << "s\n";
}

void readFile() {
  const string INPUT_FILE = "dataset/processed.csv";

  const auto rows = readDataToRows(INPUT_FILE);

  int max = 5;
  int curr = 0;

  cout << "first 5 lines read: " << endl;
  for (auto r : rows) {
    if (curr++ == max) break;

    cout << std::setw(5) << std::fixed << "ld: " << r.load
	 << "\t N1: " << r.loadN1 << "\t N2: " << r.loadN2
	 << "\t minSin: " << r.minuteSin << "\t minCos: " << r.minuteCos
	 << "\t daySin: " << r.dayOfYearSin << "\t dayCos: " << r.dayOfYearCos
	 << endl;
  }
}

void crossoverTest() {}

int main() {
  readFile();
  return 0;
}
