#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <print>
#include <thread>
#include <vector>

using counter_t = std::uint64_t;

constexpr counter_t LIMIT = 2'147'483'647; // 2^31 - 1

void collatz(counter_t limit,
             std::atomic<counter_t>& counter,
             std::atomic<counter_t>& total) noexcept
{
    for (auto n = counter.fetch_add(1, std::memory_order_relaxed);
         n <= limit;
         n = counter.fetch_add(1, std::memory_order_relaxed))
    {
        auto value = n;

        while (value != 1) {
            while ((value & 1U) != 0U) {
                value = (3 * value + 1) / 2;
            }
            while ((value & 1U) == 0U) {
                value /= 2;
            }
        }

        total.fetch_add(1, std::memory_order_relaxed);
    }
}

int main(int argc, char* argv[])
{
    if (argc != 1) {
        std::println(stderr, "Usage: {}",
                     (argc > 0 && argv[0] != nullptr) ? argv[0] : "./collatz");
        return EXIT_FAILURE;
    }

    std::atomic<counter_t> total{0};
    std::atomic<counter_t> counter{1};

    const auto hw_threads = std::thread::hardware_concurrency();
    const std::size_t thread_count = hw_threads == 0U ? 1U : static_cast<std::size_t>(hw_threads);

    std::println("Running with {} thread(s); this may take some time...", thread_count);

    std::vector<std::jthread> threads;
    threads.reserve(thread_count);

    const auto start = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < thread_count; ++i) {
        threads.emplace_back(collatz, LIMIT, std::ref(counter), std::ref(total));
    }
    // std::jthread joins on destruction

    const auto end = std::chrono::steady_clock::now();
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::println("Program statistics:");
    std::println("\tThread count   : {}", thread_count);
    std::println("\tLimit value    : {}", LIMIT);
    std::println("\tCollatz numbers: {}", total.load(std::memory_order_relaxed));
    std::println("\tRun-time       : {} ms", elapsed_ms);

    return EXIT_SUCCESS;
}
