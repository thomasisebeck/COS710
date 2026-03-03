#pragma once

#include "Tree.h"

class FullGrowTree : public Tree {
 public:
  // for allocating mem
  FullGrowTree() = default;
  FullGrowTree(int depth, int numVars, double chooseConstantProbability);

  void grow() override;
};
