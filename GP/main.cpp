#include <cassert>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "FullGrowTree.h"
#include "GrowTree.h"
#include "utilities.h"

using namespace std;

enum class Action { GROW, EVALUATE, PRINT };

enum ErrorStrategy { MEAN_SQUARED_ERROR };

void grow(int startInd, int endIndExclusive, vector<FullGrowTree>& population,
	  int depth, int numVars, double chooseConstantProbability) {
  // do the action on all the indices
  for (int i = startInd; i < endIndExclusive; i++) {
    population[i] = FullGrowTree(depth, numVars, chooseConstantProbability);
    population[i].grow();
  }
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
	      const vector<double>& targets, vector<double>& errors,
	      ErrorStrategy errorStrategy) {
  assert(inputs.size() == targets.size() && "targets inputs size mismatch");

  // do the action my individuals
  for (int popInd = startInd; popInd < endIndExclusive; popInd++) {
    double errorSum = 0;

    // TODO: remove
    string str = "";

    // loop through all the targets
    for (size_t currTargetInd = 0; currTargetInd < targets.size();
	 currTargetInd++) {
      // TODO: remove
      str +=
	  "TREE: " + population[popInd].toString(inputs[currTargetInd]) + "\n";

      double value = population[popInd].evaluate(inputs[currTargetInd]);
      double currError = value - targets[currTargetInd];

      // TODO: remove
      str += "<" + utils::vectorToString(inputs[currTargetInd]) +
	     "> [ target: " + to_string(targets[currTargetInd]) +
	     ", got: " + to_string(value) + "]";

      // get the error based on the error strategy
      switch (errorStrategy) {
	case MEAN_SQUARED_ERROR:
	  errorSum += (currError * currError);

	  // TODO: remove
	  str += "\n Error (MSE) for individual " + to_string(popInd) + ": " +
		 to_string(currError * currError);
	  break;
	default:
	  throw runtime_error("Unknown error strategy");
      }
    }

    switch (errorStrategy) {
      case MEAN_SQUARED_ERROR:
	errors[popInd] = errorSum / targets.size();

	break;
      default:
	throw runtime_error("Unknown error strategy");
    }

    // TODO: remove
    cout << str << endl;
  }
}

struct Config {
  int populationSize;
  int numThreads;
  int depth;
  int generations;
  double chooseConstantProbability;
  int tournamentSize;
};

void generation(vector<FullGrowTree>& population,
		const vector<vector<double>>& inputs,
		const vector<double>& targets, vector<double>& errors,
		Config& conf, ErrorStrategy errorStrategy) {
  // list of threads
  vector<thread> threads;
  threads.reserve(conf.numThreads);

  const int CHUNK_SIZE =
      utils::cielInt(static_cast<int>(population.size()), conf.numThreads);
  vector<tuple<int, int>> indices;
  indices.reserve(conf.numThreads);

  for (int i = 0; i < conf.numThreads; i++) {
    indices.push_back(
	utils::getIndices(i, static_cast<int>(population.size()), CHUNK_SIZE));
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

  // INFO: 2A) spawn threads to evaluate the whole buffer
  for (int i = 0; i < conf.numThreads; i++) {
    auto [start, end] = indices[i];

    threads.emplace_back(&evaluate, start, end, ref(population), cref(inputs),
			 cref(targets), ref(errors), errorStrategy);
  }

  // INFO: 2B) join
  for (auto& currThread : threads) {
    currThread.join();
  }
  threads.clear();

  cout << "-------------- errors (MSE) -------------- " << endl;
  cout << utils::vectorToString(errors) << endl;

  // INFO: 3A) tournament selection (single threaded, just return indices)
  vector<int> selectedIndices =
      utils::tournamentSelection(errors, conf.tournamentSize);

  cout << "------------ selected indices ------------- " << endl;
  cout << utils::vectorToString(selectedIndices) << endl;
  cout << "------------------------------------------- " << endl;

  // INFO: 4A) crossover (single threaded) INFO: 3B) join
  // INFO: 4B) spawn 3 threads to mutate the whole buffer INFO: 4B) join
}

void crossoverTest() {
  const double CHOOSE_CONST_PROB = 0.5;
  const int DEPTH = 5;
  vector<double> vars = {0.5, 0.2};
  Tree::highestConstant = 10;
  Tree::smallestConstant = -10;

  FullGrowTree tree1(DEPTH, vars.size(), CHOOSE_CONST_PROB);
  tree1.grow();

  FullGrowTree tree2(DEPTH, vars.size(), CHOOSE_CONST_PROB);
  tree2.grow();

  cout << "Full grow trees: " << endl;
  cout << tree1.toString(vars) << endl;
  cout << tree2.toString(vars) << endl;
  cout << "---------------" << endl;

  tree1.crossover(tree2);

  cout << "After crossover: " << endl;
  cout << tree1.toString(vars) << endl;
  cout << tree2.toString(vars) << endl;
  cout << "---------------" << endl;
}

void growTreeTest() {
  const double CHOOSE_CONST_PROB = 0.5;
  const double PREMATURE_LEAF_PROB = 0.3;
  const int DEPTH = 5;
  vector<double> vars = {0.5, 0.2};
  Tree::highestConstant = 10;
  Tree::smallestConstant = -10;

  GrowTree tree1(DEPTH, vars.size(), CHOOSE_CONST_PROB, PREMATURE_LEAF_PROB);
  GrowTree tree2(DEPTH, vars.size(), CHOOSE_CONST_PROB, PREMATURE_LEAF_PROB);
  GrowTree tree3(DEPTH, vars.size(), CHOOSE_CONST_PROB, PREMATURE_LEAF_PROB);
  GrowTree tree4(DEPTH, vars.size(), CHOOSE_CONST_PROB, PREMATURE_LEAF_PROB);
  GrowTree tree5(DEPTH, vars.size(), CHOOSE_CONST_PROB, PREMATURE_LEAF_PROB);
  GrowTree tree6(DEPTH, vars.size(), CHOOSE_CONST_PROB, PREMATURE_LEAF_PROB);

  tree1.grow();
  tree2.grow();
  tree3.grow();
  tree4.grow();
  tree5.grow();
  tree6.grow();

  cout << "AFTER GROW: " << endl;
  cout << "tree1: " << tree1.toString(vars) << endl;
  cout << "tree2: " << tree2.toString(vars) << endl;
  cout << "tree3: " << tree3.toString(vars) << endl;
  cout << "tree4: " << tree4.toString(vars) << endl;
  cout << "tree5: " << tree5.toString(vars) << endl;
  cout << "tree6: " << tree6.toString(vars) << endl;

  cout << "EVALUATE: " << endl;
  cout << "tree1: " << tree1.evaluate(vars) << endl;
  cout << "tree2: " << tree2.evaluate(vars) << endl;
  cout << "tree3: " << tree3.evaluate(vars) << endl;
  cout << "tree4: " << tree4.evaluate(vars) << endl;
  cout << "tree5: " << tree5.evaluate(vars) << endl;
  cout << "tree6: " << tree6.evaluate(vars) << endl;

  tree1.crossover(tree2);
  tree3.crossover(tree4);
  tree5.crossover(tree6);

  cout << "AFTER CROSSOVER: " << endl;
  cout << "tree1: " << tree1.toString(vars) << endl;
  cout << "tree2: " << tree2.toString(vars) << endl;
  cout << "tree3: " << tree3.toString(vars) << endl;
  cout << "tree4: " << tree4.toString(vars) << endl;
  cout << "tree5: " << tree5.toString(vars) << endl;
  cout << "tree6: " << tree6.toString(vars) << endl;

  cout << "EVALUATE: " << endl;
  cout << "tree1: " << tree1.evaluate(vars) << endl;
  cout << "tree2: " << tree2.evaluate(vars) << endl;
  cout << "tree3: " << tree3.evaluate(vars) << endl;
  cout << "tree4: " << tree4.evaluate(vars) << endl;
  cout << "tree5: " << tree5.evaluate(vars) << endl;
  cout << "tree6: " << tree6.evaluate(vars) << endl;

  cout << "SAME TREE CROSSOVER: " << endl;
  cout << "tree1 before: " << tree1.toString(vars) << endl;
  tree1.crossover(tree1);
  cout << "tree1 after 1 crossover: " << tree1.toString(vars) << endl;
  tree1.crossover(tree1);
  cout << "tree1 after 2 crossovers: " << tree1.toString(vars) << endl;
  tree1.crossover(tree1);
  cout << "tree1 after 3 crossovers: " << tree1.toString(vars) << endl;
}

void generationTest() {
  const double CHOOSE_CONST_PROB = 0.5;
  const double PREMATURE_LEAF_PROB = 0.3;
  const int DEPTH = 5;
  vector<double> vars = {0.5, 0.2};
  Tree::highestConstant = 10;
  Tree::smallestConstant = -10;

  GrowTree tree1(DEPTH, vars.size(), CHOOSE_CONST_PROB, PREMATURE_LEAF_PROB);
  GrowTree tree2(DEPTH, vars.size(), CHOOSE_CONST_PROB, PREMATURE_LEAF_PROB);
  GrowTree tree3(DEPTH, vars.size(), CHOOSE_CONST_PROB, PREMATURE_LEAF_PROB);
  GrowTree tree4(DEPTH, vars.size(), CHOOSE_CONST_PROB, PREMATURE_LEAF_PROB);
  GrowTree tree5(DEPTH, vars.size(), CHOOSE_CONST_PROB, PREMATURE_LEAF_PROB);
  GrowTree tree6(DEPTH, vars.size(), CHOOSE_CONST_PROB, PREMATURE_LEAF_PROB);

  tree1.grow();
  tree2.grow();
  tree3.grow();
  tree4.grow();
  tree5.grow();
  tree6.grow();

  cout << "AFTER GROW: " << endl;
  cout << "tree1: " << tree1.toString(vars) << endl;
  cout << "tree2: " << tree2.toString(vars) << endl;
  cout << "tree3: " << tree3.toString(vars) << endl;
  cout << "tree4: " << tree4.toString(vars) << endl;
  cout << "tree5: " << tree5.toString(vars) << endl;
  cout << "tree6: " << tree6.toString(vars) << endl;

  tree1.crossover(tree2);
  tree3.crossover(tree4);
  tree5.crossover(tree6);

  cout << "AFTER crossover: " << endl;
  cout << "tree1: " << tree1.toString(vars) << endl;
  cout << "tree2: " << tree2.toString(vars) << endl;
  cout << "tree3: " << tree3.toString(vars) << endl;
  cout << "tree4: " << tree4.toString(vars) << endl;
  cout << "tree5: " << tree5.toString(vars) << endl;
  cout << "tree6: " << tree6.toString(vars) << endl;

  tree1.mutate();
  tree2.mutate();
  tree3.mutate();
  tree4.mutate();
  tree5.mutate();
  tree6.mutate();

  cout << "AFTER MUTATE: " << endl;
  cout << "tree1: " << tree1.toString(vars) << endl;
  cout << "tree2: " << tree2.toString(vars) << endl;
  cout << "tree3: " << tree3.toString(vars) << endl;
  cout << "tree4: " << tree4.toString(vars) << endl;
  cout << "tree5: " << tree5.toString(vars) << endl;
  cout << "tree6: " << tree6.toString(vars) << endl;
}

void errorTest() {
  const double CHOOSE_CONST_PROB = 0.5;
  const double PREMATURE_LEAF_PROB = 0.3;
  const int DEPTH = 5;
  vector<double> vars = {0.5, 0.2};

  Tree::highestConstant = 10;
  Tree::smallestConstant = -10;
  /*

struct Config {
  int populationSize;
  int numThreads;
  int depth;
  int generations;
  double chooseConstantProbability;
};
 */
  Config config = {.populationSize = 10,
		   .numThreads = 2,
		   .depth = 3,
		   .generations = 2,
		   .chooseConstantProbability = 0.5,
		   .tournamentSize = 2};

  vector<FullGrowTree> population;
  population.resize(config.populationSize);
  vector<vector<double>> inputs = {{5, 2}};
  vector<double> targets = {10};
  vector<double> errors;
  errors.resize(config.populationSize);

  // create a popluation
  generation(population, inputs, targets, errors, config, MEAN_SQUARED_ERROR);
}

int main() {
  errorTest();
  return 0;
}
