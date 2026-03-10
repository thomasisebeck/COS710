#include <iomanip>
#include <iostream>

#include "DataProccessor.h"
#include "csv-parser/include/internal/csv_reader.hpp"
using namespace std;

void DataProcessor::readCSV(string inputFile) {
  csv::CSVReader reader(inputFile);

  for (auto& row : reader) {
    targets.push_back(row["load"].get<double>());

    inputs.push_back({row["load_n1"].get<double>(),
		      row["load_n2"].get<double>(),
		      row["normalised_day_of_year_cos"].get<double>(),
		      row["normalised_day_of_year_sin"].get<double>(),
		      row["normalised_minute_cos"].get<double>(),
		      row["normalised_minute_sin"].get<double>()});
  }
}

std::vector<double> DataProcessor::getTargets() { return this->targets; }

std::vector<std::vector<double>> DataProcessor::getInputs() {
  return this->inputs;
}
