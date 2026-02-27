#pragma once
#include <memory>

#include "Node.h"
#include "Tree.h"

class FullGrowTree : public Tree {
 private:
  std::unique_ptr<Node> fullGrowRec(int maxDepth);

 public:
  FullGrowTree(int depth, int numVars, double chooseConstantProbability);
  void grow() override;
};
