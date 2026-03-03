
#pragma once

#include "Tree.h"

class GrowTree : public Tree {
 private:
  double prematureLeafProbability;

 public:
  // for allocating mem
  GrowTree() = default;
  GrowTree(int depth, int numVars, double chooseConstantProbability,
	   double prematureLeafProbability);

  void grow() override;
};
