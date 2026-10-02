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

std::optional<counter_t> reference_steps(counter_t value) {
  if (value == 0) {
    return std::nullopt;
  }
  constexpr auto max = std::numeric_limits<counter_t>::max();
  counter_t steps = 0;
  while (value != 1) {
    if (((value & 1U) && value > (max - 1) / 3) || steps == max) {
      return std::nullopt;
    }
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
  check(!collatz_steps(2 * ((max - 1) / 3 + 1)),
        "Overflow after an even step must be detected");

  for (unsigned exponent = 0; exponent < 64; ++exponent) {
    check(collatz_steps(counter_t{1} << exponent) == exponent,
          "Incorrect power-of-two sequence length");
  }
  for (unsigned exponent = 4; exponent < 64; exponent += 2) {
    check(collatz_steps(((counter_t{1} << exponent) - 1) / 3) == exponent + 1,
          "Incorrect long even run after an odd step");
  }

  // Missing entries force the jump, checked fallback, and sentinel paths to run.
  std::array<collatz_detail::cached_t, 8> cache;
  cache.fill(collatz_detail::uncached);
  cache[1] = 0;
  for (counter_t n = 1; n <= 10'000; ++n) {
    check(collatz_steps(n) == reference_steps(n),
          "Sequence differs from the unoptimized reference");
    check(collatz_detail::cached_steps(n, cache) == reference_steps(n),
          "Jump sequence differs from the unoptimized reference");
  }
  const auto jump_limit =
      collatz_detail::safe_jump_limit(collatz_detail::jump_bits);
  for (counter_t n = jump_limit - 32; n <= jump_limit + 32; ++n) {
    check(collatz_detail::cached_steps(n, cache) == reference_steps(n),
          "Incorrect sequence at the jump overflow boundary");
  }
  counter_t sample = 0x9e3779b97f4a7c15ULL;
  for (unsigned i = 0; i < 10'000; ++i) {
    sample ^= sample << 13;
    sample ^= sample >> 7;
    sample ^= sample << 17;
    const auto expected = reference_steps(sample);
    check(collatz_steps(sample) == expected,
          "Large sequence differs from the checked reference");
    check(collatz_detail::cached_steps(sample, cache) == expected,
          "Large jump sequence differs from the checked reference");
  }
}

void test_parallel_ranges() {
  constexpr counter_t cache_size = collatz_detail::cache_capacity;
  constexpr counter_t block = collatz_detail::group_size;
  constexpr std::array<counter_t, 21> limits{
      0, 1, 2, 10, 100, 255, 256, 257, 1'023, 1'024, 1'025, 10'000,
      cache_size - 1, cache_size, cache_size + 1,
      cache_size + block - 1, cache_size + block, cache_size + block + 1,
      cache_size + 8 * block - 1, cache_size + 8 * block,
      cache_size + 8 * block + 17};
  constexpr std::array<std::size_t, 3> worker_counts{1, 2, 8};
  counter_t checked_limit = 0;
  counter_t expected_steps = 0;
  for (const auto limit : limits) {
    while (checked_limit < limit) {
      const auto steps = reference_steps(++checked_limit);
      check(steps.has_value(), "Reference range overflowed");
      expected_steps += *steps;
    }

    for (const auto threads : worker_counts) {
      const auto result = collatz(limit, threads);
      check(result.numbers == limit, "Parallel range skipped or repeated inputs");
      check(result.steps == expected_steps,
            "Parallel step total differs from the reference");
    }
  }

  for (const auto limit : {0, 10}) {
    bool rejected = false;
    try {
      (void)collatz(limit, 0);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    check(rejected, "Zero workers must be rejected");
  }
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
