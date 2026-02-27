#include <atomic>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "FullGrowTree.h"
#include "Node.h"
#include "csv-parser/include/internal/csv_reader.hpp"

using namespace std;
using namespace csv;

void nodeTest() {
  const int A_ind = 0;
  const vector<double> var_values = {2};

  const double CONST_NODE_ONE = 6;
  const double CONST_NODE_TWO = 4;
  const double CONST_NODE_THREE = 4;

  unique_ptr<OperatorNode> root = make_unique<OperatorNode>(OpType::DIV);
  unique_ptr<OperatorNode> left = make_unique<OperatorNode>(OpType::MUL);
  unique_ptr<Node> right = make_unique<VariableNode>(A_ind);

  unique_ptr<OperatorNode> leftleft = make_unique<OperatorNode>(OpType::ADD);
  unique_ptr<Node> leftleftleft = make_unique<ConstantNode>(CONST_NODE_THREE);

  unique_ptr<Node> leftright = make_unique<ConstantNode>(CONST_NODE_TWO);

  leftleft->addChild(std::move(leftleftleft));
  left->addChild(std::move(leftleft));
  left->addChild(std::move(leftright));

  root->addChild(std::move(left));
  root->addChild(std::move(right));

  cout << root->toString(var_values) << endl;

  cout << "Value: " << root->evaluate(var_values) << endl;
}

void parseCSV() {
  CSVReader reader("dataset/Dataset.csv");

  for (auto& row : reader) {
    // Note: Can also use index of column with [] operator
    cout << row["utc_timestamp"].get<string>() << ", "
	 << row["Electricity_load"].get<string>() << ", "
	 << row["Residential_electricity_price"].get<double>() << ", "
	 << row["Residential_solar_generation"].get<double>() << ", "
	 << row["Residential_wind_generation"].get<double>() << ", "
	 << row["Temperature"].get<double>() << ", "
	 << row["Relative Humidity"].get<double>() << endl;
  }
}

void doWork(const vector<vector<double>>& inputs, const vector<double>& targets,
	    const int chunkSize, std::atomic<int>& index, string threadName) {
  int currentChunk;

  // no. array partitions is no. elements / chunk size
  // ciel division using int division
  int chunks = (inputs.size() + chunkSize - 1) / chunkSize;

  // assign current chunk using the atomic int and increment it
  // it means there is still an array partition to process
  while ((currentChunk = index.fetch_add(1)) < chunks) {
    // get my index by multipyling by the chunk size
    const int startInd = currentChunk * chunkSize;

    // proccess till just before the next chunk
    const int endInd = std::min((startInd + chunkSize - 1),
				(static_cast<int>(inputs.size()) - 1));

    double sum = 0;

    // exclude the last index
    for (int i = startInd; i <= endInd; i++) {
      for (int j = 0; j < 100; j++) {
	sum += inputs[i][0];
	sum += inputs[i][1];
	sum = sum / 2;
      }
    }
  }
}

void treeTest() {
  const int DEPTH = 3;
  const double CHOOSE_CONSTANT_PROB = 0.4;

  // Solar and Wind inputs
  vector<vector<double>> inputs = {
      {20.5, 10.5}, {24.5, 11.5}, {24.5, 0.5},	{30.0, 5.0},  {35.5, 12.0},
      {40.0, 15.5}, {45.2, 20.1}, {50.0, 25.0}, {55.5, 30.2}, {60.0, 35.0},
  };

  // Target Prices for Fitness Calculation
  vector<double> targets = {150.2, 130.5, 145.0, 110.2, 95.8, 80.1, 65.4,
			    50.0,  42.1,  35.5,	 30.2,	25.4, 22.1, 20.5,
			    18.2,  17.1,  16.5,	 16.1,	15.9, 15.8};

  atomic<int> index{0};
  const int CHUNK_SIZE = 5;

  // use cref to create a constant reference that is thread safe
  // use ref for the index, so that it can be incremented
  thread thread1(doWork, cref(inputs), cref(targets), CHUNK_SIZE, ref(index),
		 "thread1");
  // thread thread2(doWork, cref(inputs), cref(targets), CHUNK_SIZE, ref(index),
  //	 "thread2");
  // thread thread3(doWork, cref(inputs), cref(targets), CHUNK_SIZE, ref(index),
  //	 "thread3");

  // execute the threads
  auto start = std::chrono::high_resolution_clock::now();

  thread1.join();
  // thread2.join();
  // thread3.join();

  auto end = std::chrono::high_resolution_clock::now();

  std::chrono::duration<double, std::milli> elapsed = end - start;

  std::cout << "Waited for: " << elapsed.count() << " ms" << std::endl;
}

int main() {
  // parseCSV();

  treeTest();

  return 0;
}
