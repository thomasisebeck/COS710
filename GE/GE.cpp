#include "GE.h"
#include "Operations.h"
#include "Tree.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
using namespace std;

int Genome::genomeSize = 10;
int Genome::chooseVariableBias = 0;
int Genome::chooseConstantBias = 0;
double Genome::prematureLeafProbalility = 0.5;

enum class RuleSet { ADD = 0, SUB, MUL, DIV, SQUARE, SIZE };

int MAX_INT_CODON = 65535;

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

std::tuple<int, int, int> getRulesVarsConst(int varSize) {
  const int numRules = static_cast<int>(RuleSet::SIZE);
  const int numVars = varSize + Genome::chooseVariableBias;
  const int numConstants = Genome::chooseConstantBias;

  return std::make_tuple(numRules, numVars, numConstants);
}

void Genome::mutate() {
  cout << "mutation not implemented for genes!!!" << endl;
}

void Genome::crossover(Genome &other) {
  cout << "crossover not implemented for genes!!!" << endl;
}

template <op::FreezeType Type> void Genome::freezeToPercent(double scorediff) {
  /*
   * for a tree
      this->frozenThresholdLayer =
          round((calculateCurrDepth() - 1) * scorediff);
   */
  if constexpr (Type == op::FreezeType::NONE) {
    // unfreeze everything
    this->frozenIndex = -1;
    this->freezeType = op::FreezeType::NONE;
    return;
  } else if constexpr (Type == op::FreezeType::TOP) {
    // freeze up to one third of the genome
    // this autocasts to an int
    this->frozenIndex = (static_cast<double>(genomeSize) / 3) * scorediff;

    this->freezeType = op::FreezeType::TOP;
  } else { // bottom freeze

    this->freezeType = op::FreezeType::BOTTOM;

    // freeze the bottom third of the genome
    this->frozenIndex =
        static_cast<double>(genomeSize) - (static_cast<double>(genomeSize) / 3);
  }

  cout << "Freezing not implemented for genes!!" << endl;
}

unique_ptr<Genome> Genome::clone() { return std::make_unique<Genome>(*this); }

void Genome::generateRandomGenomeRec(int currDepth, int varSize, bool grow) {

  bool chooseLeafNow =
      Tree::getRandomDouble(0, 1) >= Genome::prematureLeafProbalility;

  // reached the max depth, or have a premature leaf
  bool mustChooseTerminal =
      currDepth >= maxInitialDepth || (grow && chooseLeafNow);

  const auto [numRules, numVars, numConstants] = getRulesVarsConst(varSize);

  const auto fullSize =
      static_cast<int>(RuleSet::SIZE) + numVars + numConstants;

  // modify this random val to map to a constant
  // since we are adding to this random value, it must be
  // reduced at least by one fullsize
  auto randomValMisaligned = Tree::getRandomInt(0, MAX_INT_CODON - fullSize);

  // this is how much the random val is misaligned
  auto remainder = randomValMisaligned % fullSize;

  // align with the start of the ruleset
  // currently pointing at first production rule for operator
  auto randomValueAligned = randomValMisaligned - remainder;

  // must have a leaf here....
  if (mustChooseTerminal) {

    if (Tree::getRandomInt(0, fullSize) >= Genome::chooseConstantBias) {

      // start one off the end of the ruleset array (at the constant thresh)
      // add a random int up till the end constant index (numConstants - 1)
      // to put it still in constant range
      // eg: if you have 2 constants, it will add 0 or 1
      auto constantOffset = static_cast<int>(RuleSet::SIZE) +
                            Tree::getRandomInt(0, numConstants - 1);

      // produce a constant node
      // add the aligned random value (always points at first operator
      // production rule) to the size of production rules + a random int to
      // get a constant
      this->genome.push_back(randomValueAligned + constantOffset);

      // write the actual constant bits to the tape
      // use pushback to prevent any issues
      this->genome.push_back(Tree::getRandomInt(0, MAX_INT_CODON));
      this->genome.push_back(Tree::getRandomInt(0, MAX_INT_CODON));
    } else {
      // reverse engineer a variable node

      // start one off the end of the ruleset array, and the constant array
      // add a random int up till the end constant index (numConstants - 1)
      // to put it still in variable range
      // eg: if you have 3 vars, it will add 0, 1, or 2
      auto variableOffset = static_cast<int>(RuleSet::SIZE) + numConstants +
                            Tree::getRandomInt(0, numVars - 1);

      // produce a variable node
      this->genome.push_back(randomValueAligned + variableOffset);
    }

    return;
  }

  // chooosing a function node.....
  // X rules, X - 1 indices
  auto ruleOffset = Tree::getRandomInt(0, static_cast<int>(RuleSet::SIZE) - 1);

  this->genome.push_back(randomValueAligned + ruleOffset);

  // get the rule and produce the children

  switch (static_cast<RuleSet>(ruleOffset)) {
    // binary expressions
  case RuleSet::ADD:
  case RuleSet::SUB:
  case RuleSet::MUL:
  case RuleSet::DIV:
    // produced 2 children
    generateRandomGenomeRec(currDepth + 1, varSize, grow);
    generateRandomGenomeRec(currDepth + 1, varSize, grow);

    break;
  case RuleSet::SQUARE:
    // produce 1 childe
    generateRandomGenomeRec(currDepth + 1, varSize, grow);
    break;
  default:
    assert(false && "Unknown rule found during init");
  }
}

void Genome::generateRandomGenome(bool grow, int varSize) {
  this->generateRandomGenomeRec(0, varSize, grow);

  // pad the rest to make sure that it is fixed size
  while (this->genome.size() < this->genomeSize) {
    this->genome.push_back(Tree::getRandomInt(0, MAX_INT_CODON));
  }

  // clip the end of the array down
  if (this->genome.size() > this->genomeSize) {
    this->genome.resize(this->genomeSize);
  }

  assert(this->genome.size() == this->genomeSize &&
         "Genome size after init is not correct");
}

Genome::Genome(int depth, bool grow, int varSize) {
  this->maxInitialDepth = depth;
  this->nodeCount = -1;
  this->frozenIndex = -1;
  // unfreeze
  this->freezeType = op::FreezeType::NONE;
  generateRandomGenome(grow, varSize);
}

// choosing a var has a greate probability depending on ChooseVariableBias
int boundAndBiasRule(int input, int varSize) {
  assert(input >= 0 && "bounding: input cannot be negative");

  const auto [numRules, numVars, numConstants] = getRulesVarsConst(varSize);
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
  // therefore if it is just greater, it is out of bounds and in the vars
  // space
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
    // reference when each function call starts, the memory will be
    // overwritten
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

int Genome::getNodeCount() {
  assert(this->nodeCount != -1 && "Node count not initialised");

  return this->nodeCount;
}

double Genome::evaluate(const std::vector<double> &vars) {
  int currInd = 0;
  int currCalls = 0;

  // recursively evaluate this genome
  // this will increment the number of calls that the tree
  // took to get the node count
  const auto res = evaluateRec(vars, currInd, currCalls);

  // set the "node count" to the number of calls that it takes to evaluate the
  // gene this is essentially how many children the genome has, and dictates
  // the evaluation complexity
  this->nodeCount = currCalls;

  return res;
}

string Genome::toString() const {
  string res = "[ ";
  for (const auto &c : this->genome)
    res += to_string(c) + " ";

  return res + "]";
}

// Explicitly tell the compiler to generate the code for these exact types
template void Genome::freezeToPercent<op::FreezeType::BOTTOM>(double);
template void Genome::freezeToPercent<op::FreezeType::NONE>(double);
template void Genome::freezeToPercent<op::FreezeType::TOP>(double);
