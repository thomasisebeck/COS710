#include "Tree.h"

#include <algorithm>
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
int Tree::smallestConstant = -10;
int Tree::highestConstant = 10;

using namespace std;

double Tree::getRandomDouble(int min, int max) {
  std::uniform_real_distribution<double> dist(min, max);

  return dist(engine);
}

int Tree::getRandomInt(int min, int max) {
  std::uniform_int_distribution<int> dist(min, max);

  return dist(engine);
}

int Tree::getMaxDepth() const { return this->maxDepth; }

OpType Tree::getRandomOperator() {
  // INFO: must not include the size type
  return static_cast<OpType>(Tree::getRandomInt(0, OP_TYPE_SIZE - 1));
}

// swap the nodes
string Tree::toString(const std::vector<double>& vars) {
  assert(this->root != nullptr);

  return this->root->toString(vars);
}

void Tree::collectNodesRec(std::unique_ptr<Node>& curr,
			   std::vector<std::unique_ptr<Node>*>& res) {
  // push back the current node
  res.push_back(&curr);

  std::vector<std::unique_ptr<Node>*> tempChildren;
  tempChildren.reserve(curr->getNumberOfChildren());

  // for each of the children, call this function
  curr->getChildren(tempChildren);

  for (auto* childPointer : tempChildren) {
    collectNodesRec(*childPointer, res);
  }
}

double Tree::evaluate(const std::vector<double>& vars) {
  return this->root->evaluate(vars);
}

unique_ptr<Node> Tree::getTerminal() {
  // create a variable or constant node
  // flip a coin to decide whether to choose a constant

  double coin = this->getRandomDouble(0, 1);

  if (coin < this->chooseConstantProbability) {
    // get a random variable node
    return make_unique<ConstantNode>(
	Tree::getRandomDouble(Tree::smallestConstant, Tree::highestConstant));
  }

  // not making a constant, choosing a variable rather.....
  return make_unique<VariableNode>(
      Tree::getRandomInt(0, this->getNumVars() - 1));
}

unique_ptr<Node> Tree::growRec(int remainingDepth, bool fullGrow,
			       double prematureLeafProbability) {
  assert(this->getNumVars() > 0);

  // base case: must generate a leaf node
  if (remainingDepth == 0) {
    return getTerminal();
  }

  // can make a non-terminal
  if (!fullGrow) {
    // flip a coin to see whether to make a terminal
    const auto makeTerminal = this->getRandomDouble(0, 1);

    if (makeTerminal <= prematureLeafProbability) {
      return getTerminal();
    }
  }

  // create the node of a random operator
  auto operatorNode = make_unique<OperatorNode>(Tree::getRandomOperator());

  if (operatorNode->getIsUnary()) {
    // add one child that is an operator
    operatorNode->addChild(
	this->growRec(remainingDepth - 1, fullGrow, prematureLeafProbability));
  } else {
    // add two children that are operators
    operatorNode->addChild(
	this->growRec(remainingDepth - 1, fullGrow, prematureLeafProbability));
    operatorNode->addChild(
	this->growRec(remainingDepth - 1, fullGrow, prematureLeafProbability));
  }

  // will automatically be moved
  return operatorNode;
}

std::vector<std::unique_ptr<Node>*> Tree::collectNodes() {
  std::vector<std::unique_ptr<Node>*> toRet;

  collectNodesRec(this->root, toRet);

  return toRet;
}

void Tree::crossover(Tree& other) {
  // prevent same tree crossover
  if (this == &other) {
    return;
  }

  auto myNodes = this->collectNodes();
  auto otherNodes = other.collectNodes();

  // auto* double checks that it's a pointer
  auto* myReference = myNodes[this->getRandomInt(0, (int)myNodes.size() - 1)];
  auto* theirReference =
      otherNodes[this->getRandomInt(0, (int)otherNodes.size() - 1)];

  // get the value inside the reference
  // that is the unique pointer
  // std::unique_ptr<Node> test = std::move(*myReference);
  swap(*myReference, *theirReference);
}

int Tree::getNumVars() const { return this->numVars; }

double Tree::getChooseConstantProbability() const {
  return this->chooseConstantProbability;
}

int findDepthOfNodeRec(Node* root, Node* toFind) {
  // base case -> unwind if found
  if (root == toFind) return 0;

  // not found
  if (root == nullptr) return -1;

  // populate children
  std::vector<std::unique_ptr<Node>*> children;
  root->getChildren(children);

  for (auto& c : children) {
    int depth = findDepthOfNodeRec(c->get(), toFind);

    if (depth >= 0) return depth + 1;
  }

  // not found
  return -1;
}

int Tree::getDepthOfNode(Node* toFind) {
  int res = findDepthOfNodeRec(this->root.get(), toFind);
  assert(res != -1 && "Node not found");
  return res;
}

void Tree::mutate() {
  // INFO: get random node

  std::vector<std::unique_ptr<Node>*> myNodes = this->collectNodes();

  // get unique pointer pointer to a random node
  std::unique_ptr<Node>* randomNode =
      myNodes[this->getRandomInt(0, myNodes.size() - 1)];

  // INFO: get remaining height that can be grown
  // Pass in a Node* (call get on unique_ptr)
  const int remainingHeight =
      this->getMaxDepth() - getDepthOfNode(randomNode->get());

  // cout << "Node to mutate " << randomNode->get()->toString({0.5, 0.2}) <<
  // endl;

  // INFO: replace the node with new subtree of depth (maxDepth -
  // nodeDepth) remaining height to grow = maxHeight - currDepth growRec
  // returns unique_ptr<Node> move new tree into the unique pointer
  *randomNode = std::move(this->growRec(remainingHeight, true, 0));
}

int Tree::getMaxDepth() { return this->maxDepth; }

Tree::Tree(int depth, int numVars, double chooseConstantProbability)
    : maxDepth(depth),
      numVars(numVars),
      chooseConstantProbability(chooseConstantProbability) {
  assert((chooseConstantProbability > 0 && chooseConstantProbability < 1) &&
	 "Choose constant probability should be between 0 and 1");
  // INFO: generate a random operator for the root
  this->root = std::make_unique<OperatorNode>(getRandomOperator());
}
