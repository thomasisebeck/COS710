#include "Tree.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "Node.h"
using namespace std;

int Tree::smallestConstant = -10;
int Tree::highestConstant = 10;
double Tree::tuneRange = 0.5;
int Tree::seed = 1010;
double Tree::freezeBottomPercent = 0.5;
int Tree::freezeCutoffDepth = 6;

// init the random device
thread_local std::mt19937 Tree::engine(Tree::seed);

// TODO: test out which ones work with these

using namespace std;

double Tree::getRandomDouble(double min, double max) {
  std::uniform_real_distribution<double> dist(min, max);

  return dist(engine);
}

int Tree::getRandomInt(int min, int max) {
  std::uniform_int_distribution<int> dist(min, max);

  return dist(engine);
}

int Tree::getMaxInitialDepth() const { return this->maxInitialDepth; }

OpType Tree::getRandomOperator() {
  // INFO: must not include the size type
  return static_cast<OpType>(Tree::getRandomInt(0, OP_TYPE_SIZE - 1));
}

// swap the nodes
string Tree::toString() {
  assert(this->root != nullptr);

  return this->root->toString();
}

int calculateCurrDepthRec(Node *curr) {
  // should never happen
  assert(curr != nullptr && "calc depth rec received a null");

  std::vector<std::unique_ptr<Node> *> children;
  curr->getChildren(children);

  // return my height, no children
  if (children.empty())
    return 0;

  int maxHeight = -1;

  for (const auto ch : children) {
    int currHeight = calculateCurrDepthRec(ch->get());
    if (currHeight > maxHeight)
      maxHeight = currHeight;
  }

  return maxHeight + 1;
}

int Tree::calculateCurrDepth() {
  return calculateCurrDepthRec(this->root.get());
};

void Tree::collectNodesRec(std::unique_ptr<Node> &curr,
                           std::vector<std::unique_ptr<Node> *> &res,
                           int currDepth) {

  // bottom freeze: stop at the threshold
  // any layer below this layer cannot be collected
  // cannot freeze the root node : exclusive >
  if (this->freezeType == FreezeType::BOTTOM &&
      currDepth > this->frozenThresholdLayer)
    return;

  // collect if:
  // 1. no freeze
  // 2. top freeze and below the threshold layer
  // 3. bottom freeze and above the threshold layer (previously checked)
  // cannot freeze the deepest layer
  if (this->freezeType == FreezeType::NONE ||
      (this->freezeType == FreezeType::TOP &&
       currDepth >= this->frozenThresholdLayer) ||
      this->freezeType == FreezeType::BOTTOM)
    res.push_back(&curr);

  std::vector<std::unique_ptr<Node> *> tempChildren;
  tempChildren.reserve(curr->getNumberOfChildren());

  // for each of the children, call this function
  curr->getChildren(tempChildren);

  for (auto *childPointer : tempChildren) {
    collectNodesRec(*childPointer, res, currDepth + 1);
  }
}

double Tree::evaluate(const std::vector<double> &vars) {
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
  if (remainingDepth <= 0) {
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

std::vector<std::unique_ptr<Node> *> Tree::collectNodes() {
  std::vector<std::unique_ptr<Node> *> toRet;

  collectNodesRec(this->root, toRet, 0);

  if (toRet.empty())
    toRet.push_back(&this->root);

  return toRet;
}

int Tree::getNodeCount() { return this->collectNodes().size(); }

void Tree::crossover(Tree &other) {
  // prevent same tree crossover
  if (this == &other) {
    return;
  }

  auto myNodes = this->collectNodes();
  auto otherNodes = other.collectNodes();

  // auto* double checks that it's a pointer
  auto *myReference = myNodes[this->getRandomInt(0, (int)myNodes.size() - 1)];
  auto *theirReference =
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

int findDepthOfNodeRec(Node *root, Node *toFind) {
  // base case -> unwind if found
  if (root == toFind)
    return 0;

  // not found
  if (root == nullptr)
    return -1;

  // populate children
  std::vector<std::unique_ptr<Node> *> children;
  root->getChildren(children);

  for (auto &c : children) {
    int depth = findDepthOfNodeRec(c->get(), toFind);

    if (depth >= 0)
      return depth + 1;
  }

  // not found
  return -1;
}

int Tree::getDepthOfNode(Node *toFind) {
  int res = findDepthOfNodeRec(this->root.get(), toFind);
  assert(res != -1 && "Node not found");
  return res;
}

double Tree::getTuneConstantProbability() const {
  return this->tuneConstantProbability;
}

void Tree::mutate() {
  // INFO: get random node

  std::vector<std::unique_ptr<Node> *> myNodes = this->collectNodes();

  assert(myNodes.size() >= 1 && "Nodes are empty");

  std::unique_ptr<Node> *randomNode =
      myNodes[this->getRandomInt(0, myNodes.size() - 1)];

  // tune constant probabalistically
  if (Tree::getRandomDouble(0, 1) < this->tuneConstantProbability &&
      randomNode->get()->tryTuneValue(
          Tree::getRandomDouble(-Tree::tuneRange, Tree::tuneRange))) {
    return;
  }

  // INFO: get remaining height that can be grown
  // Pass in a Node* (call get on unique_ptr)
  int remainingHeight =
      this->getMaxInitialDepth() - getDepthOfNode(randomNode->get());

  // do nothing
  if (remainingHeight <= 0)
    return;

  // INFO: replace the node with new subtree of depth (maxDepth -
  // nodeDepth) remaining height to grow = maxHeight - currDepth growRec
  // returns unique_ptr<Node> move new tree into the unique pointer
  *randomNode = std::move(this->growRec(remainingHeight, true, 0));
}

int Tree::getMaxInitialDepth() { return this->maxInitialDepth; }

Tree::Tree(int depth, int numVars, double chooseConstantProbability,
           double tuneConstantProbability)
    : maxInitialDepth(depth), numVars(numVars),
      chooseConstantProbability(chooseConstantProbability),
      tuneConstantProbability(tuneConstantProbability) {
  assert((chooseConstantProbability > 0 && chooseConstantProbability < 1) &&
         "Choose constant probability should be between 0 and 1");
  // INFO: generate a random operator for the root
  this->root = std::make_unique<OperatorNode>(getRandomOperator());

  this->freezeToPercent<FreezeType::NONE>();
}

template <FreezeType Type> void Tree::freezeToPercent(double scorediff) {

  const auto depth = this->calculateCurrDepth();

  if (depth >= Tree::freezeCutoffDepth) {
    if constexpr (Type == FreezeType::TOP) {

      this->freezeType = FreezeType::TOP;

      // can freeze all layers except the leaf nodes
      this->frozenThresholdLayer =
          round((calculateCurrDepth() - 1) * scorediff);

      /*
      cout << "[T -> " << this->frozenThresholdLayer << " | *] ("
           << this->calculateCurrDepth() << ") " << endl;
      */

      return;

    } else if constexpr (Type == FreezeType::BOTTOM) {

      this->freezeType = FreezeType::BOTTOM;

      this->frozenThresholdLayer =
          round((calculateCurrDepth() - 1) * Tree::freezeBottomPercent);

      /*
      cout << "[* | B -> " << this->frozenThresholdLayer << "] ("
           << this->calculateCurrDepth() << ") " << endl;
        */

      return;
    }
  }

  // not a candidate for freezing, or chose none
  this->freezeType = FreezeType::NONE;
}

// Tell the compiler to actually generate the binary for these specific types
template void Tree::freezeToPercent<FreezeType::TOP>(double);
template void Tree::freezeToPercent<FreezeType::BOTTOM>(double);
template void Tree::freezeToPercent<FreezeType::NONE>(double);
