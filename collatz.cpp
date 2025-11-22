#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <print>
#include <thread>
#include <vector>

using counter_t = std::uint64_t;

constexpr counter_t LIMIT = 2'147'483'647;  // 2^31 - 1

void collatz(counter_t limit,
             std::atomic<counter_t>& counter,
             std::atomic<counter_t>& total) noexcept {
  counter_t n;

  while ((n = counter.fetch_add(1)) <= limit) {
    auto value = n;

    while (value != 1) {
      while (value & 1U) {  // while odd
        value = (3 * value + 1) / 2;
      }
      while ((value & 1U) == 0U) {  // while even
        value /= 2;
      }
    }

    total.fetch_add(1);
  }
}

int main(int argc, char* argv[]) {
  if (argc != 1) {
    std::println(stderr, "Usage: {}",
                 (argc > 0 && argv[0] != nullptr) ? argv[0] : "./collatz");
    return EXIT_FAILURE;
  }

  std::atomic<counter_t> total{0};
  std::atomic<counter_t> counter{1};

  const auto hw_threads = std::thread::hardware_concurrency();
  const std::size_t thread_count = hw_threads == 0U
                                       ? 1U
                                       : static_cast<std::size_t>(hw_threads);

  std::println("Running with {} thread(s); this may take some time...", thread_count);

  const auto start = std::chrono::steady_clock::now();

  // RAII thread management
  std::vector<std::thread> threads;
  threads.reserve(thread_count);

  for (std::size_t i = 0; i < thread_count; ++i) {
    threads.emplace_back(collatz, LIMIT, std::ref(counter), std::ref(total));
  }

  for (auto& t : threads) {
    t.join();
  }

  const auto end = std::chrono::steady_clock::now();
  const auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  std::println("Program Statistics:");
  std::println("\tThread Count   : {}", thread_count);
  std::println("\tLimit Value    : {}", LIMIT);
  std::println("\tCollatz Numbers: {}", total.load());
  std::println("\tRun-time       : {} ms", elapsed_ms);

  return EXIT_SUCCESS;
}
