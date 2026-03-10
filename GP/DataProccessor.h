#pragma once
#include <string>
#include <vector>

class DataProcessor {
 private:
  std::vector<double> targets;
  std::vector<std::vector<double>> inputs;
  std::string inputFile;

 public:
  void readCSV(std::string inputFile);
  std::vector<double> getTargets();
  std::vector<std::vector<double>> getInputs();
};
