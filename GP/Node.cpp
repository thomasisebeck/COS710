#include "Node.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
using namespace std;

// ------------------------ NODE ----------------------- //

Node::Node(NodeType type) : type(type) {}

NodeType Node::getType() const { return this->type; }

// -------------------- CONSTANT NODE ------------------ //

ConstantNode::ConstantNode(double value) : Node(CONSTANT), value(value) {}

double ConstantNode::evaluate(const vector<double>& vars) {
  // constant node does not care about variables
  return this->value;
}

string ConstantNode::toString(const vector<double>& vars) {
  return std::to_string(this->value);
}

// -------------------- VARIABLE NODE ------------------ //

VariableNode::VariableNode(int index) : index(index), Node(VARIABLE) {}

double VariableNode::evaluate(const vector<double>& vars) {
  assert((this->index < vars.size() && this->index > 0) &&
	 "Variable node index is out of bounds");
  // look up my value in the hashmap
  return vars[this->index];
}

string VariableNode::toString(const vector<double>& vars) {
  return "x(" + std::to_string(vars[this->index]) + ")";
}

// -------------------- OPERATOR NODE ------------------ //

OperatorNode::OperatorNode(OpType type) : Node(OPERATOR), type(type) {
  // to check the assert
  this->isUnary = this->type == OpType::SIN || this->type == OpType::SQRT ||
		  this->type == OpType::SQUARE;
}

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
    case OpType::SQRT:
      return "SQRT";
    case OpType::SIN:
      return "SIN";
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

void OperatorNode::addChild(unique_ptr<Node> newChild) {
  // adding the first child for unary, and first or second childe for binary
  assert(((this->isUnary && children.empty()) ||
	  (!this->isUnary && children.size() < 2)) &&
	 "Cannot add child");

  this->children.push_back(std::move(newChild));
}

double protectedDivide(double numerator, double denominator) {
  if (denominator == 0) {
    return 0;
  }

  return numerator / denominator;
}

double protectedSqrt(double num) {
  if (num < 0) {
    return 0;
  }

  return sqrt(num);
}

double protectedPow(double base, double exp) {
  if (base < 0) {
    return 0;
  }
  return pow(base, exp);
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

    case OpType::POW:
      // INFO: x to the power of n
      return protectedPow(this->children[0]->evaluate(vars),
			  this->children[1]->evaluate(vars));

    case OpType::SIN:
      return sin(this->children[0]->evaluate(vars));

    case OpType::SQRT:
      return protectedSqrt(this->children[0]->evaluate(vars));

    case OpType::SUB:
      return this->children[0]->evaluate(vars) -
	     this->children[1]->evaluate(vars);

    default:
      assert(false && "OperatorNode::evaluate - Unhandled node type");
  }
}
