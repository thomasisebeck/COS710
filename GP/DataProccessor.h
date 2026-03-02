#pragma once
#include <string>
#include <vector>

struct Row {
  int timeIndex;
  double currentLoad;
  double load_1;
  double load_2;
  double load_3;
  double load_4;
};

class DataProcessor {
 private:
  std::vector<Row> rows;
  std::string inputFile;

 public:
  void readCSV(std::string inputFile, int skipLines);
  std::vector<Row> getRows();
};
