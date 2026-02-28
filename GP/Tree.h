#pragma once
#include <memory>
#include <random>

#include "Node.h"

class Tree {
 private:
  int depth;
  double chooseConstantProbability;
  int numVars;
  static std::mt19937 engine;

 protected:
  // cannot instantiate
  Tree() = default;

  std::unique_ptr<Node> root;
  static OpType getRandomOperator();
  [[nodiscard]] int getDepth() const;
  [[nodiscard]] int getNumVars() const;
  [[nodiscard]] double getChooseConstantProbability() const;
  static double getRandomConstant();
  static int getRandomInt(int min, int max);

 public:
  // WARN: must init these!!!
  static int smallestConstant;
  static int highestConstant;

  Tree(int depth, int numVars, double chooseConstantProbability);
  std::string toString(const std::vector<double>& vars);

  virtual void grow() = 0;

  // must be on template
  virtual double evaluate(const std::vector<double>& vars) = 0;
};
