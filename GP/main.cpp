#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "FullGrowTree.h"
#include "GrowTree.h"
#include "utilities.h"

using namespace std;

enum class Action { GROW, EVALUATE, PRINT };

enum ErrorStrategy { MEAN_SQUARED_ERROR };

/*
void generation(vector<Tree*>& population, const vector<vector<double>>& inputs,
		const vector<double>& targets, vector<double>& errors,
		Config& conf, ErrorStrategy errorStrategy) {

    threads.emplace_back(&grow, start, end, ref(population), conf.depth,
			 inputs[0].size(), conf.chooseConstantProbability);
 */

enum class TreeType { FULL_GROW, GROW };

void grow(int startInd, int endIndExclusive, vector<Tree*>& population,
	  int depth, int numVars, double chooseConstantProbability,
	  TreeType type, double prematureLeafProbability) {
  // do the action on all the indices
  for (int i = startInd; i < endIndExclusive; i++) {
    if (type == TreeType::FULL_GROW) {
      population[i] =
	  new FullGrowTree(depth, numVars, chooseConstantProbability);
    } else {
      population[i] = new GrowTree(depth, numVars, chooseConstantProbability,
				   prematureLeafProbability);
    }

    population[i]->grow();
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
	      vector<unique_ptr<Tree>>& population,
	      const vector<vector<double>>& inputs,
	      const vector<double>& targets, vector<double>& errors,
	      ErrorStrategy errorStrategy) {
  assert(inputs.size() == targets.size() && "targets inputs size mismatch");

  // do the action my individuals
  for (int popInd = startInd; popInd < endIndExclusive; popInd++) {
    double errorSum = 0;

    // TODO: remove
    // string str = "";

    // loop through all the targets
    for (size_t currTargetInd = 0; currTargetInd < targets.size();
	 currTargetInd++) {
      // TODO: remove
      /*
      str += "TREE (" + to_string(popInd) +
	     "): " + population[popInd]->toString(inputs[currTargetInd]) + "\n";
       */
      double value = population[popInd]->evaluate(inputs[currTargetInd]);
      double currError = value - targets[currTargetInd];

      /*
      // TODO: remove
      str += "<" + utils::vectorToString(inputs[currTargetInd]) +
	     "> [ target: " + to_string(targets[currTargetInd]) +
	     ", got: " + to_string(value) + "]";
       */

      // get the error based on the error strategy
      switch (errorStrategy) {
	case MEAN_SQUARED_ERROR:
	  errorSum += (currError * currError);

	  /*
		// TODO: remove
		str += "\n Error (MSE) for individual " + to_string(popInd) + ":
	     " + to_string(currError * currError);
	   */
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
    // cout << str << endl;
  }
}

struct Config {
  int populationSize;
  int numThreads;
  int generations;
  double chooseConstantProbability;
  int tournamentSize;
  int numVars;
  double prematureLeafProbability;
  double crossoverRate;
  double mutationRate;
};

struct GrowStrategy {
  int minDepth;
  int maxDepth;
  int fullGrow;
  int grow;
};

void mutatePopulation(vector<unique_ptr<Tree>>& population, const Config& conf,
		      int startInd, int endIndExclusive) {
  for (int i = startInd; i < endIndExclusive; i++) {
    if (Tree::getRandomDouble(0, 1) < conf.mutationRate)
      population[i]->mutate();
  }
}

// WARN: call single threaded
void growPopulation(vector<unique_ptr<Tree>>& population, Config& conf,
		    const GrowStrategy& growStrategy) {
  int index = 0;

  // loop through the min and max depths
  for (int depth = growStrategy.minDepth; depth <= growStrategy.maxDepth;
       depth++) {
    // grow all the fullgrow trees
    for (int f = 0; f < growStrategy.fullGrow; f++) {
      assert(index < population.size() &&
	     "Population index out of bounds for fullgrow");
      population[index] = make_unique<FullGrowTree>(
	  depth, conf.numVars, conf.chooseConstantProbability);

      population[index++]->grow();
    }

    // grow all the grow trees
    for (int g = 0; g < growStrategy.grow; g++) {
      assert(index < population.size() &&
	     "Population index out of bounds for grow");
      population[index] = make_unique<GrowTree>(depth, conf.numVars,
						conf.chooseConstantProbability,
						conf.prematureLeafProbability);

      population[index++]->grow();
    }
  }

  assert(index == population.size() && "popluation not filled");

  auto rng = std::default_random_engine{};
  std::ranges::shuffle(population, rng);
}

// assumes that the initial trees are already grown
void generation(vector<unique_ptr<Tree>>& population,
		const vector<vector<double>>& inputs,
		const vector<double>& targets, vector<double>& errors,
		Config& conf, ErrorStrategy errorStrategy,
		std::unique_ptr<Tree>& overallBestIndividual,
		double& overallLowestError) {
  assert(population.size() % 2 == 0 &&
	 "Population size must be divisible by 2");

  // list of threads
  vector<thread> threads;
  threads.reserve(conf.numThreads);

  auto indices = utils::getThreadIndices(population.size(), conf.numThreads);

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

  /*
  // TODO: remove
  cout << "-------------- errors (MSE) -------------- " << endl;
  cout << utils::vectorToString(errors) << endl;
  */

  // INFO: 3A) tournament selection (single threaded, just return indices)
  auto selRes = utils::tournamentSelection(errors, conf.tournamentSize);

  /*
  // TODO: remove
  cout << "------------ selected indices ------------- " << endl;
  cout << utils::vectorToString(selRes.selectedIndices) << endl;
  cout << "BEST: " << selRes.bestOverallIndex << endl;
  */

  auto bestIndiv = population[selRes.bestOverallIndex]->clone();

  /*
  cout << "-----------CLONE BEST INDIVIDUAL --------- " << endl;
  cout << bestIndiv->toString(inputs[0]) << endl;
  */

  if (errors[selRes.bestOverallIndex] < overallLowestError) {
    overallLowestError = errors[selRes.bestOverallIndex];
    overallBestIndividual = population[selRes.bestOverallIndex]->clone();
  }

  // Create the next generation from the selected indices
  vector<unique_ptr<Tree>> nextGeneration;
  nextGeneration.reserve(population.size());

  // INFO: 4A) crossover (single threaded):
  for (size_t i = 0; i < population.size() / 2; i++) {
    // get 2 selected parents in order
    int parent1 = selRes.selectedIndices[2 * i];
    int parent2 = selRes.selectedIndices[2 * i + 1];

    auto child1 = population[parent1]->clone();
    auto child2 = population[parent2]->clone();

    // randomly crossover if need be
    if (Tree::getRandomDouble(0, 1) < conf.crossoverRate)
      child1->crossover(*child2);

    // now child 1 and child2 are modified by the crossover and mutation, put in
    nextGeneration.push_back(std::move(child1));
    nextGeneration.push_back(std::move(child2));
  }

  assert(nextGeneration.size() == population.size() &&
	 "Next generation and population are different sizes");

  // replace my generation now with the next one
  population.swap(nextGeneration);

  /*
  cout << "------------ after crossover -------------" << endl;
  for (auto& currIndiv : population) {
    cout << currIndiv->toString(inputs[0]);
  }
  */

  // INFO: 4A) spawn threads to mutate the whole buffer
  for (int i = 0; i < conf.numThreads; i++) {
    auto [start, end] = indices[i];

    threads.emplace_back(&mutatePopulation, ref(population), conf, start, end);
  }

  // INFO: 4B) join
  for (auto& currThread : threads) {
    currThread.join();
  }
  threads.clear();

  // TODO: remove
  /*
  cout << "------------ after mutation ------------- " << endl;
  for (auto& currIndiv : population) {
    cout << currIndiv->toString(inputs[0]);
  }
  */

  /*
  cout << endl << " ---------- replace best indiv -----------" << endl;
  cout << bestIndiv->toString(inputs[0]) << " = "
       << bestIndiv->evaluate(inputs[0]) << endl;
       */
  // move the best individual into a random spot
  population[Tree::getRandomInt(0, population.size() - 1)] = bestIndiv->clone();
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
  // CONFIG
  Tree::highestConstant = 50;
  Tree::smallestConstant = -50;

  // per thread strategy
  GrowStrategy growStrategy = {
      .minDepth = 1, .maxDepth = 4, .fullGrow = 500, .grow = 500};

  const int NUM_THREADS = 8;

  const int POP_SIZE = (growStrategy.fullGrow + growStrategy.grow) *
		       (growStrategy.maxDepth - growStrategy.minDepth + 1);

  cout << "Growing popluation of size: " << POP_SIZE;

  vector<vector<double>> inputs = {{1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1},
				   {1, 2}, {2, 2}, {3, 2}, {4, 2}, {5, 2}};

  vector<double> targets = {1.0,      1.470588, 1.792079, 1.941176, 1.975610,
			    1.470588, 1.941176, 2.262667, 2.411765, 2.446199};

  vector<double> errors;

  Config config = {.populationSize = POP_SIZE,
		   .numThreads = NUM_THREADS,
		   .generations = 800,
		   .chooseConstantProbability = 0.5,
		   .tournamentSize = 5,
		   .numVars = static_cast<int>(inputs[0].size()),
		   .prematureLeafProbability = 0.2,
		   .mutationRate = 0.8};

  vector<unique_ptr<Tree>> population;
  population.resize(config.populationSize);

  errors.resize(config.populationSize);

  growPopulation(population, config, growStrategy);

  std::unique_ptr<Tree> overallBestIndividual = population[0]->clone();
  double overallLowestError = 100000;

  cout << "--------------- INITIAL GROW -------------------------" << endl;
  utils::printTrees(population, inputs[0]);

  unordered_set<string> uniqueTrees;

  for (int i = 1; i <= config.generations; i++) {
    // call generation to continue after initial grow
    generation(population, inputs, targets, errors, config,
	       ErrorStrategy::MEAN_SQUARED_ERROR, overallBestIndividual,
	       overallLowestError);
    if (i % 50 == 0) {
      cout << "----------------- GENERATION " << i
	   << " -------------------------" << endl;

      cout << "BEST: " << overallBestIndividual->toString(inputs[2])
	   << "[ error " << overallLowestError << " ]";

      uniqueTrees.clear();

      for (auto& tree : population)
	uniqueTrees.insert(tree->toString(inputs[2]));

      cout << " Unique tree size: " << uniqueTrees.size();
      cout << ", popluation size: " << population.size();

      double diversity =
	  static_cast<double>(uniqueTrees.size()) / population.size();

      cout << ", diversity " << diversity << endl;
    }
  }
}

int main() {
  generationTest();
  return 0;
}
