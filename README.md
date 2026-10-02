# collatz

A high-performance, multithreaded C++23 implementation of the Collatz iteration test.
The program distributes the computation across all available CPU cores and verifies that each number in the range `[1, LIMIT]` reaches 1 under Collatz reduction rules.

This implementation uses modern C++23 features, checked arithmetic, batched atomic work allocation, and RAII threading.

---

## Features

- Modern **C++23** implementation
- Uses **all available CPU cores** (`std::thread::hardware_concurrency()`)
- Batched atomic work allocation and per-thread totals to reduce contention
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

### Tests

```bash
make test
```

The tests cover known sequences, zero and overflow rejection, parallel range totals,
batch boundaries, and command-line argument validation. They use small ranges and
do not run the full default workload.

## Program Output

Example (thread count and run-time vary by machine):

```
Running with 11 thread(s); this may take some time...
Program Statistics:
    Thread Count   : 11
    Limit Value    : 2147483647
    Collatz Numbers: 2147483647
    Collatz Steps  : 453538085656
    Run-time       : 52479 ms
```

---

## Performance Notes

- The workload is fully parallelizable.
- Workers claim batches of 256 inputs and accumulate totals locally to minimize synchronization.
- The step total counts each `3n + 1` and `n / 2` operation separately, including when the implementation combines them.
- `-O3 -march=native` significantly improves performance.
- For large LIMIT values, expect full CPU utilization.

---

## License

© [Luis Maya Aranda](https://github.com/v0-3). All rights reserved.  
Licensed under the **MIT License**.
