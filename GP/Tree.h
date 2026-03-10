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
  double tuneConstantProbability;

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
  [[nodiscard]] double getTuneConstantProbability() const;
  std::unique_ptr<Node> growRec(int remainingDepth, bool fullGrow,
				double prematureLeafProbability);

 public:
  // WARN: must init these!!!
  static int smallestConstant;
  static int highestConstant;
  static double tuneRange;

  // for grow
  static std::mt19937 engine;

  Tree(int depth, int numVars, double chooseConstantProbability,
       double tuneConstantProbability);
  static int getRandomInt(int min, int max);
  static double getRandomDouble(double min, double max);
  std::string toString(const std::vector<double>& vars);
  int getMaxDepth();
  void crossover(Tree& other);
  [[nodiscard]] int getNodeCount();
  void mutate();

  [[nodiscard]] virtual std::unique_ptr<Tree> clone() const = 0;
  virtual void grow() = 0;

  // must be on template
  double evaluate(const std::vector<double>& vars);
  virtual ~Tree() = default;
};
