#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using counter_t = std::uint64_t;

struct collatz_statistics {
  counter_t numbers{0};
  counter_t steps{0};
};

inline std::optional<counter_t> collatz_steps(counter_t value) noexcept {
  if (value == 0) {
    return std::nullopt;
  }

  constexpr auto max = std::numeric_limits<counter_t>::max();
  counter_t steps = 0;

  while (value != 1) {
    if (value & 1U) {
      if (value > (max - 1) / 3 || steps > max - 2) {
        return std::nullopt;
      }
      // Combine the odd step with the following even step.
      value = (3 * value + 1) / 2;
      steps += 2;
    } else {
      if (steps == max) {
        return std::nullopt;
      }
      value /= 2;
      ++steps;
    }
  }

  return steps;
}

inline collatz_statistics collatz(counter_t limit, std::size_t thread_count) {
  if (thread_count == 0) {
    throw std::invalid_argument("Thread count must be greater than zero");
  }
  if (limit == 0) {
    return {};
  }

  constexpr counter_t batch_size = 256;
  constexpr auto max = std::numeric_limits<counter_t>::max();
  std::atomic<counter_t> counter{0};
  std::atomic<counter_t> failed_input{0};
  std::vector<collatz_statistics> results(thread_count);
  std::vector<std::jthread> threads;
  threads.reserve(thread_count);

  for (std::size_t i = 0; i < thread_count; ++i) {
    threads.emplace_back([&, i](std::stop_token stop) {
      collatz_statistics result;
      auto begin = counter.load(std::memory_order_relaxed);

      while (begin < limit && !stop.stop_requested() &&
             failed_input.load(std::memory_order_relaxed) == 0) {
        const auto end = begin + std::min(batch_size, limit - begin);
        // Bound each claim by limit so the shared cursor cannot wrap.
        if (!counter.compare_exchange_weak(begin, end,
                                           std::memory_order_relaxed)) {
          continue;
        }

        for (auto n = begin; n < end && !stop.stop_requested() &&
                             failed_input.load(std::memory_order_relaxed) == 0;) {
          const auto steps = collatz_steps(++n);
          if (!steps || result.steps > max - *steps) {
            failed_input.store(n, std::memory_order_relaxed);
            break;
          }
          ++result.numbers;
          result.steps += *steps;
        }

        begin = counter.load(std::memory_order_relaxed);
      }

      results[i] = result;
    });
  }

  // Join without requesting cancellation on the successful path. On startup
  // failure, jthread destructors request stop and join the workers already made.
  for (auto& thread : threads) {
    thread.join();
  }

  if (const auto n = failed_input.load(std::memory_order_relaxed); n != 0) {
    throw std::overflow_error("Collatz arithmetic overflow for input " +
                              std::to_string(n));
  }

  collatz_statistics total;
  for (const auto& result : results) {
    if (total.steps > max - result.steps) {
      throw std::overflow_error("Collatz step count overflow");
    }
    total.numbers += result.numbers;
    total.steps += result.steps;
  }
  return total;
}
