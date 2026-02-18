#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

enum NodeType { OPERATOR, CONSTANT, VARIABLE };

enum class OpType { ADD, SUB, MUL, DIV, SQRT, SIN, POW, SQUARE };

class Node {
 private:
  NodeType type;

 public:
  Node(NodeType type);
  [[nodiscard]] NodeType getType() const;

  // variable names mapped to values ("a" -> 4.1)
  virtual double evaluate(const std::vector<double>& vars) = 0;
  virtual std::string toString(const std::vector<double>& vars) = 0;
  virtual ~Node() = default;
};

class VariableNode : public Node {
 private:
  int index;

 public:
  VariableNode(int index);
  std::string toString(const std::vector<double>& vars) override;
  double evaluate(const std::vector<double>& vars) override;
};

// operator nodes have a type (oporand)
// and children (who to apply this operand to)
// when generating the tree
class OperatorNode : public Node {
 private:
  std::vector<std::unique_ptr<Node>> children;
  bool isUnary;
  OpType type;

 public:
  OperatorNode(OpType type);
  void addChild(std::unique_ptr<Node> newChild);
  std::string toString(const std::vector<double>& vars) override;
  double evaluate(const std::vector<double>& vars) override;
};

// always a leaf node, no children
// constant value
class ConstantNode : public Node {
 private:
  double value;

 public:
  ConstantNode(double value);
  std::string toString(const std::vector<double>& vars) override;
  double evaluate(const std::vector<double>& vars) override;
};
