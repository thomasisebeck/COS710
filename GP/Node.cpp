#include "Node.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
using namespace std;

// ------------------------ NODE ----------------------- //

Node::Node(NodeType type) : type(type) {}

NodeType Node::getType() const { return this->type; }

// do nohing
void Node::getChildren(std::vector<std::unique_ptr<Node>*>& res) {}

size_t Node::getNumberOfChildren() { return 0; }

// -------------------- CONSTANT NODE ------------------ //

ConstantNode::ConstantNode(double value) : Node(CONSTANT), value(value) {}

double ConstantNode::evaluate(const vector<double>& vars) {
  // constant node does not care about variables
  return this->value;
}

string ConstantNode::toString(const vector<double>& vars) {
  return std::to_string(this->value);
}

std::unique_ptr<Node> ConstantNode::clone() const {
  return make_unique<ConstantNode>(this->value);
}

// -------------------- VARIABLE NODE ------------------ //

VariableNode::VariableNode(int index) : index(index), Node(VARIABLE) {}

double VariableNode::evaluate(const vector<double>& vars) {
  assert((this->index >= 0 && this->index < vars.size()) &&
	 "Variable node index is out of bounds");
  // look up my value in the hashmap
  return vars[this->index];
}

string VariableNode::toString(const vector<double>& vars) {
  assert((this->index >= 0 && this->index < vars.size()) &&
	 "Variable node index out of bound");

  return "x(" + std::to_string(vars[this->index]) + ")";
}

std::unique_ptr<Node> VariableNode::clone() const {
  return make_unique<VariableNode>(this->index);
}

// -------------------- OPERATOR NODE ------------------ //

OperatorNode::OperatorNode(OpType type) : Node(OPERATOR), type(type) {
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
      /*
    case OpType::SQRT:
      return "SQRT";
    case OpType::SIN:
      return "SIN";
      */
    case OpType::POW:
      return "POW";
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

    case OpType::POW:
      return std::pow(this->children[0]->evaluate(vars),
		      this->children[1]->evaluate(vars));

    default:
      assert(false && "OperatorNode::evaluate - Unhandled node type");
  }
}
