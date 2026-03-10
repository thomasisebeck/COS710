#pragma once

#include "Tree.h"

class FullGrowTree : public Tree {
 public:
  // for allocating mem
  FullGrowTree() = default;
  FullGrowTree(int depth, int numVars, double chooseConstantProbability,
	       double tuneConstantProbability);

  [[nodiscard]] std::unique_ptr<Tree> clone() const override;
  void grow() override;
};
