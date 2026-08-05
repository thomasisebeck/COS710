
#pragma once

#include "Tree.h"

class GrowTree : public Tree {
private:
  double prematureLeafProbability;
  void grow() override;

public:
  // for allocating mem
  GrowTree() = default;
  GrowTree(int depth, int numVars, double chooseConstantProbability,
           double prematureLeafProbability, double tuneConstantProbability);

  [[nodiscard]] std::unique_ptr<Tree> clone() const override;
};
