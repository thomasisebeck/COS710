#pragma once

#include <string>
#include <vector>
class Genome {

private:
  std::vector<int> genome;
  void generateRandomGenome();

  double evaluateRec(const std::vector<double> &vars, int &currInd,
                     int &currCalls);
  int getNextGenome(int &currInd);
  int nodeCount;

public:
  static int genomeSize;
  static int chooseVariableBias;
  static int chooseConstantBias;
  Genome();
  double evaluate(const std::vector<double> &vars);
  std::string toString() const;
};
