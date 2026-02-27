#include "Tree.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "Node.h"
using namespace std;

// init the random device
std::mt19937 Tree::engine{std::random_device{}()};

// TODO: test out which ones work with these
int Tree::smallestConstant = -10;  // or whatever default you want
int Tree::highestConstant = 10;

using namespace std;

double Tree::getRandomConstant() {
  static std::uniform_real_distribution<double> dist(Tree::smallestConstant,
						     Tree::highestConstant);

  return dist(engine);
}

int Tree::getRandomInt(int min, int max) {
  std::uniform_int_distribution<int> dist(min, max);

  return dist(engine);
}

int Tree::getDepth() const { return this->depth; }

OpType Tree::getRandomOperator() {
  // INFO: must not include the size type
  return static_cast<OpType>(Tree::getRandomInt(0, OP_TYPE_SIZE - 1));
}

string Tree::toString(std::vector<double>& vars) {
  assert(this->root != nullptr);

  return this->root->toString(vars);
}

int Tree::getNumVars() const { return this->numVars; }

double Tree::getChooseConstantProbability() const {
  return this->chooseConstantProbability;
}

Tree::Tree(int depth, int numVars, double chooseConstantProbability)
    : depth(depth),
      numVars(numVars),
      chooseConstantProbability(chooseConstantProbability) {
  // INFO: generate a random operator for the root
  this->root = std::make_unique<OperatorNode>(getRandomOperator());
}
