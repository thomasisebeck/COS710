
#pragma once

namespace op {

static const double EPSILON = 1e-6;

double protectedDivide(double numerator, double denominator);

double protectedSqrt(double num);

double protectedPow(double base, double exp);

enum class FreezeType { BOTTOM = 0, TOP = 1, NONE = 2 };

} // namespace op
