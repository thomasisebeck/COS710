#include "GrowTree.h"

#include <cassert>

using namespace std;

GrowTree::GrowTree(int depth, int numVars, double chooseConstantProbability,
		   double prematureLeafProbability)
    : prematureLeafProbability(prematureLeafProbability),
      Tree(depth, numVars, chooseConstantProbability) {
  assert((prematureLeafProbability >= 0.0 && prematureLeafProbability < 1.0) &&
	 "Invalid value for premature leaf probability");
}

/*
 * This function will sprout either 1 or 2 nodes on each level
 * until the maximum depth has been reached
 */
void GrowTree::grow() {
  this->root =
      this->growRec(this->getMaxDepth(), false, prematureLeafProbability);
}
