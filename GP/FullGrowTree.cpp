#include "FullGrowTree.h"

using namespace std;

FullGrowTree::FullGrowTree(int depth, int numVars,
			   double chooseConstantProbability)
    : Tree(depth, numVars, chooseConstantProbability) {}

/*
 * This function will sprout max nodes on each level
 * until the maximum depth has been reached
 */
void FullGrowTree::grow() {
  this->root = this->growRec(this->getMaxDepth(), true, 0);
}
