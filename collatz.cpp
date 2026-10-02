#include "collatz.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <print>
#include <thread>

constexpr counter_t LIMIT = 2'147'483'647;  // 2^31 - 1

int main(int argc, char* argv[]) {
  if (argc != 1) {
    std::println(stderr, "Usage: {}",
                 (argc > 0 && argv[0] != nullptr) ? argv[0] : "./collatz");
    return EXIT_FAILURE;
  }

  const auto hw_threads = std::thread::hardware_concurrency();
  const std::size_t thread_count = hw_threads == 0U
                                       ? 1U
                                       : static_cast<std::size_t>(hw_threads);

  std::println("Running with {} thread(s); this may take some time...", thread_count);

  const auto start = std::chrono::steady_clock::now();

  collatz_statistics total;
  try {
    total = collatz(LIMIT, thread_count);
  } catch (const std::exception& error) {
    std::println(stderr, "Error: {}", error.what());
    return EXIT_FAILURE;
  }

  const auto end = std::chrono::steady_clock::now();
  const auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  std::println("Program Statistics:");
  std::println("\tThread Count   : {}", thread_count);
  std::println("\tLimit Value    : {}", LIMIT);
  std::println("\tCollatz Numbers: {}", total.numbers);
  // Observing the calculated steps prevents the optimizer from discarding the
  // sequence traversal as a side-effect-free loop.
  std::println("\tCollatz Steps  : {}", total.steps);
  std::println("\tRun-time       : {} ms", elapsed_ms);

  return EXIT_SUCCESS;
}
