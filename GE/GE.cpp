#include "GE.h"
#include "Operations.h"
#include "Tree.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <print>
#include <string>
using namespace std;

int Genome::genomeSize = 10;
int Genome::chooseVariableBias = 0;
int Genome::chooseConstantBias = 0;

enum class RuleSet { ADD = 0, SUB, MUL, DIV, SQUARE, SIZE };

/*
  std::vector<int> genome;
  static int SIZE;
  */

void Genome::generateRandomGenome() {
  assert(this->genome.empty() && "genome must be empty");

  for (int i = 0; i < Genome::genomeSize; i++) {
    this->genome.push_back(Tree::getRandomInt(0, 255));
  }
}

const int MAX_INT_CODON = 65535;

// codon values are between 0 and 255
double convertCodonsToScaledFloat(int first, int second) {

  // bit shift the first by 8 bits
  // 0 -> 255 = 00000000 -> 11111111
  // 11010011 becomes 1101001100000000
  first = first << 8;

  // combined = 0000000000000000 -> 1111111111111111
  //          = 0 -> 65535
  int combined = first + second;

  // scale between 0 and 1
  double percentage =
      static_cast<double>(combined) / static_cast<double>(MAX_INT_CODON);

  // if -10 to 10, then range is 20
  const int range = Tree::highestConstant - Tree::smallestConstant;

  // scale between smallest and highest allowed constant
  return Tree::smallestConstant + (percentage * range);
}

Genome::Genome() {
  this->nodeCount = -1;
  generateRandomGenome();
}

// choosing a var has a greate probability depending on ChooseVariableBias
int boundAndBiasRule(int input, int varSize) {
  assert(input >= 0 && "bounding: input cannot be negative");

  const int numRules = static_cast<int>(RuleSet::SIZE);
  const int numVars = varSize + Genome::chooseVariableBias;
  const int numConstants = Genome::chooseConstantBias;

  // bias the genome towards choosing variable nodes and constant nodes
  int maxInd = numRules + numVars + numConstants;

  return input % maxInd;
}

int Genome::getNextGenome(int &currInd) {
  // return the current ind and increment for the next function call
  // overwrite the memory to prevent accessing the same genome
  return this->genome[currInd++ % genome.size()];
}

const int MAX_CALLS = 500;

// pass the currInd by value
double Genome::evaluateRec(const std::vector<double> &vars, int &currInd,
                           int &currCalls) {

  // you are always guaranteed to choose variables
  // so no need for assertation
  assert(Genome::chooseConstantBias > 0 && "must be able to choose constants");
  assert(Genome::chooseVariableBias > 0 && "must be able to choose variables");

  // don't want to bias the exact choice of the variable
  // each must have an equal porbability of being chosen
  assert(Genome::chooseVariableBias % vars.size() == 0 &&
         "Choose var bias must divide among vars equally");

  // non-terminating genome, get rid of it
  if (currCalls++ > MAX_CALLS) {

    return std::numeric_limits<double>::quiet_NaN();
  }

  // choose a rule: 0 -> 5
  // choose a variable: 0 -> (size - 1)
  const int currentGenome = this->getNextGenome(currInd);

  // get the bounded decision
  // this includes choosing a varible from the vars array
  const int decision = boundAndBiasRule(currentGenome, vars.size());

  // as you hit out of bounds for the ruleset, choose a constant
  const int chooseConstantThreshold = static_cast<int>(RuleSet::SIZE);

  // as you are out of bounds for the constants, choose a variable
  const int chooseVariableThreshold =
      chooseConstantThreshold + Genome::chooseConstantBias;

  // added the rules and the constant max indices
  // therefore if it is just greater, it is out of bounds and in the vars space
  if (decision >= chooseVariableThreshold) {
    return vars[(decision - static_cast<int>(RuleSet::SIZE)) % vars.size()];
  }

  // not a var, but not an operator either
  // must be a constant
  if (decision >= chooseConstantThreshold) {

    // consume 2 codons
    const auto nextLeft = getNextGenome(currInd);
    const auto nextRight = getNextGenome(currInd);

    // const auto printMe = convertCodonsToScaledFloat(nextLeft, nextRight);
    // cout << "value: " << printMe << endl;

    // convert to float
    return convertCodonsToScaledFloat(nextLeft, nextRight);
  }

  // not a var
  // not a constant
  // must be a production rule
  assert((decision < static_cast<int>(RuleSet::SIZE)) &&
         "Decision out of bounds for ruleset");

  switch (static_cast<RuleSet>(decision)) {
  case RuleSet::ADD: {
    // add the next 2 genomes
    // don't need to worry about incrementing it, because it is passed by
    // reference when each function call starts, the memory will be overwritten
    const auto left = evaluateRec(vars, currInd, currCalls);
    const auto right = evaluateRec(vars, currInd, currCalls);

    return left + right;
  }

  case RuleSet::SUB: {
    const auto left = evaluateRec(vars, currInd, currCalls);
    const auto right = evaluateRec(vars, currInd, currCalls);

    // subtract the next 2 genomes
    return left - right;
  }

  case RuleSet::MUL: {
    const auto left = evaluateRec(vars, currInd, currCalls);
    const auto right = evaluateRec(vars, currInd, currCalls);

    // subtract the next 2 genomes
    return left * right;
  }

  case RuleSet::DIV: {

    const auto left = evaluateRec(vars, currInd, currCalls);
    const auto right = evaluateRec(vars, currInd, currCalls);

    // divide the next 2 genomes
    return op::protectedDivide(left, right);
  }

  case RuleSet::SQUARE:
    // square the next genome
    const auto toSquare = evaluateRec(vars, currInd, currCalls);
    return toSquare * toSquare;
  }

  std::cout << "DECISION FAILED: " << decision << std::endl;
  std::cout << "CHOOSE VAR THRESH : " << chooseVariableThreshold << std::endl;
  std::cout << "CHOOSE CONST THRESH : " << chooseConstantThreshold << std::endl;
  assert(false && "You dun messed up bradda");
}

double Genome::evaluate(const std::vector<double> &vars) {
  int currInd = 0;
  int currCalls = 0;

  // recursively evaluate this genome
  // this will increment the number of calls that the tree
  // took to get the node count
  const auto res = evaluateRec(vars, currInd, currCalls);

  // set the "node count" to the number of calls that it takes to evaluate the
  // gene this is essentially how many children the genome has, and dictates the
  // evaluation complexity
  this->nodeCount = currCalls;

  return res;
}

string Genome::toString() const {
  string res = "[ ";
  for (const auto &c : this->genome)
    res += to_string(c) + " ";

  return res + "]";
}
