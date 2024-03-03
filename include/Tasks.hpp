#ifndef TASKS_HPP_
#define TASKS_HPP_

#include <chrono>
#include <functional>

/// @brief This namespace contains the classes that implement a cooperative task scheduler
namespace fablabbg::Tasks
{
  using milliseconds = std::chrono::milliseconds;
  using time_point_sc = std::chrono::time_point<std::chrono::system_clock>;
  using namespace std::chrono_literals;

  class Scheduler;

  // A task is a function that is executed periodically
  class Task
  {
  public:
    Task() = delete;

    /// @brief Creates a new task
    /// @param id task id, used for logging
    /// @param period period of the calls
    /// @param callback function to callback
    /// @param scheduler scheduler object to be added into
    /// @param active if false, scheduler will ignore the task until start()/restart() are called on the task
    /// @param delay initial delay before considering the task for execution
    Task(const std::string &id, milliseconds period, std::function<void()> callback, Scheduler &scheduler, bool active = true, milliseconds delay = 0ms);
    ~Task() = default;

    Task(const Task &other) = default;
    Task(Task &&other) = default;
    Task &operator=(const Task &other) = default;
    Task &operator=(Task &&other) = default;

    /// @brief Execute the task if active
    auto run() -> void;

    /// @brief  Prevent the task from running again
    auto stop() -> void;

    /// @brief Allows the task to run again
    auto start() -> void;

    /// @brief recompute the next run time (now + delay) and allows the task to run again
    auto restart() -> void;

    /// @brief Change the task period
    /// @param new_period new period. Use 0ms for single shot.
    auto setPeriod(milliseconds new_period) -> void;

    /// @brief Change the task initial delay
    /// @param new_delay Initial delay. Use 0s to avoid any initial delay.
    auto setDelay(milliseconds new_delay) -> void;

    /// @brief Change the callback function
    /// @param new_callback function to be called back
    auto setCallback(std::function<void()> new_callback) -> void;

    /// @brief Status of the task
    /// @return True if Scheduler can launch it
    auto isActive() const -> bool;

    /// @brief Current period of the task
    [[nodiscard]] auto getPeriod() const -> milliseconds;

    [[nodiscard]] auto getCallback() const -> std::function<void()>;

    /// @brief Get the Task Identifier
    [[nodiscard]] auto getId() const -> const std::string;

    [[nodiscard]] auto getDelay() const -> milliseconds;

    /// @brief Get the average tardiness, i.e. the average period between scheduled start and actual start of execution.
    [[nodiscard]] auto getAvgTardiness() const -> milliseconds;

    /// @brief Gets the number of times the task has been run.
    [[nodiscard]] auto getRunCounter() const -> unsigned long;

    /// @brief Gets the total execution time of the task. Useful to spot slowest tasks
    [[nodiscard]] auto getTotalRuntime() const -> milliseconds;

    /// @brief When shall the task be run again
    /// @return time_point of the next run or time_point::max() if the task will not run.
    [[nodiscard]] auto getNextRun() const -> time_point_sc;

  private:
    bool active;
    const std::string id;
    milliseconds period;
    milliseconds delay;
    time_point_sc last_run;
    time_point_sc next_run;
    milliseconds average_tardiness;
    milliseconds total_runtime;
    std::function<void()> callback;
    unsigned long run_counter;
  };

  class Scheduler
  {
  public:
    Scheduler() : tasks{} {};
    ~Scheduler() = default;

    auto addTask(Task &task) -> void;
    auto removeTask(Task &task) -> void;

    /// @brief Execute all tasks that are ready to run
    /// @details Tasks will be ordered by next_run time ascending, then run sequentially
    auto execute() const -> void;

    /// @brief Recompute all the next run times for all the tasks
    auto restart() const -> void;

    /// @brief Gets the number of tasks in the scheduler
    auto taskCount() const -> size_t;

    /// @brief Get a vector of references to the tasks
    auto getTasks() const -> const std::vector<std::reference_wrapper<Task>>;

  private:
    std::vector<std::reference_wrapper<Task>> tasks; // Vector containing references to the tasks, not the tasks themselves

    auto printStats() const -> void;
  };

  void task_delay(const milliseconds delay);

} // namespace fablabbg::Tasks
#endif // TASKS_HPP_