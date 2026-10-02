#include "collatz.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <utility>

namespace {

void check(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

counter_t reference_steps(counter_t value) {
  counter_t steps = 0;
  while (value != 1) {
    value = (value & 1U) ? 3 * value + 1 : value / 2;
    ++steps;
  }
  return steps;
}

void test_sequences() {
  constexpr std::array<std::pair<counter_t, counter_t>, 10> cases{{
      {1, 0}, {2, 1}, {3, 7}, {6, 8}, {7, 16}, {27, 111}, {97, 118},
      {837'799, 524}, {2'147'483'647, 450}, {counter_t{1} << 63, 63},
  }};
  for (const auto& [value, expected] : cases) {
    check(collatz_steps(value) == expected, "Incorrect known sequence length");
  }

  check(!collatz_steps(0), "Zero must be rejected");
  constexpr auto max = std::numeric_limits<counter_t>::max();
  check(!collatz_steps(max), "Overflow must be detected");
  check(!collatz_steps((max - 1) / 3 + 1),
        "Overflow to zero must be detected");

  for (counter_t n = 1; n <= 10'000; ++n) {
    check(collatz_steps(n) == reference_steps(n),
          "Sequence differs from the unoptimized reference");
  }
}

void test_parallel_ranges() {
  constexpr std::array<counter_t, 10> limits{
      0, 1, 2, 10, 100, 255, 256, 257, 1'024, 10'000};
  constexpr std::array<std::size_t, 3> worker_counts{1, 2, 8};
  for (const auto limit : limits) {
    counter_t expected_steps = 0;
    for (counter_t n = 1; n <= limit; ++n) {
      expected_steps += reference_steps(n);
    }

    for (const auto threads : worker_counts) {
      const auto result = collatz(limit, threads);
      check(result.numbers == limit, "Parallel range skipped or repeated inputs");
      check(result.steps == expected_steps,
            "Parallel step total differs from the reference");
    }
  }

  bool rejected = false;
  try {
    (void)collatz(10, 0);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  check(rejected, "Zero workers must be rejected");
}

}  // namespace

int main() {
  try {
    test_sequences();
    test_parallel_ranges();
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return EXIT_FAILURE;
  }

  std::puts("All Collatz tests passed");
  return EXIT_SUCCESS;
}
