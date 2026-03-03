#pragma once
#include <string>
#include <vector>

struct Row {
  double load;
  double loadN1;
  double loadN2;
  double dayOfYearCos;
  double dayOfYearSin;
  double minuteCos;
  double minuteSin;
};

class DataProcessor {
 private:
  std::vector<Row> rows;
  std::string inputFile;

 public:
  void readCSV(std::string inputFile);
  std::vector<Row> getRows();
};
