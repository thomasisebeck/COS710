#pragma once

#include "Tree.h"

class FullGrowTree : public Tree {
private:
  void grow() override;

public:
  // for allocating mem
  FullGrowTree() = default;
  FullGrowTree(int depth, int numVars, double chooseConstantProbability,
               double tuneConstantProbability);

  [[nodiscard]] std::unique_ptr<Tree> clone() const override;
};
