#include "GE.h"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "DataProccessor.h"
#include "FullGrowTree.h"
#include "GrowTree.h"
#include "Tree.h"
#include "utilities.h"

namespace fs = std::filesystem;
using namespace std;

enum class Action { GROW, EVALUATE, PRINT };

enum ErrorStrategy { MEAN_SQUARED_ERROR };

enum class TreeType { FULL_GROW, GROW };

struct Config {
  struct Percycle {
    int localSearchGenerations;
    int peturbGenerations;
  } percycle;
  int cycles;
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

enum class OperationMode { TREE = 0, GE = 1 };

// INFO: inputs is a 2d vector that
// represent the variable inputs of the
// tree targets is a 1d vector which
// each tree is trying to evaluate to
// (given each set of inputs) results is
// a 1d vector storing the values of the
// evaluations
template <class TreeOrGenome>
void evaluate(int startInd, int endIndExclusive,
              vector<unique_ptr<TreeOrGenome>> &population,
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

template <class TreeOrGenome>
void mutatePopulation(vector<unique_ptr<TreeOrGenome>> &population,
                      const Config &conf, int startInd, int endIndExclusive) {
  for (int i = startInd; i < endIndExclusive; i++) {
    if (Tree::getRandomDouble(0, 1) < conf.mutationRate) {
      population[i]->mutate();
    }
  }
}

template <OperationMode Mode, class TreeOrGenome>
void growInitialPopulation(vector<unique_ptr<TreeOrGenome>> &population,
                           Config &conf, const GrowStrategy &growStrategy) {

  int index = 0;

  // loop through the min and max depths
  for (int depth = growStrategy.minDepth; depth <= growStrategy.maxDepth;
       depth++) {
    // grow all the fullgrow trees
    for (int f = 0; f < growStrategy.fullGrow; f++) {
      assert(index < population.size() &&
             "Population index out of bounds for fullgrow");

      if constexpr (Mode == OperationMode::GE) {
        // int depth, bool grow, int varSize
        population[index++] = make_unique<Genome>(depth, false, conf.numVars);
      } else {
        population[index++] = make_unique<FullGrowTree>(
            depth, conf.numVars, conf.chooseConstantProbability,
            conf.tuneConstantProbability);
      }
    }

    // grow all the grow trees
    for (int g = 0; g < growStrategy.grow; g++) {
      assert(index < population.size() &&
             "Population index out of bounds for grow");

      if constexpr (Mode == OperationMode::GE) {

        population[index++] = make_unique<Genome>(depth, true, conf.numVars);
      } else {
        population[index++] = make_unique<GrowTree>(
            depth, conf.numVars, conf.chooseConstantProbability,
            conf.prematureLeafProbability, conf.tuneConstantProbability);
      }
    }
  }

  assert(index == population.size() && "population not filled");

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

template <class TreeOrGenome, OperationMode ParsMode>
// assumes that the initial trees are already grown
GenerationRet generation(vector<unique_ptr<TreeOrGenome>> &population,
                         const vector<vector<double>> &inputs,
                         const vector<double> &targets, vector<double> &errors,
                         const vector<vector<double>> &validationInputs,
                         const vector<double> &validationTargets,
                         vector<double> &validationErrors, Config &conf,
                         unique_ptr<TreeOrGenome> &fittestIndivdual,
                         double &fittestErr) {
  assert(population.size() % conf.numThreads == 0 &&
         "Population size must be divisible by numthreads");

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

    threads.emplace_back(&evaluate<TreeOrGenome>, start, end, ref(population),
                         cref(inputs), cref(targets), ref(errors), cref(conf),
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
  vector<unique_ptr<TreeOrGenome>> nextGeneration;
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

    threads.emplace_back(&mutatePopulation<TreeOrGenome>, ref(population), conf,
                         start, end);
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
  evaluate<TreeOrGenome>(replaceMe, replaceMe + 1, population, validationInputs,
                         validationTargets, validationErrors, conf, bestHits,
                         0);

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

template <class TreeOrGenome>
ValidationResult validatePopulation(
    const vector<vector<double>> &inputs, const vector<double> &targets,
    vector<unique_ptr<TreeOrGenome>> &population, int startInd, int endInd) {
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
    vector<int> hitsPerGeneration(config.cycles *
                                  (config.percycle.canonicalGenerations *
                                   config.percycle.localSearchGenerations *
                                   config.percycle.peturbGenerations));
*/
/*
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
  cout << "Exiting..." <<
 */

//----------------------------------- run the generations
//---------------------//

/*
// grow initial population
growPopulation(population, config, growStrategy);

// init best indivdual
auto fittestIndividual = population[0]->clone();
double fittestErr = 100000;

for (int i = 0; i < config.generations; i++) {
// call generation to continue after initial grow
auto check =
    generation(population, inputs, targets, errors, validationInputs,
               validationTargets, validationErrors, config,
               fittestIndividual, fittestErr);



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

*/

void exportToCSV(string seed, const ValidationResult &valRes,
                 const ValidationResult &testRes, const Config &config,
                 const GrowStrategy &grow, double runtimeS, int bestNodeCount,
                 const string &bestIndividual, const vector<int> &hitsPerGen,
                 int populationSize) {

  if (!fs::exists("./results")) {
    cout << "creating results dir..." << endl;
    fs::create_directories("./results");
  }

  string filename = "./results/final.csv";
  bool fileExists = fs::exists(filename);

  // Open in append mode
  ofstream file(filename, ios::app);

  if (!file.is_open()) {
    cerr << "Failed to open " << filename << " for writing." << endl;
    return;
  }

  // If the file is new, write the header row first
  if (!fileExists) {
    file
        << "seed,valBestMSE,valWorstMSE,valMedianMSE,valAvgMSE,valStdDevMSE,"
        << "testBestMSE,testWorstMSE,testMedianMSE,testAvgMSE,testStdDevMSE,"
        << "smallestConstant,highestConstant,minDepth,maxDepth,fullGrow,grow,"
        << "popSize,canonicalGens,localSearchGens,perturbGens,cycles,"
        << "tournamentSize,prematureLeafProbability,mutationRate,crossoverRate,"
        << "tuneConstantProbability,parsimonyPressure,freezeElitePercent,"
        << "runtimeS,bestIndividualNodeCount,bestIndividual,hitsPerGen\n";
  }

  // Write the data row
  // Using defaultfloat and setting high precision to capture exact MSEs
  file << defaultfloat << setprecision(8);

  file << seed << "," << valRes.bestMSE << "," << valRes.worstMSE << ","
       << valRes.medianMSE << "," << valRes.avgMSE << "," << valRes.stdDev
       << "," << testRes.bestMSE << "," << testRes.worstMSE << ","
       << testRes.medianMSE << "," << testRes.avgMSE << "," << testRes.stdDev
       << "," << Tree::smallestConstant << "," << Tree::highestConstant << ","
       << grow.minDepth << "," << grow.maxDepth << "," << grow.fullGrow << ","
       << grow.grow << "," << populationSize << ","
       << config.percycle.localSearchGenerations << ","
       << config.percycle.peturbGenerations << "," << config.cycles << ","
       << config.tournamentSize << "," << config.prematureLeafProbability << ","
       << config.mutationRate << "," << config.crossoverRate << ","
       << config.tuneConstantProbability << "," << scientific
       << config.parsimonyPressure << defaultfloat << ","
       << config.freezeEliteIndividualsPercent << "," << fixed
       << setprecision(5) << runtimeS << "," << bestNodeCount << ","
       << "\"" << bestIndividual << "\",\""; // Wrap string in quotes

  // Format the hitsPerGen array as a string enclosed in brackets [ x y z ]
  file << "[ ";
  for (size_t i = 0; i < hitsPerGen.size(); i++) {
    file << hitsPerGen[i] << (i == hitsPerGen.size() - 1 ? "" : " ");
  }
  file << " ]\"\n"; // Close the string quotes and add newline

  file.close();
}

template <OperationMode Mode, class TreeOrGenome>
void runTestCase(Config &config, vector<GrowStrategy> growStrategies,
                 const vector<vector<double>> &trainingInputs,
                 const vector<double> &trainingTargets,
                 const vector<vector<double>> &validationInputs,
                 const vector<double> &validationTargets,
                 const vector<vector<double>> &testInputs,
                 const vector<double> &testTargets) {

  std::vector<int> topSeeds;

  const int START_SEED = 1001;
  const int END_SEED = 1002;

  for (int i = START_SEED; i <= END_SEED; i++) {
    topSeeds.push_back(i);
  }

  //------------------------------------------------------------//

  int fileName = 0;

  for (const auto &seed : topSeeds) {

    cout << endl
         << "<<<<<<<<<<<<<<<< seed " << seed << " >>>>>>>>>>>>>>>" << endl;

    for (const auto &strategy : growStrategies) {

      int POP_SIZE = (strategy.fullGrow + strategy.grow) *
                     (strategy.maxDepth - strategy.minDepth + 1);

      int TOTAL_GENERATIONS =
          (config.cycles * config.percycle.localSearchGenerations) +
          ((config.cycles - 1) * config.percycle.peturbGenerations);

      for (int i = 0; i < 2; i++) {

        vector<unique_ptr<TreeOrGenome>> population(POP_SIZE);
        vector<double> trainingErrors(POP_SIZE);
        vector<double> validationErrors(POP_SIZE);
        vector<int> hitsPerGeneration;
        hitsPerGeneration.reserve(TOTAL_GENERATIONS);

        Tree::seed = seed;
        Tree::engine.seed(Tree::seed);

        int genCounter = 0;

        auto t1 = chrono::steady_clock::now();

        // grow initial population
        growInitialPopulation<Mode, TreeOrGenome>(population, config, strategy);

        // init best indivdual
        auto fittestIndividual = population[0]->clone();
        double fittestErr = 100000;

        if (i == 0) { // Canonical
                      //
                      //
          cout << "---------------------- canonical -------------------------"
               << endl;

          // evolve through each cycle
          for (int gen = 0; gen < TOTAL_GENERATIONS; gen++) {

            cout << genCounter++ << " ";

            const auto genRes = generation<TreeOrGenome, Mode>(
                population, trainingInputs, trainingTargets, trainingErrors,
                validationInputs, validationTargets, validationErrors, config,
                fittestIndividual, fittestErr);

            if (genRes.mustStop) {
              cout << "exiting..." << endl;
              break;
            }

            hitsPerGeneration.push_back(genRes.hits);
          }

          auto t2 = chrono::steady_clock::now();

          auto valRes =
              validatePopulation(validationInputs, validationTargets,
                                 population, 0, population.size() - 1);

          auto testRes = validatePopulation(testInputs, testTargets, population,
                                            0, population.size() - 1);

          int bestNodeCount =
              population[valRes.bestIndividualIndex]->getNodeCount();
          string bestIndivString =
              population[valRes.bestIndividualIndex]->toString();

          exportToCSV(to_string(seed) + to_string(POP_SIZE) + "_canonical" +
                          to_string(fileName++),
                      valRes, testRes, config, strategy,
                      chrono::duration<double>(t2 - t1).count(), bestNodeCount,
                      bestIndivString, hitsPerGeneration, POP_SIZE);
        } else { // SBGP
          cout << "---------------------- SBGP -------------------------"
               << endl;

          // evolve through each cycle
          for (int cycle = 0; cycle < config.cycles; cycle++) {
            // call generation to continue after initial grow
            cout << "================ cycle " << cycle << " of "
                 << config.cycles << " ===============" << endl;

            cout << ", local: ";

            for (int locGen = 0;
                 locGen < config.percycle.localSearchGenerations; locGen++) {

              cout << genCounter++ << " ";

              const auto genRes = generation<TreeOrGenome, Mode>(
                  population, trainingInputs, trainingTargets, trainingErrors,
                  validationInputs, validationTargets, validationErrors, config,
                  fittestIndividual, fittestErr);

              if (genRes.mustStop) {
                cout << "Exiting..." << endl;
                break;
              }

              hitsPerGeneration.push_back(genRes.hits);
            }

            // no point in peturbing on the last generation
            // they are about to go into the test case
            if (cycle < config.cycles - 1) {

              // get the max error to peturb calcuated from the end of the
              // array
              auto threshMax = utils::getThreshError<utils::Mode::MAX>(
                  trainingErrors, config.freezeEliteIndividualsPercent);

              // threshold and max error will likely always be large enough
              for (int e = 0; e < trainingErrors.size(); e++) {
                // check if this individuals error is above or equal (prevents
                // stagnation on convergence) to the threshold error, in which
                // case it needs a shake up
                if (trainingErrors[e] >= get<0>(threshMax)) {
                  // need to tell the compiler that the member is a template
                  population[e]
                      ->template freezeToPercent<op::FreezeType::BOTTOM>();
                }
              }
              cout << ", peturb: ";

              for (int petGen = 0; petGen < config.percycle.peturbGenerations;
                   petGen++) {

                cout << genCounter++ << " ";

                const auto genRes = generation<TreeOrGenome, Mode>(
                    population, trainingInputs, trainingTargets, trainingErrors,
                    validationInputs, validationTargets, validationErrors,
                    config, fittestIndividual, fittestErr);

                if (genRes.mustStop) {
                  cout << "Exiting..." << endl;
                  break;
                }

                hitsPerGeneration.push_back(genRes.hits);
              }

              // unfreeze for the start of the new cycle
              // where canonical will begin
              for (const auto &p : population)
                p->template freezeToPercent<op::FreezeType::NONE>();

              cout << endl;
            }
          }

          auto t2 = chrono::steady_clock::now();

          auto valRes =
              validatePopulation(validationInputs, validationTargets,
                                 population, 0, population.size() - 1);

          auto testRes = validatePopulation(testInputs, testTargets, population,
                                            0, population.size() - 1);

          int bestNodeCount =
              population[valRes.bestIndividualIndex]->getNodeCount();
          string bestIndivString =
              population[valRes.bestIndividualIndex]->toString();

          exportToCSV(to_string(seed) + "_" + to_string(POP_SIZE) + "_SB" +
                          to_string(fileName++),
                      valRes, testRes, config, strategy,
                      chrono::duration<double>(t2 - t1).count(), bestNodeCount,
                      bestIndivString, hitsPerGeneration, POP_SIZE);
        }
      }
    }
  }
}

void testGE() {
  cout << "Testing GE: " << endl;

  Tree::highestConstant = 2;
  Tree::smallestConstant = -2;
  Tree::seed = 6;

  Genome::chooseConstantBias = 2;
  Genome::chooseVariableBias = 3;
  Genome::genomeSize = 50;
  const int initialGenomeDepth = 5;

  std::vector<double> vars = {1, 2, 3};
  std::vector<double> vars2 = {-1, -2, -3};
  std::vector<double> vars3 = {100, 200, 300};

  std::vector<Genome> fullGrowGenomes;

  for (int i = 0; i < 20; i++) {
    // full grow
    Genome g(3, false, vars.size());
    fullGrowGenomes.emplace_back(g);
  }

  cout << "---------------- FULL GROW: -----------------" << endl;

  for (auto &g : fullGrowGenomes) {
    // cout << "GENOME" << g.toString() << endl;
    // cout << "EVALUATE (1,2,3): " << g.evaluate(vars) << endl;
    cout << "EVALUATE (1, 2, 3): " << g.toString() << " = " << g.evaluate(vars)
         << endl;
  }

  std::vector<Genome> growGenomes;

  for (int i = 0; i < 20; i++) {
    // full grow
    Genome g(3, true, vars.size());
    growGenomes.emplace_back(g);
  }

  cout << "------------------ GROW: --------------------" << endl;

  for (auto &g : growGenomes) {
    // cout << "GENOME" << g.toString() << endl;
    // cout << "EVALUATE (1,2,3): " << g.evaluate(vars) << endl;
    cout << "EVALUATE (1, 2, 3): " << g.toString() << " = " << g.evaluate(vars)
         << endl;
  }
}

int main() {

  // freeze only if the tree is >= 2 nodes deep
  Tree::freezeCutoffDepth = 2;
  Tree::freezeBottomPercent = 0.5; // freeze half the tree
  Tree::highestConstant = 2;
  Tree::smallestConstant = -2;

  // GE params
  Genome::chooseConstantBias = 7;
  Genome::chooseVariableBias = 7;
  Genome::genomeSize = 50;

  // Init data processor
  DataProcessor dataProcessor;

  // create the test data
  dataProcessor.readCSV("./dataset/training.csv");
  vector<vector<double>> trainingInputs = dataProcessor.getInputs();
  vector<double> trainingTargets = dataProcessor.getTargets();

  dataProcessor.readCSV("./dataset/validation.csv");
  vector<vector<double>> validationInputs = dataProcessor.getInputs();
  vector<double> validationTargets = dataProcessor.getTargets();

  dataProcessor.readCSV("./dataset/test.csv");
  vector<vector<double>> testInputs = dataProcessor.getInputs();
  vector<double> testTargets = dataProcessor.getTargets();

  Config config = {
      .percycle = {.localSearchGenerations = 10, .peturbGenerations = 2},
      .cycles = 5,
      .numThreads = 2,
      .chooseConstantProbability = 0.5,
      .tournamentSize = 3,
      .numVars = static_cast<int>(trainingInputs[0].size()),
      .prematureLeafProbability = 0.25,
      .crossoverRate = 0.70,
      .mutationRate = 0.05,
      .tuneConstantProbability = 0.5,
      .parsimonyPressure = 0.0001,
      .highestStoppingError = 0.005,
      .highestHitError = 0.012,
      .freezeEliteIndividualsPercent = 0.30};

  // use later
  std::vector<GrowStrategy> growStrategies = {
      //{.minDepth = 3, .maxDepth = 7, .fullGrow = 15, .grow = 15}, // 750
      //{.minDepth = 3, .maxDepth = 7, .fullGrow = 60, .grow = 60}, // 600
      //{.minDepth = 3, .maxDepth = 7, .fullGrow = 45, .grow = 45}, // 450
      //{.minDepth = 3, .maxDepth = 7, .fullGrow = 30, .grow = 30}, // 300
      // TODO: change back
      {.minDepth = 3, .maxDepth = 4, .fullGrow = 3, .grow = 3}, // test
  };

  /* signature:
template <OperationMode Mode, class TreeOrGenome>
void runTestCase(Config &config, vector<GrowStrategy> growStrategies,
                 const vector<vector<double>> &trainingInputs,
                 const vector<double> &trainingTargets,
                 const vector<vector<double>> &validationInputs,
                 const vector<double> &validationTargets,
                 const vector<vector<double>> &testInputs,
                 const vector<double> &testTargets) {
                 */

  cout << "Starting GE Evolution..." << endl;
  cout << "input size" << trainingInputs[0].size() << endl;

  runTestCase<OperationMode::GE, Genome>(
      config, growStrategies, trainingInputs, trainingTargets, validationInputs,
      validationTargets, testInputs, testTargets);

  return 0;
}
