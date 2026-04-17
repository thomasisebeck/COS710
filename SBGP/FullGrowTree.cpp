#include "FullGrowTree.h"

#include <cassert>

using namespace std;

FullGrowTree::FullGrowTree(int depth, int numVars,
			   double chooseConstantProbability,
			   double tuneConstantProbability)
    : Tree(depth, numVars, chooseConstantProbability, tuneConstantProbability) {
}

/*
 * This function will sprout max nodes on each level
 * until the maximum depth has been reached
 */
void FullGrowTree::grow() {
  this->root = this->growRec(this->getMaxDepth(), true, 0);
}

std::unique_ptr<Tree> FullGrowTree::clone() const {
  auto newTree = make_unique<FullGrowTree>(
      this->getMaxDepth(), this->getNumVars(),
      this->getChooseConstantProbability(), this->getTuneConstantProbability());

  assert(this->root != nullptr && "This root is null with fullgrowtree clone");

  newTree->root = root->clone();

  // automatically moves
  return newTree;
}
