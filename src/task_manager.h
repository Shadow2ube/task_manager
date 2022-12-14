//
// Created by christian on 7/5/22.
//

#ifndef TASK_MANAGER__TASK_MANAGER_H_
#define TASK_MANAGER__TASK_MANAGER_H_

#include <vector>
#include <functional>
#include <algorithm>
#include <mutex>
#include <string>
#include <any>
#include <queue>
#include <thread>
#include <iostream>

#define GUARD(x) std::lock_guard<std::mutex> guard(x)
#define EVAL(x, y) ((x & y) == (y))
#define SET_ON(x, y) x |= y
#define SET_OFF(x, y) x &= ~y

namespace unmined {

using namespace std::chrono_literals;

/**
 * @brief The possible settings for the task manager
 */
enum tm_settings {
  KILL_ON_EMPTY = 1,
  IN_ORDER = 1 << 1,
};

/**
 * @brief The possible settings for a task
 */
enum task_settings {
  /**
   * @brief The task that the current is after
   */
  AFTER = 0,
  /**
   * @brief The pool (vector) that the task should return to
   */
  POOL = 1,
  /// The ID of the task, will be overwritten by the task manager
  ID = -1,
};

struct retype {
  std::string ret;
  int err;
};

/**
 * @brief The task container, contains a name, a function, and settings
 */
struct task {
  std::string name; // the name of the task
  std::function<retype()> func; // the function to be _run for said task
  std::unordered_map<enum task_settings, std::string> settings{ // the settings of the tesk
      {AFTER, ""},
      {POOL, name},
  };
  int id = -1;
};

/**
 * @brief The task manager class
 * @tparam WORKER_COUNT The amount of worker threads, default 4
 */
template<int WORKER_COUNT = 4>
class task_manager {
 private:
  /// The singleton instance
  static task_manager<WORKER_COUNT> *instance_;

  /// The main running thread, so that the main program can do other things
  std::thread main_thread_;
  /// The array of worker threads
  std::array<std::thread, WORKER_COUNT> workers_;

  /// The queue of tasks
  std::deque<task> queue_;
  /// A vector of the done tasks' names
  std::vector<std::string> done_;
  /// A map of the pools that functions can return to
  std::unordered_map<std::string, std::unordered_map<int, std::string>> pools_;
  /// The next available task IDs for which pool
  std::unordered_map<std::string, int> pool_ntIDs;
  /// If the task manager is paused
  bool is_paused_ = true;
  /// If the task manager is stopped, will kill the task manager cleanly
  bool stop_ = false;
  /// The settings, only 2 exist, but there are 16 possible
  uint16_t settings_ = 0 | (~KILL_ON_EMPTY | IN_ORDER);

  // the mutexes for each variable that will be modified
  std::mutex settings_lock_;
  std::mutex is_paused_lock_;
  std::mutex kill_lock_;
  std::mutex done_lock_;
  std::mutex queue_lock_;
  std::mutex pools_lock_;
  std::mutex pool_ntids_lock_;

 public:
  // TODO: REWORK THIS, maybe make them private or sm

  /**
   * @brief The function called when a task starts
   * @param t task - The task
   * @param wid int - The worker ID
   */
  std::function<void(const task &t, const int &wid)> task_start_callback
      = [](const task &t, const int &wid) {};
  /**
   * @brief The function called when a task finishes
   * @param t task - The task
   * @param wid int - The worker ID
   */
  std::function<void(const task &t, const int &wid)> task_stop_callback
      = [](const task &t, const int &wid) {};
  /**
   * @brief The function called when a task fails
   * @param t task - The task
   * @param wid int - The worker ID
   * @param err int - The error returned from the task
   */
  std::function<void(const task &t, const int &wid, const int &err)> task_fail_callback
      = [](const task &t, const int &wid, const int &err) {};
  /**
   * @brief The function called when a worker starts
   * @param wid int - The worker ID
   */
  std::function<void(const int &wid)> worker_start_callback = [](const int &wid) {};

  /**
   * @brief The function called when a worker stops
   * @param wid int - The worker ID
   */
  std::function<void(const int &wid)> worker_stop_callback = [](const int &wid) {};

 private:
  /// makes a new task manager, do not use this
  task_manager();
  /// deletes the task manager safely
  ~task_manager();

  /**
   * @brief The run function that starts all the workers and main thread
   */
  void _run();
  /**
   * @brief Starts a worker with an ID
   * @param id int - The ID of the worker
   */
  void _run_worker(int id);

  /**
   * @brief Gets the first item in the queue, and removes it
   * @return task - The first item in the queue
   */
  task _pop_queue();

 public:
  /// you can't use this
  task_manager(task_manager &other) = delete;
  /// you can't use this
  void operator=(const task_manager &) = delete;

  /**
   * @brief Gets the instance of the task manager
   * @return task_manager* - The task manager instance
   */
  static task_manager *get_instance();

  /**
   * @brief Adds a task to the queue of tasks to be done
   * @param task task - The task to be done, will be last in the order
   */
  void add(task task);

  /**
   * @brief Starts fulfilling tasks
   */
  void start();
  /**
   * @brief Pauses fulfilling tasks
   */
  void pause();
  /**
   * @brief Stops the task manager and cleans it up
   */
  void stop();
  /**
   * @brief Checks if the the queue is empty
   * @return bool - If the queue is empty
   */
  bool is_done();

  /**
   * @brief Joins the main task manager thread, which will wait until all tasks are complete
   */
  void join();

  /**
   * @brief Returns a vector of task names
   * @return vector<string> - vector of task names
   */
  std::vector<std::string> tasks();

  /**
   * @brief Sets a setting
   * @param t tm_settings - The setting to set
   * @param val bool - The value of the setting
   */
  void set(tm_settings t, bool val) {
    GUARD(settings_lock_);
    if (val) SET_ON(settings_, t);
    else
      SET_OFF(settings_, ~t);
  }

  /**
   * @brief Gets the current value of a setting
   * @param mask tm_settings - The setting(s) to get
   * @return bool - The value of the setting (true or false)
   */
  bool get(tm_settings mask) {
    return EVAL(settings_, mask);
  }

  void clear_pool(const std::string &pool);
  std::unordered_map<std::string, std::unordered_map<int, std::string>> pools();
  std::unordered_map<int, std::string> pool(const std::string& name);

};

}

#endif //TASK_MANAGER__TASK_MANAGER_H_
