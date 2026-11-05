#include "sum.h"

long long Sum(const int *array, size_t begin, size_t end) {
  long long total = 0;

  for (size_t i = begin; i < end; ++i) {
    total += array[i];
  }

  return total;
}