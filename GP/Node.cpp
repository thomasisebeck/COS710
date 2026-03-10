#include "Node.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string>
using namespace std;

// ------------------------ NODE ----------------------- //

// do nohing
void Node::getChildren(std::vector<std::unique_ptr<Node>*>& res) {}

size_t Node::getNumberOfChildren() { return 0; }

// fail the tuning unless constant node
bool Node::tryTuneValue(double) { return false; }

// -------------------- CONSTANT NODE ------------------ //

ConstantNode::ConstantNode(double value) : value(value) {}

double ConstantNode::evaluate(const vector<double>&) {
  // constant node does not care about variables
  return this->value;
}

string ConstantNode::toString(const vector<double>&) {
  return std::to_string(this->value);
}

std::unique_ptr<Node> ConstantNode::clone() const {
  return make_unique<ConstantNode>(this->value);
}

bool ConstantNode::tryTuneValue(double delta) {
  this->value += delta;

  return true;
}

// -------------------- VARIABLE NODE ------------------ //

VariableNode::VariableNode(int index) : index(index) {}

double VariableNode::evaluate(const vector<double>& vars) {
  assert((this->index >= 0 && this->index < vars.size()) &&
	 "Variable node index is out of bounds");
  // look up my value in the hashmap
  return vars[this->index];
}

string VariableNode::toString(const vector<double>& vars) {
  assert((this->index >= 0 && this->index < vars.size()) &&
	 "Variable node index out of bound");

  return "x" + std::to_string(this->index) + "(" +
	 std::to_string(vars[this->index]) + ")";
}

std::unique_ptr<Node> VariableNode::clone() const {
  return make_unique<VariableNode>(this->index);
}

// -------------------- OPERATOR NODE ------------------ //

OperatorNode::OperatorNode(OpType type) : type(type) {
  // to check the assert
  this->isUnary = this->type == OpType::SQUARE;
}

std::unique_ptr<Node> OperatorNode::clone() const {
  // return a clone copy of this node
  auto copy = make_unique<OperatorNode>(this->type);

  for (const auto& child : this->children) {
    copy->addChild(child->clone());
  }

  return copy;
}

bool OperatorNode::getIsUnary() const { return this->isUnary; }

string convOpToString(OpType op) {
  switch (op) {
    case OpType::ADD:
      return "ADD";
    case OpType::SUB:
      return "SUB";
    case OpType::MUL:
      return "MUL";
    case OpType::DIV:
      return "DIV";
    case OpType::SQUARE:
      return "SQUARE";
    default:
      assert(false && "convOpToString: Unknown OpType");
      return "UNKNOWN";
  }
}

string OperatorNode::toString(const vector<double>& vars) {
  string childrenOut;

  for (const auto& child : this->children) {
    childrenOut += child->toString(vars) + ",";
  }

  return convOpToString(this->type) + "( " +
	 childrenOut.substr(0, childrenOut.length() - 1) + " )";
}

void OperatorNode::getChildren(std::vector<std::unique_ptr<Node>*>& res) {
  for (auto& child : this->children) {
    res.push_back(&child);
  }
}

size_t OperatorNode::getNumberOfChildren() { return this->children.size(); }

void OperatorNode::addChild(unique_ptr<Node> newChild) {
  // adding the first child for unary, and first or second childe for binary
  if (isUnary) {
    assert(children.empty() && "Unary node already has a child.");
  } else {
    assert(children.size() < 2 && "Binary node already has two children.");
  }

  this->children.push_back(std::move(newChild));
}

// create a smallest denomitator value
const double EPSILON = 1e-6;

double protectedDivide(double numerator, double denominator) {
  if (std::abs(denominator) < EPSILON) {
    return 1;
  }

  return numerator / denominator;
}

double protectedSqrt(double num) { return sqrt(std::abs(num)); }

double protectedPow(double base, double exp) {
  const double MAX_EXP = 10;

  // exponent can only go up to 10
  exp = std::clamp(exp, -MAX_EXP, MAX_EXP);

  return pow(abs(base), exp);
}

double OperatorNode::evaluate(const vector<double>& vars) {
  // INFO: unary must have 1 child
  // binay must have 2 children
  assert((this->isUnary && children.size() == 1) ||
	 (!this->isUnary && children.size() == 2));

  // loop over the children and call evalute based on the operater
  switch (this->type) {
    case OpType::DIV: {
      // protected divide, 2 children
      return protectedDivide(this->children[0]->evaluate(vars),
			     this->children[1]->evaluate(vars));
    }

    case OpType::ADD:
      return this->children[0]->evaluate(vars) +
	     this->children[1]->evaluate(vars);

    case OpType::SQUARE: {
      double val = this->children[0]->evaluate(vars);
      return val * val;
    }

    case OpType::MUL:
      return this->children[0]->evaluate(vars) *
	     this->children[1]->evaluate(vars);

    case OpType::SUB:
      return this->children[0]->evaluate(vars) -
	     this->children[1]->evaluate(vars);

    default:
      assert(false && "OperatorNode::evaluate - Unhandled node type");
  }

  assert(false && "OperatorNode::evaluate - Unhandled node type");
}
