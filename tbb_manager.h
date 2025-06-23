#ifndef TBB_MANAGER_H_
#define TBB_MANAGER_H_

#include <gflags/gflags.h>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "tbb/tbb.h"

DECLARE_string(custom_tbb_parallel_control);

namespace varch {
namespace tools {
namespace determinism {

struct ThreadContext {
  int thread_id;
  std::string task_name;
  uint64_t task_instance_id;
};

struct TBBState {
  std::shared_ptr<tbb::task_arena> arena;
  bool initialized = false;
};

class TBBManager {
 public:
  static TBBManager& GetInstance();
  std::shared_ptr<tbb::task_arena> Init(const std::string& tbb_name);

  template <typename IntType, typename Func>
  void ParallelFor(const std::string& tbb_name, IntType start, IntType end,
                   const Func& task);

  template <typename T, typename Func>
  void ParallelFor(const std::string& tbb_name,
                   const tbb::blocked_range<T>& range, const Func& task);

  void Release();
  ~TBBManager();

  static std::map<std::string, int> InitTBBParallelCountDefines();
  static std::map<std::string, int>& GetTBBParallelCountDefines();

 private:
  TBBManager() = default;

  uint64_t GenerateUniqueTaskId() const;

  TBBManager(const TBBManager&) = delete;
  TBBManager& operator=(const TBBManager&) = delete;

  std::unordered_map<std::string, TBBState> task_arenas_;
  std::unordered_map<std::string, std::queue<ThreadContext>> thread_contexts_;

  mutable std::mutex arenas_mutex_;
  mutable std::mutex contexts_mutex_;
};

template <typename IntType, typename Func>
void TBBManager::ParallelFor(const std::string& tbb_name, IntType start,
                             IntType end, const Func& task) {
  uint64_t task_id = GenerateUniqueTaskId();
  std::string unique_task_name = tbb_name + "_" + std::to_string(task_id);

  std::shared_ptr<tbb::task_arena> arena = Init(tbb_name);

  arena->execute(
      [this, task, tbb_name, task_id, unique_task_name, start, end]() {
        tbb::parallel_for(
            tbb::blocked_range<IntType>(start, end),
            [this, task, tbb_name, task_id,
             unique_task_name](const tbb::blocked_range<IntType>& range) {
              // Create thread-local storage for contexts
              std::vector<ThreadContext> local_contexts;
              local_contexts.reserve(range.end() - range.begin());

              // Execute tasks and record contexts without locks
              for (IntType i = range.begin(); i < range.end(); ++i) {
                ThreadContext context{static_cast<int>(i), tbb_name, task_id};
                task(i);
                local_contexts.push_back(context);
              }

              // Only lock once to push all local contexts to the shared queue
              if (!local_contexts.empty()) {
                std::lock_guard<std::mutex> lock(contexts_mutex_);
                auto& queue = thread_contexts_[unique_task_name];
                for (auto&& ctx : local_contexts) {
                  queue.push(std::move(ctx));
                }
              }
            });
      });
}

template <typename T, typename Func>
void TBBManager::ParallelFor(const std::string& tbb_name,
                             const tbb::blocked_range<T>& range,
                             const Func& task) {
  uint64_t task_id = GenerateUniqueTaskId();
  std::string unique_task_name = tbb_name + "_" + std::to_string(task_id);

  std::shared_ptr<tbb::task_arena> arena = Init(tbb_name);

  arena->execute([this, &task, &range, tbb_name, task_id, unique_task_name]() {
    tbb::parallel_for(range, [this, &task, tbb_name, task_id, unique_task_name](
                                 const tbb::blocked_range<T>& sub_range) {
      // Create thread-local storage for contexts
      std::vector<ThreadContext> local_contexts;
      local_contexts.reserve(sub_range.size());

      // Execute tasks and record contexts without locks
      for (auto it = sub_range.begin(); it != sub_range.end(); ++it) {
        ThreadContext context{0, tbb_name, task_id};
        task(it);
        local_contexts.push_back(context);
      }

      // Only lock once to push all local contexts to the shared queue
      if (!local_contexts.empty()) {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        auto& queue = thread_contexts_[unique_task_name];
        for (auto&& ctx : local_contexts) {
          queue.push(std::move(ctx));
        }
      }
    });
  });
}

#define VOY_ARENA_TBB_WITH_GFLAGS_PARALLEL_FOR(gflagsName, ...)       \
  do {                                                                \
    varch::tools::determinism::TBBManager::GetInstance().ParallelFor( \
        #gflagsName, __VA_ARGS__);                                    \
  } while (0)

}  // namespace determinism
}  // namespace tools
}  // namespace varch

#endif  // TBB_MANAGER_H_
