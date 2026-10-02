# collatz

A high-performance, multithreaded C++23 implementation of the Collatz iteration test.
The program verifies the finite range `[1, LIMIT]` and reports the exact sum of the
steps needed for its inputs to reach 1. The default `LIMIT` is `2,147,483,647`
(`2^31 - 1`).

The main optimization is reusing work: cache verified sequence tails, jump over
several transformations at once, and combine inputs that reach the same tail.
Every `3n + 1` and `n / 2` operation still counts separately in the reported total.

---

## Features

- Modern **C++23** implementation
- Selects workers using `std::thread::hardware_concurrency()`, with a fallback of one
- Bounded suffix cache and precomputed sequence jumps
- Groups equivalent sequence prefixes before distributing complete blocks to workers
- Atomic work allocation and per-thread totals to reduce contention
- RAII thread management with `std::jthread` and cancellation on startup failure
- Checked 64-bit sequence arithmetic and step counts
- Reports the total number of Collatz steps, keeping the computed work observable in optimized builds
- High-resolution execution timing
- Deterministic input and step totals regardless of worker scheduling

---

## Build Requirements

- **C++23-capable compiler and standard library**, including `std::println` and `std::jthread`
- **Make** (optional, if using the included Makefile)

---

## Build & Run

### Using Make

```bash
git clone https://github.com/v0-3/collatz.git
cd collatz
make
./collatz
```

### Manual Build

```bash
clang++ -std=c++23 -O3 -march=native -pthread collatz.cpp -o collatz
./collatz
```

The executable accepts no command-line arguments. To change its workload, edit
`LIMIT` in [collatz.cpp](collatz.cpp) and rebuild. The functions in
[collatz.hpp](collatz.hpp) also accept individual inputs or a range and worker count.

### Tests

```bash
make test
```

The tests compare results with a checked, one-operation-at-a-time reference. They cover:

- Known sequences, all 64 representable powers of two, and long runs of even steps
- Zero rejection and overflow, including overflow after an initial even step
- Missing cache entries, jump safety boundaries, and 10,000 deterministic samples across 64-bit inputs
- Range totals around cache and block boundaries with 1, 2, and 8 requested workers
- Zero-worker rejection and command-line argument validation

The largest automated range is `25,165,841`; `make test` does not run the full
default workload. Additional manual validation included AddressSanitizer,
UndefinedBehaviorSanitizer, full default runs, and a comparison over
`6,000,000,017` inputs between the production fallback and a temporary variant
using 16-transformation grouping. These additional checks are not part of `make test`.

---

## How the Computation Works

The implementation is in [collatz.hpp](collatz.hpp). Let `s(n)` be the ordinary
Collatz step count, with `s(1) = 0`.

### 1. Collapse consecutive even steps

For a positive value, `std::countr_zero` gives the number of consecutive divisions
by two that can be performed with one shift. After an odd `3n + 1` operation, the
implementation immediately removes all those factors of two and adds the full
operation count.

For example, `5 -> 16 -> 8 -> 4 -> 2 -> 1` becomes one compressed transition from
5 to 1, but still contributes five steps. This checked reduction is used by
`collatz_steps`, cache construction, and the fallback for unsafe table jumps.

### 2. Cache verified sequence tails

Each `collatz` call builds a cache in ascending input order. To compute `s(n)`, it
follows the sequence until it reaches an already cached value, then adds that
known tail length. It also accumulates the totals for the cached input range.

The cache holds at most `2^24` entries, covering inputs `1` through `16,777,215`.
Entries use `uint16_t`, so the maximum allocation is **32 MiB**. The value `65,535`
marks an uncached entry; lengths at or above that value are left uncached rather
than truncated. Their actual counts continue to use 64-bit arithmetic.

The cache is read-only once workers start. Ranges that fit entirely in the cache
finish during its construction without starting worker threads.

### 3. Jump through 12 transformations at a time

Define a reduced transformation that includes the mandatory division after an
odd operation:

```text
T(n) = n / 2          when n is even
T(n) = (3n + 1) / 2   when n is odd
```

For a fixed number of transformations `k`, write the starting value as
`n = q * 2^k + r`, where `r` contains its lowest `k` bits. Those bits determine the
first `k` parity decisions. If `a` of those decisions are odd, then:

```text
T^k(n) = q * 3^a + T^k(r)
ordinary steps added = k + a
```

A table with `2^12 = 4,096` entries stores the multiplier, remainder, and step
count for each residue. A lookup, multiplication, and addition replace twelve
reduced transformations. These jumps continue until a cached tail is reached.

Jumps are used only when the starting value is at least `2^k`, so they cannot pass
the first arrival at 1, and when their intermediate arithmetic is guaranteed to fit.

### 4. Group inputs that share a tail after 20 transformations

The same identity also applies to a complete block of `2^20 = 1,048,576` inputs
sharing the same quotient `q`. Different residues can produce the same pair
`(3^a, T^20(r))`, and therefore the same value after twenty transformations.

The implementation sorts and combines those residues into **246,912 groups**.
For each group in a block, it computes the remaining tail once and multiplies
its length by the number of represented inputs.

Prefix steps are added separately. Across a complete block, every sequence of
`k` parity decisions occurs once. There are `k * 2^k` divisions and
`k * 2^(k-1)` odd operations, giving:

```text
prefix steps per block = 3 * k * 2^(k-1)
                      = 31,457,280 when k = 20

block total = prefix steps
            + sum(group count * s(q * group multiplier + group remainder))
```

Only complete blocks beyond the cached input range use this grouping. The cache
already accounts for the initial range; any final partial block is evaluated
input by input with cached tails and jumps. Each original input is counted once.

### 5. Distribute blocks and combine totals

Workers claim complete blocks through a relaxed atomic counter and keep their
totals locally. They share immutable cache and lookup tables, so sequence
evaluation needs no locks. The requested worker count is capped by available
work; cache construction and the partial tail are handled before workers start.

Workers use `std::jthread`. Successful runs explicitly join every worker before
combining totals. If thread creation fails, the existing workers are stopped and
joined during stack unwinding.

---

## Arithmetic Safety and Fallbacks

Inputs, sequence values, and totals use `uint64_t`. An odd operation must pass
`n <= (UINT64_MAX - 1) / 3` before computing `3n + 1`, even if dividing its result
would produce a representable value. Step additions, group multiplications, and
the final reduction are checked as well.

Table jumps use the conservative bound:

```text
safe_jump_limit(k) = floor(UINT64_MAX / 3^k) - 1
```

Even if every division were omitted, intermediate values would stay below
`3^k * (n + 1)`. Values outside this bound use checked scalar reduction instead
of the jump; crossing the bound does not by itself imply an overflow.

Grouping requires the entire requested limit to satisfy this bound for `k = 20`,
currently `5,290,474,531`. Larger ranges use atomic claims of up to **4,096 inputs**
and evaluate each input with the same cache and safe 12-transformation jumps.
Work claims are bounded by the limit to prevent the shared cursor from wrapping.

The public functions preserve explicit failure behavior:

| Function | Result and errors |
| --- | --- |
| `collatz_steps(value)` | Returns an optional step count; zero or arithmetic overflow returns `std::nullopt`. |
| `collatz(limit, thread_count)` | Returns input and step totals; zero workers throw `std::invalid_argument`, and arithmetic or total overflow throws `std::overflow_error`. A zero limit returns empty totals when the worker count is valid. |

The executable reports caught exceptions on stderr and exits with a failure status.

---

## Program Output

Example (thread count and run-time vary by machine):

```
Running with 11 thread(s); this may take some time...
Program Statistics:
    Thread Count   : 11
    Limit Value    : 2147483647
    Collatz Numbers: 2147483647
    Collatz Steps  : 453538085656
    Run-time       : 938 ms
```

---

## Performance and Memory

Measured on an **Apple M3 Pro with 11 workers**, using Apple Clang 21 and the
project's release flags (`-std=c++23 -O3 -march=native -pthread`), for the default
`2,147,483,647` inputs. The baseline is commit `249c695`; the optimized
implementation is commit `af174d5`.

| Measurement | Previous implementation | Current approach |
| --- | ---: | ---: |
| Run-time | 58.590 s | 0.938 s |
| Peak resident memory | About 2 MiB | About 50 MiB |
| Total Collatz steps | 453,538,085,656 | 453,538,085,656 |

The previous time is one baseline run. The current time is the median of three
fresh runs (`1.003`, `0.936`, and `0.938` seconds), including cache and table setup:
approximately **62× faster** on that machine. Results depend on hardware,
compiler, input range, and system load.

The speedup trades memory for reused computation. In addition to the 32 MiB
suffix cache, the current group vector retains a 16 MiB allocation after merging
its entries, and the 12-transformation jump table occupies 48 KiB on this build.
Smaller ranges allocate a smaller cache and can finish without constructing the
group table. Lookup tables are initialized once per process; the suffix cache is
rebuilt for each `collatz` call.

The cache capacity and jump/group widths are constants in `collatz_detail`.
Their values also enforce arithmetic and block-alignment assumptions through
static assertions. `-march=native` targets the build machine's CPU; use suitable
architecture flags when building a binary for another machine.

---

## License

© [Luis Maya Aranda](https://github.com/v0-3). All rights reserved.  
Licensed under the **MIT License**.
