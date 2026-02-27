#include "FullGrowTree.h"

#include <cassert>
#include <iostream>
#include <memory>

using namespace std;

FullGrowTree::FullGrowTree(int depth, int numVars,
			   double chooseConstantProbability)
    : Tree(depth, numVars, chooseConstantProbability) {}

unique_ptr<Node> FullGrowTree::fullGrowRec(int maxDepth) {
  assert(this->getNumVars() > 0);

  // base case: must generate a leaf node
  if (maxDepth == 0) {
    // create a variable or constant node
    // flip a coin to decide whether to choose a constant
    const bool makeConstant =
	Tree::getRandomInt(0, 1) < this->getChooseConstantProbability();

    if (makeConstant) {
      // get a random variable node
      return make_unique<ConstantNode>(Tree::getRandomConstant());
    }

    // not making a constant, choosing a variable rather.....
    return make_unique<VariableNode>(
	Tree::getRandomInt(0, this->getNumVars() - 1));
  }

  // create the node of a random operator
  auto operatorNode = make_unique<OperatorNode>(Tree::getRandomOperator());

  if (operatorNode->getIsUnary()) {
    // add one child that is an operator
    operatorNode->addChild(this->fullGrowRec(maxDepth - 1));
  } else {
    // add two children that are operators
    operatorNode->addChild(this->fullGrowRec(maxDepth - 1));
    operatorNode->addChild(this->fullGrowRec(maxDepth - 1));
  }

  // will automatically be moved
  return operatorNode;
}

/*
 * This function will sprout either 1 or 2 nodes on each level
 * until the maximum depth has been reached
 */
void FullGrowTree::grow() { this->root = fullGrowRec(this->getDepth()); }
