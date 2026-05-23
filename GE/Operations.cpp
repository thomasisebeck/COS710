
#include "Operations.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
using namespace std;

double op::protectedDivide(double numerator, double denominator) {
  if (abs(denominator) < EPSILON) {
    return 1;
  }

  return numerator / denominator;
}

double protectedSqrt(double num) { return sqrt(std::abs(num)); }

double protectedPow(double base, double exp) {
  const double MAX_EXP = 10;

  // exponent can only go up to 10
  exp = clamp(exp, -MAX_EXP, MAX_EXP);

  return pow(std::abs(base), exp);
}
