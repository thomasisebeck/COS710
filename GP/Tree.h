#pragma once
#include <memory>
#include <random>
#include <vector>

#include "Node.h"

class Tree {
 private:
  int maxDepth;
  double chooseConstantProbability;
  int numVars;
  static std::mt19937 engine;

  void collectNodesRec(std::unique_ptr<Node>& curr,
		       std::vector<std::unique_ptr<Node>*>& res);
  std::vector<std::unique_ptr<Node>*> collectNodes();

  std::unique_ptr<Node> getTerminal();
  int getDepthOfNode(Node* toFind);

 protected:
  // cannot instantiate
  Tree() = default;
  std::unique_ptr<Node> root;
  static OpType getRandomOperator();
  [[nodiscard]] int getMaxDepth() const;
  [[nodiscard]] int getNumVars() const;
  [[nodiscard]] double getChooseConstantProbability() const;
  static double getRandomDouble(int min, int max);
  std::unique_ptr<Node> growRec(int remainingDepth, bool fullGrow,
				double prematureLeafProbability);

 public:
  // WARN: must init these!!!
  static int smallestConstant;
  static int highestConstant;

  Tree(int depth, int numVars, double chooseConstantProbability);
  static int getRandomInt(int min, int max);
  std::string toString(const std::vector<double>& vars);
  int getMaxDepth();
  void crossover(Tree& other);
  void mutate();

  virtual void grow() = 0;

  // must be on template
  double evaluate(const std::vector<double>& vars);
};
