#include <iostream>
#include <memory>
#include <vector>

#include "Node.h"
using namespace std;

int main() {
  const int A_ind = 0;
  const vector<double> var_values = {2};

  const double CONST_NODE_ONE = 6;
  const double CONST_NODE_TWO = 4;
  const double CONST_NODE_THREE = 4;

  unique_ptr<OperatorNode> root = make_unique<OperatorNode>(OpType::DIV);
  unique_ptr<OperatorNode> left = make_unique<OperatorNode>(OpType::MUL);
  unique_ptr<Node> right = make_unique<VariableNode>(A_ind);

  unique_ptr<OperatorNode> leftleft = make_unique<OperatorNode>(OpType::SIN);
  unique_ptr<Node> leftleftleft = make_unique<ConstantNode>(CONST_NODE_THREE);

  unique_ptr<Node> leftright = make_unique<ConstantNode>(CONST_NODE_TWO);

  leftleft->addChild(std::move(leftleftleft));
  left->addChild(std::move(leftleft));
  left->addChild(std::move(leftright));

  root->addChild(std::move(left));
  root->addChild(std::move(right));

  cout << root->toString(var_values) << endl;

  cout << "Value: " << root->evaluate(var_values) << endl;

  return 0;
}
