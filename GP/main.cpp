#include <algorithm>
#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "DataProccessor.h"
#include "FullGrowTree.h"
#include "GrowTree.h"
#include "utilities.h"

using namespace std;

enum class Action { GROW, EVALUATE, PRINT };

enum ErrorStrategy { MEAN_SQUARED_ERROR };

enum class TreeType { FULL_GROW, GROW };

void grow(int startInd, int endIndExclusive, vector<Tree*>& population,
	  int depth, int numVars, double chooseConstantProbability,
	  TreeType type, double prematureLeafProbability,
	  double tuneConstantProbability) {
  // do the action on all the indices
  for (int i = startInd; i < endIndExclusive; i++) {
    if (type == TreeType::FULL_GROW) {
      population[i] = new FullGrowTree(
	  depth, numVars, chooseConstantProbability, tuneConstantProbability);
    } else {
      population[i] =
	  new GrowTree(depth, numVars, chooseConstantProbability,
		       prematureLeafProbability, tuneConstantProbability);
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
	      ErrorStrategy errorStrategy, int evaluationSampleSize,
	      const double& parsimonyPressure) {
  assert(inputs.size() == targets.size() && "targets inputs size mismatch");

  vector<int> evaluationSampleIndices;
  evaluationSampleIndices.reserve(evaluationSampleSize);

  // create the evaluation criteria
  for (size_t currTargetInd = 0; currTargetInd < evaluationSampleSize;
       currTargetInd++) {
    evaluationSampleIndices.push_back(Tree::getRandomInt(0, inputs.size() - 1));
  }

  assert(evaluationSampleIndices.size() == evaluationSampleSize);

  // do the action my individuals
  for (int popInd = startInd; popInd < endIndExclusive; popInd++) {
    double errorSum = 0;

    // loop through all the targets
    for (size_t currTargetInd = 0; currTargetInd < evaluationSampleSize;
	 currTargetInd++) {
      int select = evaluationSampleIndices[currTargetInd];
      double value = population[popInd]->evaluate(inputs[select]);
      double currError = value - targets[select];

      // get the error based on the error strategy
      switch (errorStrategy) {
	case MEAN_SQUARED_ERROR:
	  errorSum += (currError * currError);

	  break;
	default:
	  throw runtime_error("Unknown error strategy");
      }
    }

    switch (errorStrategy) {
      case MEAN_SQUARED_ERROR:
	errors[popInd] =
	    (errorSum / evaluationSampleSize) +
	    (population[popInd]->getNodeCount() * parsimonyPressure);

	break;
      default:
	throw runtime_error("Unknown error strategy");
    }
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
  int evaluationSampleSize;
  double tuneConstantProbability;
  double parsimonyPressure;
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
    if (Tree::getRandomDouble(0, 1) < conf.mutationRate) {
      population[i]->mutate();
    }
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
	  depth, conf.numVars, conf.chooseConstantProbability,
	  conf.tuneConstantProbability);

      population[index++]->grow();
    }

    // grow all the grow trees
    for (int g = 0; g < growStrategy.grow; g++) {
      assert(index < population.size() &&
	     "Population index out of bounds for grow");

      population[index] = make_unique<GrowTree>(
	  depth, conf.numVars, conf.chooseConstantProbability,
	  conf.prematureLeafProbability, conf.tuneConstantProbability);

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
			 cref(targets), ref(errors), errorStrategy,
			 conf.evaluationSampleSize, conf.parsimonyPressure);
  }

  // INFO: 2B) join
  for (auto& currThread : threads) {
    currThread.join();
  }
  threads.clear();

  // INFO: 3A) tournament selection (single threaded, just return indices)
  auto selRes = utils::tournamentSelection(errors, conf.tournamentSize);

  auto bestIndiv = population[selRes.bestOverallIndex]->clone();

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

  // move the best individual into a random spot
  population[Tree::getRandomInt(0, population.size() - 1)] = bestIndiv->clone();
}

void generationTest(const vector<vector<double>>& inputs,
		    const vector<double>& targets, vector<double>& errors,
		    const GrowStrategy& growStrategy,
		    vector<unique_ptr<Tree>>& population, Config& config) {
  cout << "Growing initial population..." << endl;
  // grow initial population
  growPopulation(population, config, growStrategy);

  // init best indivdual
  std::unique_ptr<Tree> overallBestIndividual = population[0]->clone();
  double overallLowestError = 100000;

  cout << "Starting program..." << endl;

  // evolve through generations
  for (int i = 1; i <= config.generations; i++) {
    cout << "----------------- GENERATION " << i << " -------------------------"
	 << endl;

    // call generation to continue after initial grow
    generation(population, inputs, targets, errors, config,
	       ErrorStrategy::MEAN_SQUARED_ERROR, overallBestIndividual,
	       overallLowestError);

    cout << "BEST: " << overallBestIndividual->toString(inputs[2]) << "[ error "
	 << overallLowestError << " ]" << endl;
  }
}

struct ValidationResult {
  double avgMSE;
  double bestMSE;
  double worstMSE;
  double stdDev;
  double medianMSE;
};

ValidationResult validatePopulation(const vector<vector<double>>& inputs,
				    const vector<double>& targets,
				    vector<unique_ptr<Tree>>& population) {
  // take each of the individuals in the population and get the MSE per
  // individual
  ValidationResult res = {.avgMSE = 0,
			  .bestMSE = 10000000,
			  .worstMSE = 0,
			  .stdDev = 0,
			  .medianMSE = 0};

  assert((inputs.size() == targets.size()) &&
	 "Inputs, targets and errors are not the same size");

  vector<double> mse;

  // for each individual in the population
  for (const auto& indiv : population) {
    double errorSum = 0;

    // get the total error sum
    for (size_t i = 0; i < inputs.size(); i++) {
      double predicted = indiv->evaluate(inputs[i]);
      double difference = predicted - targets[i];
      errorSum += difference * difference;
    }

    // add to the end of the array
    mse.push_back(errorSum / targets.size());
  }

  double totalError = 0;

  // get the average, best and worst
  for (const auto& err : mse) {
    if (err < res.bestMSE) res.bestMSE = err;
    if (err > res.worstMSE) res.worstMSE = err;
    totalError += err;
  }

  res.avgMSE = totalError / population.size();

  res.stdDev = utils::calculateSD(mse);

  std::ranges::sort(mse.begin(), mse.end());

  res.medianMSE = mse[mse.size() / 2];

  return res;
}

int main() {
  cout << "Reading csv..." << endl;
  // Init data processor
  DataProcessor dataProcessor;

  dataProcessor.readCSV("./dataset/training.csv");
  vector<vector<double>> trainingInputs = dataProcessor.getInputs();
  vector<double> trainingTargets = dataProcessor.getTargets();

  cout << "Done training..." << endl;

  dataProcessor.readCSV("./dataset/validation.csv");
  vector<vector<double>> validationInputs = dataProcessor.getInputs();
  vector<double> validationTargets = dataProcessor.getTargets();

  cout << "Done validation..." << endl;

  dataProcessor.readCSV("./dataset/test.csv");
  vector<vector<double>> testInputs = dataProcessor.getInputs();
  vector<double> testTargets = dataProcessor.getTargets();

  cout << "Done ..." << endl;

  // ----------------------------- CONFIG ----------------------- //
  const int SEED = 1010;
  Tree::engine.seed(SEED);

  GrowStrategy growStrategy = {
      .minDepth = 2, .maxDepth = 8, .fullGrow = 1000, .grow = 1000};

  const int POP_SIZE = (growStrategy.fullGrow + growStrategy.grow) *
		       (growStrategy.maxDepth - growStrategy.minDepth + 1);

  Tree::highestConstant = 5;
  Tree::smallestConstant = -5;

  Config config = {.populationSize = POP_SIZE,
		   .numThreads = 8,
		   .generations = 100,
		   .chooseConstantProbability = 0.7,
		   .tournamentSize = 3,
		   .numVars = static_cast<int>(trainingInputs[0].size()),
		   .prematureLeafProbability = 0.5,
		   .crossoverRate = 0.7,
		   .mutationRate = 0.35,
		   .evaluationSampleSize = 30000,
		   .tuneConstantProbability = 0.7,
		   .parsimonyPressure = 0.00008};
  // ------------------------------------------------------------ //

  chrono::steady_clock::time_point t1 = chrono::steady_clock::now();

  // allocate popluation
  vector<unique_ptr<Tree>> population;
  population.resize(config.populationSize);

  vector<double> tempErrors;
  tempErrors.resize(config.populationSize);

  generationTest(trainingInputs, trainingTargets, tempErrors, growStrategy,
		 population, config);

  chrono::steady_clock::time_point t2 = chrono::steady_clock::now();

  chrono::duration<double> duration =
      duration_cast<chrono::duration<double>>(t2 - t1);

  cout << "Calculating results..." << endl;

  // test population on validation set
  auto validationResults =
      validatePopulation(validationInputs, validationTargets, population);

  cout << "-------------------- RESULTS ---------------------- " << endl;
  cout << "seed,bestMSE,worstMSE,medianMSE,avgMSE,stdDevMSE,smallestConstant,"
	  "highestConstant,"
	  "minDepth,maxDepth,fullGrow,grow,popSize,generations,tournamentSize,"
	  "prematureLeafProbability,mutationRate,crossoverRate,"
	  "tuneConstantProbability,runtimeS"
       << endl;
  cout << SEED << "," << validationResults.bestMSE << ","
       << validationResults.worstMSE << "," << validationResults.medianMSE
       << "," << validationResults.avgMSE << "," << validationResults.stdDev
       << "," << Tree::smallestConstant << "," << Tree::highestConstant << ","
       << growStrategy.minDepth << "," << growStrategy.maxDepth << ","
       << growStrategy.fullGrow << "," << growStrategy.grow << "," << POP_SIZE
       << "," << config.generations << "," << config.tournamentSize << ","
       << config.prematureLeafProbability << "," << config.mutationRate << ","
       << config.crossoverRate << "," << config.tuneConstantProbability << ","
       << duration.count() << endl;
  cout << "--------------------------------------------------- " << endl;

  return 0;
}
