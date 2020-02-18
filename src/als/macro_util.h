#pragma once

#define Max(a, b) ((a) >= (b) ? (a) : (b))
#define Min(a, b) ((a) <= (b) ? (a) : (b))
#define Abs(a) ((a) >= 0 ? (a) : -(a))
#define FloatEq(a, b, epsilon) (Abs((a) - (b)) <= (epsilon))