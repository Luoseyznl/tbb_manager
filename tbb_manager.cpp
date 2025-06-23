#include "tbb_manager.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <sstream>
#include <vector>

TBBManager& TBBManager::GetInstance() {
  static TBBManager instance;
  return instance;
}

std::map<std::string, int> TBBManager::InitTBBParallelCountDefines() {
  std::map<std::string, int> tbb_parallel_count_defines;
  if (FLAGS_custom_tbb_parallel_control.empty()) {
    return tbb_parallel_count_defines;
  }

  std::istringstream ss(FLAGS_custom_tbb_parallel_control);
  std::string item;
  while (std::getline(ss, item, ',')) {
    auto pos = item.find(':');
    if (pos != std::string::npos) {
      std::string name = item.substr(0, pos);
      int count = std::stoi(item.substr(pos + 1));
      tbb_parallel_count_defines[name] = count;
    }
  }
  return tbb_parallel_count_defines;
}

std::map<std::string, int>& TBBManager::GetTBBParallelCountDefines() {
  static std::once_flag flag;
  static std::map<std::string, int> tbb_parallel_count_defines;

  std::call_once(flag, []() {
    tbb_parallel_count_defines = InitTBBParallelCountDefines();
  });
  return tbb_parallel_count_defines;
}

std::shared_ptr<tbb::task_arena> TBBManager::Init(const std::string& tbb_name) {
  {
    std::lock_guard<std::mutex> read_lock(arenas_mutex_);
    auto it = task_arenas_.find(tbb_name);
    if (it != task_arenas_.end() && it->second.initialized) {
      return it->second.arena;
    }
  }

  std::lock_guard<std::mutex> write_lock(arenas_mutex_);

  // Check again in case another thread initialized while we were waiting
  auto it = task_arenas_.find(tbb_name);
  if (it != task_arenas_.end() && it->second.initialized) {
    return it->second.arena;
  }

  int thread_count = tbb::this_task_arena::max_concurrency();
  auto& parallel_count_defines = GetTBBParallelCountDefines();
  if (parallel_count_defines.find(tbb_name) != parallel_count_defines.end()) {
    thread_count = parallel_count_defines[tbb_name];
  }

  auto& state = task_arenas_[tbb_name];
  state.arena = std::make_shared<tbb::task_arena>(thread_count);
  state.initialized = true;

  return state.arena;
}

uint64_t TBBManager::GenerateUniqueTaskId() const {
  // Combine a timestamp with a counter for uniqueness
  static std::atomic<uint32_t> global_counter(0);
  uint64_t timestamp = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFFFF);

  uint64_t counter = global_counter.fetch_add(1, std::memory_order_relaxed);
  return (timestamp << 32) | (counter & 0xFFFFFFFF);
}

void TBBManager::Release() {
  std::lock_guard<std::mutex> arenas_lock(arenas_mutex_);
  std::lock_guard<std::mutex> contexts_lock(contexts_mutex_);

  task_arenas_.clear();
  thread_contexts_.clear();
}

TBBManager::~TBBManager() { Release(); }
