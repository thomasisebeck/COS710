#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "DataProccessor.h"
#include "FullGrowTree.h"
#include "GrowTree.h"
#include "utilities.h"

using namespace std;
namespace fs = std::filesystem;

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
  double highestStoppingError;
  double highestHitError;
};

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
	      const Config& conf, vector<int>& threadHits,
	      const int& threadHitIndex) {
  assert(inputs.size() == targets.size() && "targets inputs size mismatch");

  threadHits[threadHitIndex] = 0;

  vector<int> evaluationSampleIndices;
  evaluationSampleIndices.reserve(conf.evaluationSampleSize);

  // create the evaluation criteria
  for (size_t currTargetInd = 0; currTargetInd < conf.evaluationSampleSize;
       currTargetInd++) {
    evaluationSampleIndices.push_back(Tree::getRandomInt(0, inputs.size() - 1));
  }

  assert(evaluationSampleIndices.size() == conf.evaluationSampleSize);

  // do the action my individuals
  for (int popInd = startInd; popInd < endIndExclusive; popInd++) {
    double errorSum = 0;

    // loop through all the targets
    for (size_t currTargetInd = 0; currTargetInd < conf.evaluationSampleSize;
	 currTargetInd++) {
      int select = evaluationSampleIndices[currTargetInd];
      double value = population[popInd]->evaluate(inputs[select]);
      double currError = value - targets[select];

      // get the error based on the error strategy
      errorSum += (currError * currError);
    }

    errors[popInd] =
	(errorSum / conf.evaluationSampleSize) +
	(population[popInd]->getNodeCount() * conf.parsimonyPressure);

    if (errors[popInd] < conf.highestHitError) threadHits[threadHitIndex]++;
  }
}

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

struct generationRet {
  vector<unique_ptr<Tree>> overallBestIndividual;
  double overallLowestError;
};

struct GenerationRet {
  bool mustStop;
  int hits;
};

// assumes that the initial trees are already grown
GenerationRet generation(vector<unique_ptr<Tree>>& population,
			 const vector<vector<double>>& inputs,
			 const vector<double>& targets, vector<double>& errors,
			 const vector<vector<double>>& validationInputs,
			 const vector<double>& validationTargets,
			 vector<double>& validationErrors, Config& conf,
			 unique_ptr<Tree>& fittestIndivdual,
			 double& fittestErr) {
  assert(population.size() % 2 == 0 &&
	 "Population size must be divisible by 2");

  GenerationRet toRet = {.mustStop = false, .hits = 0};

  // list of threads
  vector<thread> threads;
  threads.reserve(conf.numThreads);

  auto indices = utils::getThreadIndices(population.size(), conf.numThreads);

  // -----------------------------------------------------

  vector<int> threadHits(conf.numThreads, 0);

  // INFO: 2A) spawn threads to evaluate the whole buffer
  for (int i = 0; i < conf.numThreads; i++) {
    auto [start, end] = indices[i];

    threads.emplace_back(&evaluate, start, end, ref(population), cref(inputs),
			 cref(targets), ref(errors), cref(conf),
			 ref(threadHits), i);
  }

  // INFO: 2B) join
  for (auto& currThread : threads) {
    currThread.join();
  }
  threads.clear();

  // accumulate thread hits
  toRet.hits = std::accumulate(threadHits.begin(), threadHits.end(), 0);

  // INFO: 3A) tournament selection (single threaded, just return indices)
  auto selRes = utils::tournamentSelection(errors, conf.tournamentSize);

  // new fittest individual, update the values
  if (errors[selRes.bestOverallIndex] < fittestErr) {
    // clone out the new champion
    fittestIndivdual = population[selRes.bestOverallIndex]->clone();
    fittestErr = errors[selRes.bestOverallIndex];
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

  // overwrite the worst individual with the best one
  int replaceMe =
      std::ranges::max_element(errors.begin(), errors.end()) - errors.begin();
  population[replaceMe] = fittestIndivdual->clone();

  vector<int> bestHits(1);

  evaluate(replaceMe, replaceMe + 1, population, validationInputs,
	   validationTargets, validationErrors, conf, bestHits, 0);

  // to test
  if (validationErrors[replaceMe] <= conf.highestStoppingError) {
    cout << "Stopping criteria reached..." << endl;
    cout << "Best individual: " << endl;
    cout << population[replaceMe]->toString(inputs[0]) << endl;
    toRet.mustStop = true;
  }
  // don't stop
  return toRet;
}

void generationTest(const vector<vector<double>>& inputs,
		    const vector<double>& targets, vector<double>& errors,
		    const vector<vector<double>>& validationInputs,
		    const vector<double>& validationTargets,
		    vector<double>& validationErrors,
		    const GrowStrategy& growStrategy,
		    vector<unique_ptr<Tree>>& population, Config& config,
		    vector<int>& hitsPerGerenation) {
  // grow initial population
  growPopulation(population, config, growStrategy);

  // init best indivdual
  auto fittestIndividual = population[0]->clone();
  double fittestErr = 100000;

  // evolve through generations
  for (int i = 0; i < config.generations; i++) {
    // call generation to continue after initial grow
    auto check =
	generation(population, inputs, targets, errors, validationInputs,
		   validationTargets, validationErrors, config,
		   fittestIndividual, fittestErr);

    hitsPerGerenation.push_back(check.hits);

    if (check.mustStop) {
      cout << "Exiting..." << endl;
      break;
    }
  }
}

struct ValidationResult {
  double avgMSE;
  double bestMSE;
  double worstMSE;
  double stdDev;
  double medianMSE;
  int bestIndividualIndex;
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
			  .medianMSE = 0,
			  .bestIndividualIndex = 0};

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
  for (size_t i = 0; i < mse.size(); ++i) {
    const auto& err = mse[i];
    if (err < res.bestMSE) {
      res.bestMSE = err;
      res.bestIndividualIndex = i;
    }
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
  // Init data processor
  DataProcessor dataProcessor;

  dataProcessor.readCSV("./dataset/training.csv");
  vector<vector<double>> trainingInputs = dataProcessor.getInputs();
  vector<double> trainingTargets = dataProcessor.getTargets();

  dataProcessor.readCSV("./dataset/validation.csv");
  vector<vector<double>> validationInputs = dataProcessor.getInputs();
  vector<double> validationTargets = dataProcessor.getTargets();

  dataProcessor.readCSV("./dataset/test.csv");
  vector<vector<double>> testInputs = dataProcessor.getInputs();
  vector<double> testTargets = dataProcessor.getTargets();

  if (!fs::exists("../results")) {
    cout << "creating results dir..." << endl;
    fs::create_directories("../results");
  }

  // ----------------------------- CONFIG ----------------------- //
  GrowStrategy growStrategy = {
      .minDepth = 2, .maxDepth = 5, .fullGrow = 25, .grow = 25};

  const int POP_SIZE = (growStrategy.fullGrow + growStrategy.grow) *
		       (growStrategy.maxDepth - growStrategy.minDepth + 1);

  Tree::highestConstant = 2;
  Tree::smallestConstant = -2;

  Config config = {.populationSize = POP_SIZE,
		   .numThreads = 8,
		   .generations = 300,
		   .chooseConstantProbability = 0.5,
		   .tournamentSize = 3,
		   .numVars = static_cast<int>(trainingInputs[0].size()),
		   .prematureLeafProbability = 0.25,
		   .crossoverRate = 0.7,
		   .mutationRate = 0.35,
		   .evaluationSampleSize = 100000,
		   .tuneConstantProbability = 0.5,
		   .parsimonyPressure = 0.00008,
		   .highestStoppingError = 0.00999,
		   .highestHitError = 0.012};
  // ------------------------------------------------------------ //

  for (int trySeed = 1000; trySeed <= 9999; trySeed++) {
    Tree::seed = trySeed;
    Tree::engine.seed(Tree::seed);

    chrono::steady_clock::time_point t1 = chrono::steady_clock::now();

    // allocate popluation
    vector<unique_ptr<Tree>> population;
    population.resize(config.populationSize);

    vector<double> trainingErrors;
    trainingErrors.resize(config.populationSize);

    vector<double> validationErrors;
    validationErrors.resize(config.populationSize);

    vector<int> hitsPerGeneration;
    validationErrors.resize(config.generations);

    generationTest(trainingInputs, trainingTargets, trainingErrors,
		   validationInputs, validationTargets, validationErrors,
		   growStrategy, population, config, hitsPerGeneration);

    chrono::steady_clock::time_point t2 = chrono::steady_clock::now();

    chrono::duration<double> duration =
	duration_cast<chrono::duration<double>>(t2 - t1);

    // test population on validation set
    auto validationResults =
	validatePopulation(validationInputs, validationTargets, population);

    // 1. Prepare the strings
    std::string bestIndividual =
	"\"" +
	population[validationResults.bestIndividualIndex]->toString(
	    testInputs[0]) +
	"\"";
    std::string hitsHistory =
	"\"" + utils::vectorToString(hitsPerGeneration) + "\"";
    std::string bestNodeCount = std::to_string(
	population[validationResults.bestIndividualIndex]->getNodeCount());

    // 2. Open the file named after the seed
    std::string filename = std::to_string(Tree::seed) + ".txt";
    std::ofstream outFile("../results/" + filename);

    cout << "writing..." << endl;

    if (outFile.is_open()) {
      // Header row
      outFile
	  << "bestMSE,worstMSE,medianMSE,avgMSE,stdDevMSE,smallestConstant,"
	     "highestConstant,minDepth,maxDepth,fullGrow,grow,popSize,"
	     "generations,tournamentSize,prematureLeafProbability,mutationRate,"
	     "crossoverRate,tuneConstantProbability,runtimeS,"
	     "bestIndividualNodeCount,bestIndividual,hitsPerGen"
	  << std::endl;

      // Data row
      outFile << validationResults.bestMSE << "," << validationResults.worstMSE
	      << "," << validationResults.medianMSE << ","
	      << validationResults.avgMSE << "," << validationResults.stdDev
	      << "," << Tree::smallestConstant << "," << Tree::highestConstant
	      << "," << growStrategy.minDepth << "," << growStrategy.maxDepth
	      << "," << growStrategy.fullGrow << "," << growStrategy.grow << ","
	      << POP_SIZE << "," << config.generations << ","
	      << config.tournamentSize << "," << config.prematureLeafProbability
	      << "," << config.mutationRate << "," << config.crossoverRate
	      << "," << config.tuneConstantProbability << ","
	      << duration.count() << "," << bestNodeCount << ","
	      << bestIndividual << ","	// Wrapped in quotes
	      << hitsHistory		// Wrapped in quotes
	      << std::endl;

      outFile.close();
    }
  }

  return 0;
}
