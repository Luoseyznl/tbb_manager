# TBBManager

## Overview

`TBBManager` is a singleton utility class for managing Intel Threading Building Blocks (TBB) task arenas in a centralized, thread-safe manner. It provides deterministic control over parallel execution environments in a multi-threaded application, allowing fine-grained configuration of thread counts for different components.

## Features

- **Named Task Arenas**: Create and retrieve task arenas with specific identifiers
- **Configurable Thread Counts**: Control parallelism levels for each named arena
- **Thread Safety**: Thread-safe initialization and access to resources
- **Unique Task ID Generation**: Generate unique IDs for tracking parallel tasks
- **Resource Management**: Proper cleanup of TBB resources
- **Context Tracking**: Records thread execution contexts for debugging and analysis

## Usage

### Basic Usage

```cpp
#include "tbb_manager.h"

void myFunction() {
  // Get the singleton instance
  auto& manager = varch::tools::determinism::TBBManager::GetInstance();
  
  // Initialize a named task arena
  auto arena = manager.Init("my_parallel_region");
  
  // Execute a parallel task in this arena
  arena->execute([&]{
    // Your parallel code here
    tbb::parallel_for(0, 100, [](int i) {
      // Work item
    });
  });
}
```

### Using Simplified Macros

For convenience, the library provides macros to simplify parallel execution:

```cpp
#include "tbb_manager.h"

void processData(std::vector<float>& data) {
  // Use the macro to create a named parallel region
  VOY_ARENA_TBB_WITH_GFLAGS_PARALLEL_FOR(data_processing, 
                                         0, data.size(),
                                         [&data](int i) {
    // Process each data element
    data[i] = process(data[i]);
  });
}
```

### Using Blocked Range

```cpp
#include "tbb_manager.h"

void processImages(std::vector<Image>& images) {
  auto& manager = varch::tools::determinism::TBBManager::GetInstance();
  
  tbb::blocked_range<size_t> range(0, images.size());
  manager.ParallelFor("image_processing", range, [&](const auto& r) {
    for (size_t i = r.begin(); i != r.end(); ++i) {
      // Process each image
      images[i].process();
    }
  });
}
```

### Controlling Thread Counts

Thread counts can be configured via the `FLAGS_custom_tbb_parallel_control` flag using a comma-separated list of name:count pairs:

```bash
# Example: Set "video_processing" to use 4 threads and "physics_simulation" to use 8
./my_application --custom_tbb_parallel_control=video_processing:4,physics_simulation:8
```

## Implementation Details

- **Thread Safety**: The manager uses mutexes to ensure thread-safe initialization and access
- **Double-Checked Locking**: Efficiently handles concurrent initialization requests
- **Resource Management**: All resources are automatically released when the manager is destroyed
- **Context Tracking**: Execution contexts are stored in thread-local storage first, then batched into shared storage to minimize lock contention

## Building with Sanitizers

To build and test with sanitizers:

```bash
# With ThreadSanitizer
mkdir -p build-tsan && cd build-tsan
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=thread -g"
make
TSAN_OPTIONS="history_size=7" ./your_test_executable

# With AddressSanitizer
mkdir -p build-asan && cd build-asan
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address -g"
make
ASAN_OPTIONS="detect_leaks=1" ./your_test_executable
```

## Dependencies

- Intel Threading Building Blocks (TBB)
- GFlags for configuration options
- C++11 or newer

## License

MIT License

Copyright (c) 2025

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Similar code found with 2 license types
