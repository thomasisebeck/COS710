#include "DataProccessor.h"
#include "csv-parser/include/internal/csv_reader.hpp"
using namespace std;

void DataProcessor::readCSV(string inputFile) {
  csv::CSVReader reader(inputFile);

  for (auto& row : reader)
    rows.push_back(
	{.load = row["load"].get<double>(),
	 .loadN1 = row["load_n1"].get<double>(),
	 .loadN2 = row["load_n2"].get<double>(),
	 .dayOfYearCos = row["normalised_day_of_year_cos"].get<double>(),
	 .dayOfYearSin = row["normalised_day_of_year_sin"].get<double>(),
	 .minuteCos = row["normalised_minute_cos"].get<double>(),
	 .minuteSin = row["normalised_minute_sin"].get<double>()});
}

vector<Row> DataProcessor::getRows() { return this->rows; }
