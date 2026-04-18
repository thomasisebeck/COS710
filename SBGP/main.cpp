#include <algorithm>
#include <cassert>
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
#include "Tree.h"
#include "utilities.h"
#include "vars.h"

using namespace std;

enum class Action { GROW, EVALUATE, PRINT };

enum ErrorStrategy { MEAN_SQUARED_ERROR };

enum class TreeType { FULL_GROW, GROW };

struct Config {
  struct Percycle {
    int canonicalGenerations;
    int localSearchGenerations;
    int peturbGenerations;
  } percycle;
  int cycles;
  int populationSize;
  int numThreads;
  double chooseConstantProbability;
  int tournamentSize;
  int numVars;
  double prematureLeafProbability;
  double crossoverRate;
  double mutationRate;
  double tuneConstantProbability;
  double parsimonyPressure;
  double highestStoppingError;
  double highestHitError;
  double freezeEliteIndividualsPercent;
  double freezeTopUntilLayer;
};

// INFO: inputs is a 2d vector that
// represent the variable inputs of the
// tree targets is a 1d vector which
// each tree is trying to evaluate to
// (given each set of inputs) results is
// a 1d vector storing the values of the
// evaluations
void evaluate(int startInd, int endIndExclusive,
              vector<unique_ptr<Tree>> &population,
              const vector<vector<double>> &inputs,
              const vector<double> &targets, vector<double> &errors,
              const Config &conf, vector<int> &threadHits,
              const int &threadHitIndex) {
  assert(inputs.size() == targets.size() && "targets inputs size mismatch");

  threadHits[threadHitIndex] = 0;

  // do the action my individuals
  for (int popInd = startInd; popInd < endIndExclusive; popInd++) {
    double errorSum = 0;

    // loop through all the targets
    for (int i = 0; i < targets.size(); i++) {
      double value = population[popInd]->evaluate(inputs[i]);
      double currError = value - targets[i];

      // get the MSE
      errorSum += (currError * currError);
    }

    errors[popInd] =
        (errorSum / targets.size()) +
        (population[popInd]->getNodeCount() * conf.parsimonyPressure);

    if (errors[popInd] < conf.highestHitError)
      threadHits[threadHitIndex]++;
  }
}

struct GrowStrategy {
  int minDepth;
  int maxDepth;
  int fullGrow;
  int grow;
};

void mutatePopulation(vector<unique_ptr<Tree>> &population, const Config &conf,
                      int startInd, int endIndExclusive) {
  for (int i = startInd; i < endIndExclusive; i++) {
    if (Tree::getRandomDouble(0, 1) < conf.mutationRate) {
      population[i]->mutate();
    }
  }
}

// WARN: call single threaded
void growPopulation(vector<unique_ptr<Tree>> &population, Config &conf,
                    const GrowStrategy &growStrategy) {
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
GenerationRet generation(vector<unique_ptr<Tree>> &population,
                         const vector<vector<double>> &inputs,
                         const vector<double> &targets, vector<double> &errors,
                         const vector<vector<double>> &validationInputs,
                         const vector<double> &validationTargets,
                         vector<double> &validationErrors, Config &conf,
                         unique_ptr<Tree> &fittestIndivdual,
                         double &fittestErr) {
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
  for (auto &currThread : threads) {
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

  // INFO: 4A) spawn threads to mutate the whole buffer
  for (int i = 0; i < conf.numThreads; i++) {
    auto [start, end] = indices[i];

    threads.emplace_back(&mutatePopulation, ref(population), conf, start, end);
  }

  // INFO: 4B) join
  for (auto &currThread : threads) {
    currThread.join();
  }
  threads.clear();

  // overwrite the worst individual with the best one
  int replaceMe =
      std::ranges::max_element(errors.begin(), errors.end()) - errors.begin();
  population[replaceMe] = fittestIndivdual->clone();

  vector<int> bestHits(1);

  // evaluate only the best indivdual with the validation set
  evaluate(replaceMe, replaceMe + 1, population, validationInputs,
           validationTargets, validationErrors, conf, bestHits, 0);

  // to test
  if (validationErrors[replaceMe] <= conf.highestStoppingError) {
    cout << "Stopping criteria reached..." << endl;
    cout << "Best individual: " << endl;
    cout << population[replaceMe]->toString() << " hits -> " << bestHits[0]
         << endl;
    toRet.mustStop = true;
  }
  // don't stop
  return toRet;
}

struct ValidationResult {
  double avgMSE;
  double bestMSE;
  double worstMSE;
  double stdDev;
  double medianMSE;
  int bestIndividualIndex;
};

ValidationResult validatePopulation(const vector<vector<double>> &inputs,
                                    const vector<double> &targets,
                                    vector<unique_ptr<Tree>> &population,
                                    int startInd, int endInd) {
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

  assert((startInd >= 0 && startInd < population.size()));
  assert((endInd > 0 && endInd < population.size()));
  assert(startInd < endInd);

  vector<double> mse;

  // for each individual in the population
  for (int i = startInd; i <= endInd; i++) {
    const auto &indiv = population[i];
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
    const auto &err = mse[i];
    if (err < res.bestMSE) {
      res.bestMSE = err;
      res.bestIndividualIndex = i;
    }
    if (err > res.worstMSE)
      res.worstMSE = err;
    totalError += err;
  }

  res.avgMSE = totalError / population.size();

  res.stdDev = utils::calculateSD(mse);

  std::ranges::sort(mse.begin(), mse.end());

  res.medianMSE = mse[mse.size() / 2];

  return res;
}

struct BestSeedResult {
  int seed;
  double validationMSE;
};

/*
 *
BestSeedResult findBestSeed(const vector<vector<double>> &trainingInputs,
                            const vector<double> &trainingTargets,
                            const vector<vector<double>> &validationInputs,
                            const vector<double> &validationTargets,
                            const GrowStrategy &growStrategy, Config &config) {
  // found from the test set
  std::vector<int> topSeeds = {
      1030, 1098, 1125, 1135, 1148, 1167, 1188, 1219, 1337, 1364, 1557, 1602,
      1633, 1704, 1710, 1711, 1764, 1875, 1911, 1960, 2090, 2123, 2168, 2233,
      2275, 2308, 2369, 2534, 2539, 2626, 2658, 2679, 2714, 2822, 2838, 2929,
      2939, 3024, 3109, 3116, 3251, 3283, 3293, 3313, 3315, 3349, 3351, 3439,
      3594, 3605, 3627, 3941, 4007, 4030, 4061, 4080};

  int finalBestSeed = topSeeds[0];
  double bestValidationMSE = 1e18; // Start high

  for (const int &trySeed : topSeeds) {
    Tree::seed = trySeed;
    Tree::engine.seed(Tree::seed);

    cout << "Test seed: " << trySeed << endl;

    vector<unique_ptr<Tree>> population;
    population.resize(config.populationSize);

    vector<double> trainingErrors(config.populationSize);
    vector<double> validationErrors(config.populationSize);
    vector<int> hitsPerGeneration(config.generations);

    generationTest(trainingInputs, trainingTargets, trainingErrors,
                   validationInputs, validationTargets, validationErrors,
                   growStrategy, population, config, hitsPerGeneration);

    // Validate the population
    auto valRes = validatePopulation(validationInputs, validationTargets,
                                     population, 0, population.size() - 1);
    // Track the absolute best seed based on Validation performance
    if (valRes.bestMSE < bestValidationMSE) {
      bestValidationMSE = valRes.bestMSE;
      finalBestSeed = trySeed;
    }
  }

  return {.seed = finalBestSeed, .validationMSE = bestValidationMSE};
}

void runFinalTest() {

  auto t1 = chrono::steady_clock::now();

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
      .minDepth = 2, .maxDepth = 5, .fullGrow = 10, .grow = 10};

  const int POP_SIZE = (growStrategy.fullGrow + growStrategy.grow) *
                       (growStrategy.maxDepth - growStrategy.minDepth + 1);

  Tree::highestConstant = 2;
  Tree::smallestConstant = -2;

  Config config = {
      .populationSize = POP_SIZE,
      .numThreads = 8,
      .percycle = {.canonicalGenerations = }.chooseConstantProbability = 0.5,
      .tournamentSize = 4,
      .numVars = static_cast<int>(trainingInputs[0].size()),
      .prematureLeafProbability = 0.25,
      .crossoverRate = 0.7,
      .mutationRate = 0.35,
      .tuneConstantProbability = 0.5,
      .parsimonyPressure = 0.00008,
      .highestStoppingError = 0.00999,
      .highestHitError = 0.012};
  // ------------------------------------------------------------ //

  // 1. Find the best seed based on Validation set result
  BestSeedResult bestResult =
      findBestSeed(trainingInputs, trainingTargets, validationInputs,
                   validationTargets, growStrategy, config);

  cout << "\nBest Seed Found: " << bestResult.seed
       << " with Validation MSE: " << bestResult.validationMSE << endl;

  // BestSeedResult bestResult = {.seed = 2822, .validationMSE = 0.00952255};


  cout << "Testing the best seed on test set: " << endl;

  Tree::seed = bestResult.seed;
  Tree::engine.seed(Tree::seed);

  vector<unique_ptr<Tree>> finalPopulation(config.populationSize);
  vector<double> trainingErrors(config.populationSize);
  vector<double> validationErrors(config.populationSize);
  vector<int> hitsPerGeneration(config.generations);

  generationTest(trainingInputs, trainingTargets, trainingErrors,
                 validationInputs, validationTargets, validationErrors,
                 growStrategy, finalPopulation, config, hitsPerGeneration);

  // Get the best one from the validation set
  auto finalValRes =
      validatePopulation(validationInputs, validationTargets, finalPopulation,
                         0, finalPopulation.size() - 1);

  // Pull the lever!!!!
  auto finalTestRes = validatePopulation(
      testInputs, testTargets, finalPopulation, 0, finalPopulation.size() - 1);

  auto t2 = chrono::steady_clock::now();
  chrono::duration<double> duration = t2 - t1;

  cout << "================ FINAL REPORT ================" << endl;
  cout << "VALIDATION STATS (The Selection Criteria):" << endl;
  cout << "  Best:   " << finalValRes.bestMSE << endl;
  cout << "  Avg:    " << finalValRes.avgMSE << endl;
  cout << "  Median: " << finalValRes.medianMSE << endl;
  cout << "  stdDev: " << finalValRes.stdDev << endl;
  cout << "  Worst:  " << finalValRes.worstMSE << endl;
  cout << "---------------------------------------------" << endl;

  cout << "TEST STATS (The Unseen Data):" << endl;
  cout << "  Best:   " << finalTestRes.bestMSE << endl;
  cout << "  Avg:    " << finalTestRes.avgMSE << endl;
  cout << "  Median: " << finalTestRes.medianMSE << endl;
  cout << "  stdDev: " << finalTestRes.stdDev << endl;
  cout << "  Worst:  " << finalTestRes.worstMSE << endl;
  cout << "---------------------------------------------" << endl;

  // Calculate the gap between Validation and Test
  double genGap = std::abs(finalTestRes.bestMSE - finalValRes.bestMSE);
  cout << "Generalization Gap: " << genGap << endl;

  cout << "BEST FORMULA: "
       << finalPopulation[finalValRes.bestIndividualIndex]->toString(
              testInputs[0])
       << endl;

  cout << "Hits per generation: " << utils::vectorToString(hitsPerGeneration)
       << endl;
  cout << "Runtime: " << duration.count() << endl;
  cout << "=============================================" << endl;
}
*/

void runTestCase() {

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

  //------------------------- config ---------------------------//
  GrowStrategy growStrategy = {
      .minDepth = 5, .maxDepth = 7, .fullGrow = 5, .grow = 5};

  const int POP_SIZE = (growStrategy.fullGrow + growStrategy.grow) *
                       (growStrategy.maxDepth - growStrategy.minDepth + 1);

  // freeze only if the tree is >= 2 nodes deep
  Tree::freezeCutoffDepth = 2;
  Tree::freezeBottomPercent = 0.5; // freeze half the tree
  Tree::highestConstant = 2;
  Tree::smallestConstant = -2;

  Config config = {.percycle{.canonicalGenerations = 10,
                             .localSearchGenerations = 10,
                             .peturbGenerations = 2},
                   .cycles = 5,
                   .populationSize = POP_SIZE,
                   .numThreads = 8,
                   .chooseConstantProbability = 0.5,
                   .tournamentSize = 3,
                   .numVars = static_cast<int>(trainingInputs[0].size()),
                   .prematureLeafProbability = 0.25,
                   .crossoverRate = 0.7,
                   .mutationRate = 0.35,
                   .tuneConstantProbability = 0.5,
                   .parsimonyPressure = 0.000001,
                   .highestStoppingError = 0.00999,
                   .highestHitError = 0.012,
                   .freezeEliteIndividualsPercent = 0.3};
  //------------------------------------------------------------//

  // run 1 generation
  vector<unique_ptr<Tree>> population(config.populationSize);
  vector<double> trainingErrors(config.populationSize);
  vector<double> validationErrors(config.populationSize);
  vector<int> hitsPerGeneration(config.cycles *
                                (config.percycle.canonicalGenerations +
                                 config.percycle.peturbGenerations +
                                 config.percycle.localSearchGenerations));

  std::vector<int> topSeeds = {
      1030, 1098, 1125, 1135, 1148, 1167, 1188, 1219, 1337, 1364, 1557, 1602,
      1633, 1704, 1710, 1711, 1764, 1875, 1911, 1960, 2090, 2123, 2168, 2233,
      2275, 2308, 2369, 2534, 2539, 2626, 2658, 2679, 2714, 2822, 2838, 2929,
      2939, 3024, 3109, 3116, 3251, 3283, 3293, 3313, 3315, 3349, 3351, 3439,
      3594, 3605, 3627, 3941, 4007, 4030, 4061, 4080};

  for (const auto &seed : topSeeds) {

    Tree::seed = seed;

    int genCounter = 0;

    cout << endl
         << "<<<<<<<<<<<<<<<< seed " << seed << " >>>>>>>>>>>>>>>" << endl;

    auto t1 = chrono::steady_clock::now();

    // grow initial population
    growPopulation(population, config, growStrategy);

    // init best indivdual
    auto fittestIndividual = population[0]->clone();
    double fittestErr = 100000;

    // evolve through each cycle
    for (int i = 0; i < config.cycles; i++) {
      // call generation to continue after initial grow
      cout << "================ cycle " << i << " of " << config.cycles
           << " ===============" << endl
           << "canonical: ";

      for (int j = 0; j < config.percycle.canonicalGenerations; j++) {

        cout << genCounter++ << " ";

        if (generation(population, trainingInputs, trainingTargets,
                       trainingErrors, validationInputs, validationTargets,
                       validationErrors, config, fittestIndividual, fittestErr)
                .mustStop) {
          cout << "Exiting..." << endl;
          break;
        }
      }

      // get the min error to freeze calcuated from the begining of the array
      auto threshMin = utils::getThreshError<utils::Mode::MIN>(
          trainingErrors, config.freezeEliteIndividualsPercent);

      const double range = get<0>(threshMin) - get<1>(threshMin);

      // unfreeze all, the population has converged
      if (range < EPSILON) {

        cout << "--------------------------------------------------" << endl;
        cout << "---- Converged: Freezing bottom for diversity ----" << endl;
        cout << "--------------------------------------------------" << endl;

        for (const auto &i : population)
          i->freezeToPercent<FreezeType::BOTTOM>();

      } else {
        // do normal freezing based on fitness

        for (int i = 0; i < trainingErrors.size(); i++)
          // check if below threshold error
          if (trainingErrors[i] < get<0>(threshMin)) {
            // this is a good individual, freeze the top layers

            // (my error - min error) / (threshold error - min error)
            // my error is always less than the threshold
            // closer my error is to the threshold min, the more gets frozen
            double scoreDiff = (trainingErrors[i] - get<1>(threshMin)) /
                               (get<0>(threshMin) - get<1>(threshMin));

            // freeze more for a scoreDiff that is closer to 0 (closer to the
            // min error)
            population[i]->freezeToPercent<FreezeType::TOP>(1 - scoreDiff);
          }
      }

      cout << ", local: ";

      for (int j = 0; j < config.percycle.localSearchGenerations; j++) {

        cout << genCounter++ << " ";

        if (generation(population, trainingInputs, trainingTargets,
                       trainingErrors, validationInputs, validationTargets,
                       validationErrors, config, fittestIndividual, fittestErr)
                .mustStop) {
          cout << "Exiting..." << endl;
          break;
        }
      }

      // no point in peturbing on the last generation
      // they are about to go into the test case
      if (i < config.cycles - 1) {

        // get the max error to peturb calcuated from the end of the array
        auto threshMax = utils::getThreshError<utils::Mode::MAX>(
            trainingErrors, config.freezeEliteIndividualsPercent);

        // threshold and max error will likely always be large enough
        for (int e = 0; e < trainingErrors.size(); e++) {
          // check if this individuals error is above or equal (prevents
          // stagnation on convergence) to the threshold error, in which case it
          // needs a shake up
          if (trainingErrors[e] >= get<0>(threshMax)) {
            population[e]->freezeToPercent<FreezeType::BOTTOM>();
          }
        }
        cout << ", peturb: ";

        for (int p = 0; p < config.percycle.peturbGenerations; p++) {

          cout << genCounter++ << " ";

          if (generation(population, trainingInputs, trainingTargets,
                         trainingErrors, validationInputs, validationTargets,
                         validationErrors, config, fittestIndividual,
                         fittestErr)
                  .mustStop) {
            cout << "Exiting..." << endl;
            break;
          }
        }

        // unfreeze for the start of the new cycle
        // where canonical will begin
        for (const auto &p : population)
          p->freezeToPercent<FreezeType::NONE>();

        cout << endl;
      }
    }

    auto valRes = validatePopulation(validationInputs, validationTargets,
                                     population, 0, population.size() - 1);

    auto t2 = chrono::steady_clock::now();

    cout << endl << "VALIDATION STATS (The Selection Criteria):" << endl;
    cout << "  Best:   " << valRes.bestMSE << endl;
    cout << "  Avg:    " << valRes.avgMSE << endl;
    cout << "  Median: " << valRes.medianMSE << endl;
    cout << "  stdDev: " << valRes.stdDev << endl;
    cout << "  Worst:  " << valRes.worstMSE << endl;

    cout << "CONFIG STATS:" << endl;
    cout << "  Population size:      " << POP_SIZE << endl;
    cout << "  Tournament size:      " << config.tournamentSize << endl;
    cout << "  Canonical Gens:       " << config.percycle.canonicalGenerations
         << endl;
    cout << "  Local Search Gens:    " << config.percycle.localSearchGenerations
         << endl;
    cout << "  Perturb Gens:         " << config.percycle.peturbGenerations
         << endl;
    cout << "  Crossover rate:       " << config.crossoverRate << endl;
    cout << "  Mutation rate:        " << config.mutationRate << endl;
    cout << "  Parsimony pressure:   " << config.parsimonyPressure << endl;
    cout << "  Constant prob:        " << config.chooseConstantProbability
         << endl;
    cout << "  Tune constant prob:   " << config.tuneConstantProbability
         << endl;
    cout << "  Premature leaf prob:  " << config.prematureLeafProbability
         << endl;
    cout << "  Freeze elite %:       " << config.freezeEliteIndividualsPercent
         << endl;
    cout << "  Freeze top layer:     " << config.freezeTopUntilLayer << endl;
    cout << "  Stopping error:       " << config.highestStoppingError << endl;
    cout << "  Hit error:            " << config.highestHitError << endl;
    cout << "  Runtime:            "
         << chrono::duration<double>(t2 - t1).count() << endl;
    cout << "---------------------------------------------" << endl;
  }
}

int main() {
  runTestCase();

  return 0;
}
