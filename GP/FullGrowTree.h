#pragma once
#include <memory>

#include "Node.h"
#include "Tree.h"

class FullGrowTree : public Tree {
 private:
  std::unique_ptr<Node> fullGrowRec(int maxDepth);

 public:
  // for allocating mem
  FullGrowTree() = default;
  FullGrowTree(int depth, int numVars, double chooseConstantProbability);

  // must be on the template
  void grow() override;

  // must be on template
  double evaluate(const std::vector<double>& vars) override;
};
