#include "DataProccessor.h"
#include "csv-parser/include/internal/csv_reader.hpp"
using namespace std;

void DataProcessor::readCSV(string inputFile, int skipLines) {
  csv::CSVReader reader(inputFile);

  int skipCount = 0;

  for (auto& row : reader) {
    // skip the number of lines needed
    if (skipCount < skipLines) {
      skipCount++;
      continue;
    }

    rows.push_back({
	.timeIndex = row["time_index"].get<int>(),
	.currentLoad = row["min_max_scaled_load"].get<double>(),
	.load_1 = row["load_n1"].get<double>(),
	.load_2 = row["load_n2"].get<double>(),
	.load_3 = row["load_n3"].get<double>(),
	.load_4 = row["load_n4"].get<double>(),
    });
  }
}

vector<Row> DataProcessor::getRows() { return this->rows; }
