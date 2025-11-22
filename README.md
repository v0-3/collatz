# collatz

A high-performance, multithreaded C++23 implementation of the Collatz iteration test.
The program distributes the computation across all available CPU cores and verifies that each number in the range `[1, LIMIT]` reaches 1 under Collatz reduction rules.

This implementation uses modern C++23 features, strong typing, atomic counters, and clean RAII threading.

---

## Features

- Modern **C++23** implementation
- Uses **all available CPU cores** (`std::thread::hardware_concurrency()`)
- Lock-free atomic counters for minimal contention
- Clean RAII thread management
- High-resolution execution timing
- Fully deterministic behavior (no shared mutable state other than counters)

---

## Build Requirements

- **C++23-capable compiler**
  - `clang++ 17+` recommended
  - `g++ 14+` also supported
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
clang++ -std=c++23 -O3 -march=native collatz.cpp -o collatz
./collatz
```

## Program Output

Example:

```
Running with 12 thread(s); this may take some time...
Program Statistics:
    Thread Count   : 12
    Limit Value    : 2147483647
    Collatz Numbers: 2147483647
    Run-time       : 18342 ms
```

---

## Performance Notes

- The workload is fully parallelizable.
- Atomic fetch-add is used to minimize synchronization.
- `-O3 -march=native` significantly improves performance.
- For large LIMIT values, expect full CPU utilization.

---

## License

© [Luis Maya Aranda](https://github.com/v0-3). All rights reserved.  
Licensed under the **MIT License**.
