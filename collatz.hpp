#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using counter_t = std::uint64_t;

struct collatz_statistics {
  counter_t numbers{0};
  counter_t steps{0};
};

namespace collatz_detail {

constexpr auto max = std::numeric_limits<counter_t>::max();

// Collapse an odd step and all following divisions by two. The intermediate
// 3n + 1 must fit even when the value after division would fit.
inline bool advance(counter_t& value, counter_t& steps) noexcept {
  counter_t added = 0;
  if (value & 1U) {
    if (value > (max - 1) / 3) {
      return false;
    }
    value = 3 * value + 1;
    added = 1;
  }
  const auto shift = std::countr_zero(value);
  added += shift;
  if (steps > max - added) {
    return false;
  }
  value >>= shift;
  steps += added;
  return true;
}

}  // namespace collatz_detail

inline std::optional<counter_t> collatz_steps(counter_t value) noexcept {
  if (value == 0) {
    return std::nullopt;
  }

  counter_t steps = 0;
  while (value != 1) {
    if (!collatz_detail::advance(value, steps)) {
      return std::nullopt;
    }
  }
  return steps;
}

namespace collatz_detail {

using cached_t = std::uint16_t;
constexpr auto uncached = std::numeric_limits<cached_t>::max();
constexpr std::size_t cache_capacity = 1U << 24;
constexpr unsigned jump_bits = 12;
constexpr unsigned group_bits = 20;
constexpr counter_t group_size = counter_t{1} << group_bits;
static_assert(cache_capacity >= group_size && cache_capacity % group_size == 0);
static_assert(jump_bits <= 20 && group_bits <= 20);  // 3^20 fits uint32_t.

struct jump {
  std::uint32_t multiplier;
  std::uint32_t remainder;
  std::uint32_t steps;
};

// For T(n) = n/2 (even), (3n+1)/2 (odd), k iterations map
// n = q*2^k + r to q*3^a + T^k(r), using k+a ordinary steps.
constexpr jump make_jump(counter_t residue, unsigned bits) noexcept {
  std::uint32_t multiplier = 1;
  unsigned steps = bits;
  for (unsigned i = 0; i < bits; ++i) {
    if (residue & 1U) {
      residue = 3 * residue + 1;
      multiplier *= 3;
      ++steps;
    }
    residue /= 2;
  }
  return {multiplier, static_cast<std::uint32_t>(residue), steps};
}

constexpr counter_t safe_jump_limit(unsigned bits) noexcept {
  counter_t power = 1;
  for (unsigned i = 0; i < bits; ++i) {
    power *= 3;
  }
  // Even omitting every division, intermediates stay below 3^k*(n+1).
  return max / power - 1;
}

inline const auto& jumps() {
  static const auto table = [] {
    std::array<jump, 1U << jump_bits> result{};
    for (std::size_t r = 0; r < result.size(); ++r) {
      result[r] = make_jump(r, jump_bits);
    }
    return result;
  }();
  return table;
}

inline std::optional<counter_t> cached_steps(
    counter_t value, std::span<const cached_t> cache) noexcept {
  counter_t steps = 0;
  while (value >= cache.size() || cache[value] == uncached) {
    // A value >= 2^k cannot reach 1 before the end of a k-step jump.
    if (value >= (counter_t{1} << jump_bits) &&
        value <= safe_jump_limit(jump_bits)) {
      const auto& entry = jumps()[value & ((1U << jump_bits) - 1)];
      if (steps > max - entry.steps) {
        return std::nullopt;
      }
      value = (value >> jump_bits) * entry.multiplier + entry.remainder;
      steps += entry.steps;
    } else if (!advance(value, steps)) {
      return std::nullopt;
    }
  }
  if (steps > max - cache[value]) {
    return std::nullopt;
  }
  return steps + cache[value];
}

struct group {
  std::uint32_t multiplier;
  std::uint32_t remainder;
  std::uint32_t count;
  std::uint32_t representative;
};

inline const auto& groups() {
  static const auto table = [] {
    std::vector<group> result;
    result.reserve(group_size);
    for (std::uint32_t r = 0; r < group_size; ++r) {
      const auto prefix = make_jump(r, group_bits);
      result.push_back({prefix.multiplier, prefix.remainder, 1, r});
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
      return a.multiplier < b.multiplier ||
             (a.multiplier == b.multiplier && a.remainder < b.remainder);
    });
    std::size_t size = 0;
    for (const auto entry : result) {
      if (size != 0 &&
          result[size - 1].multiplier == entry.multiplier &&
          result[size - 1].remainder == entry.remainder) {
        result[size - 1].count += entry.count;
      } else {
        result[size++] = entry;
      }
    }
    result.resize(size);
    return result;
  }();
  return table;
}

[[noreturn]] inline void overflow(counter_t input) {
  throw std::overflow_error("Collatz arithmetic overflow for input " +
                            std::to_string(input));
}

}  // namespace collatz_detail

inline collatz_statistics collatz(counter_t limit, std::size_t thread_count) {
  if (thread_count == 0) {
    throw std::invalid_argument("Thread count must be greater than zero");
  }
  if (limit == 0) {
    return {};
  }

  using namespace collatz_detail;
  constexpr counter_t batch_size = 4'096;
  constexpr auto max = std::numeric_limits<counter_t>::max();

  // Build verified suffix lengths in ascending order, then share them read-only.
  // Lengths too large for the compact cache remain uncached, never truncated.
  std::vector<cached_t> cache(
      std::min<counter_t>(limit, cache_capacity - 1) + 1, uncached);
  cache[1] = 0;
  collatz_statistics total{cache.size() - 1, 0};
  for (counter_t n = 2; n < cache.size(); ++n) {
    counter_t value = n;
    counter_t steps = 0;
    while (value >= n || cache[value] == uncached) {
      if (!advance(value, steps)) {
        overflow(n);
      }
    }
    if (steps > max - cache[value] ||
        total.steps > max - (steps + cache[value])) {
      overflow(n);
    }
    steps += cache[value];
    total.steps += steps;
    if (steps < uncached) {
      cache[n] = static_cast<cached_t>(steps);
    }
  }
  if (total.numbers == limit) {
    return total;
  }

  const bool grouped = limit <= safe_jump_limit(group_bits);
  const auto full_blocks = (limit >> group_bits) +
                          ((limit & (group_size - 1)) == group_size - 1);
  const auto first_block = cache.size() / group_size;
  if (grouped) {
    // Every k-bit parity pattern occurs once per complete block: k divisions
    // per input and k*2^(k-1) odd operations across the block.
    constexpr auto prefix_steps = 3 * group_bits * (group_size / 2);
    const auto blocks = full_blocks - first_block;
    if (blocks > (max - total.steps) / prefix_steps) {
      throw std::overflow_error("Collatz step count overflow");
    }
    total.steps += blocks * prefix_steps;
    // Only complete blocks are grouped; handle the final partial block exactly.
    for (auto n = full_blocks * group_size; n <= limit; ++n) {
      const auto steps = cached_steps(n, cache);
      if (!steps || total.steps > max - *steps) {
        overflow(n);
      }
      ++total.numbers;
      total.steps += *steps;
    }
    thread_count = std::min<counter_t>(thread_count, full_blocks - first_block);
  } else {
    thread_count = std::min<counter_t>(thread_count, limit - total.numbers);
  }
  if (total.numbers == limit) {
    return total;
  }

  const auto* grouped_prefixes = grouped ? &groups() : nullptr;
  std::atomic<counter_t> counter{grouped ? first_block : total.numbers};
  std::atomic<counter_t> failed_input{0};
  std::vector<collatz_statistics> results(thread_count);
  std::vector<std::jthread> threads;
  threads.reserve(thread_count);

  for (std::size_t i = 0; i < thread_count; ++i) {
    threads.emplace_back([&, i](std::stop_token stop) {
      collatz_statistics result;
      if (grouped) {
        while (!stop.stop_requested() &&
               failed_input.load(std::memory_order_relaxed) == 0) {
          const auto block = counter.fetch_add(1, std::memory_order_relaxed);
          if (block >= full_blocks) {
            break;
          }
          for (const auto& entry : *grouped_prefixes) {
            const auto steps = cached_steps(
                block * entry.multiplier + entry.remainder, cache);
            if (!steps || *steps > max / entry.count ||
                result.steps > max - *steps * entry.count) {
              failed_input.store(block * group_size + entry.representative,
                                 std::memory_order_relaxed);
              break;
            }
            result.steps += *steps * entry.count;
          }
          result.numbers += group_size;
        }
        results[i] = result;
        return;
      }

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
          const auto steps = cached_steps(++n, cache);
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
    overflow(n);
  }

  for (const auto& result : results) {
    if (total.steps > max - result.steps) {
      throw std::overflow_error("Collatz step count overflow");
    }
    total.numbers += result.numbers;
    total.steps += result.steps;
  }
  return total;
}
