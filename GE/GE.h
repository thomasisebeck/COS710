#pragma once

#include "Operations.h"
#include <memory>
#include <string>
#include <vector>

class Genome {

private:
  std::vector<int> genome;

  void generateRandomGenome(bool grow, int varSize);
  void generateRandomGenomeRec(int currDepth, int varSize, bool grow);

  double evaluateRec(const std::vector<double> &vars, int &currInd,
                     int &currCalls);
  int getNextGenome(int &currInd);
  int nodeCount;
  int maxInitialDepth;

public:
  static int genomeSize;
  static int chooseVariableBias;
  static int chooseConstantBias;
  static double prematureLeafProbalility;
  Genome(int depth, bool grow, int varSize);
  double evaluate(const std::vector<double> &vars);
  std::string toString() const;
  int getNodeCount();
  [[nodiscard]] std::unique_ptr<Genome> clone();
  void mutate();

  void crossover(Genome &other);

  template <op::FreezeType> void freezeToPercent(double scorediff = 0);
};
