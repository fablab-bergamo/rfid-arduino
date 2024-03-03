#include "Tasks.hpp"
#include "Arduino.h"
#include "conf.hpp"

#include "Arduino.h"
#include "ArduinoOTA.h"
#include <algorithm>

#include "Logging.hpp"

namespace fablabbg::Tasks
{
  using milliseconds = std::chrono::milliseconds;
  using time_point_sc = std::chrono::time_point<std::chrono::system_clock>;
  using namespace std::chrono_literals;

  auto Scheduler::addTask(Task &task) -> void
  {
    tasks.push_back(task);
  }

  auto Scheduler::removeTask(Task &task) -> void
  {
    tasks.erase(std::remove_if(tasks.begin(), tasks.end(),
                               [&task](const auto &t)
                               { return t.get().getId() == task.getId(); }),
                tasks.end());
  }

  auto Scheduler::restart() const -> void
  {
    for (const auto &task : tasks)
    {
      task.get().restart();
    }
  }

  auto Scheduler::printStats() const -> void
  {
    milliseconds avg_delay = 0ms;
    auto nb_runs = 0;

    for (const auto &task : tasks)
    {
      avg_delay += task.get().getAvgTardiness() * task.get().getRunCounter();
      nb_runs += task.get().getRunCounter();
    }
    if (nb_runs > 0)
    {
      avg_delay /= nb_runs;
    }

    ESP_LOGD(TAG, "Scheduler::execute complete: %d tasks total, %lu runs, avg delay/run: %llu ms\r\n", tasks.size(), nb_runs, avg_delay.count());

    for (const auto &task : tasks)
    {
      if (task.get().isActive())
      {
        if (task.get().getRunCounter() > 0)
        {
          ESP_LOGD(TAG, "\t Task: %s, %lu runs, avg tardiness/run: %llu ms, period %llu ms, delay %llu ms, average task duration %llu ms\r\n",
                   task.get().getId().c_str(), task.get().getRunCounter(),
                   task.get().getAvgTardiness().count(), task.get().getPeriod().count(),
                   task.get().getDelay().count(),
                   task.get().getTotalRuntime().count() / task.get().getRunCounter());
        }
        else
        {
          ESP_LOGD(TAG, "\t Task: %s, never ran, period %llu ms, delay %llu ms\r\n",
                   task.get().getId().c_str(), task.get().getPeriod().count(),
                   task.get().getDelay().count());
        }
      }
      else
      {
        ESP_LOGD(TAG, "\t Task: %s, inactive\r\n", task.get().getId().c_str());
      }
    }
  }

  auto Scheduler::execute() const -> void
  {
    // Tasks shall be run in order of expiration (the most expired task shall run first)
    std::vector<decltype(tasks)::const_iterator> iters{};
    for (auto it = tasks.begin(); it != tasks.end(); ++it)
    {
      iters.push_back(it); // Vector of iterators
    }

    // Sort the iterators array as we cannot sort directly reference_wrappers
    std::sort(iters.begin(), iters.end(),
              [](const auto &it1, const auto &it2)
              {
                return (it1->get().getNextRun() < it2->get().getNextRun());
              });

    // Now iterate over the sorted iterators to run the tasks
    for (const auto &it : iters)
    {
      it->get().run();
    }

    if (conf::debug::ENABLE_TASK_LOGS && millis() % 1024 == 0)
    {
      printStats();
    }
#if (PINS_WOKWI)
    else
    {
      delay(5); // Wokwi simulation is sometimes slow and this helps to catch-up
    }
#endif
  }

  auto Scheduler::taskCount() const -> size_t
  {
    return tasks.size();
  }

  auto Scheduler::getTasks() const -> const std::vector<std::reference_wrapper<Task>>
  {
    return {tasks};
  }

  /// @brief Creates a new task
  /// @param id task id
  /// @param period task period (if 0, will be run only once)
  /// @param callback callback function to execute
  /// @param scheduler reference to the scheduler
  /// @param active if true, the task will be executed
  /// @param delay initial delay before starting the task
  Task::Task(const std::string &id, milliseconds period,
             std::function<void()> callback,
             Scheduler &scheduler, bool active, milliseconds delay) : active{active}, id{id},
                                                                      period{period}, delay{delay},
                                                                      last_run{std::chrono::system_clock::now() + delay},
                                                                      next_run{last_run},
                                                                      average_tardiness{0ms}, total_runtime{0ms},
                                                                      callback{callback}, run_counter{0}
  {
    scheduler.addTask(std::ref(*this));
  }

  auto Task::run() -> void
  {
    if (isActive() && std::chrono::system_clock::now() >= next_run)
    {
      run_counter++;
      auto last_period = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - last_run);
      average_tardiness = (average_tardiness * (run_counter - 1) + last_period) / run_counter;
      last_run = std::chrono::system_clock::now();

      if (conf::debug::ENABLE_TASK_LOGS)
      {
        ESP_LOGD(TAG, "Task %s\r\n", getId().c_str());
      }

      callback();

      total_runtime += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - last_run);

      if (period > 0ms)
      {
        next_run = last_run + period; // Schedule next run
      }
      else
      {
        next_run = std::chrono::system_clock::time_point::max(); // Disable the task
      }
    }
  }

  auto Task::stop() -> void
  {
    active = false;
  }

  auto Task::start() -> void
  {
    active = true;
    restart();
  }

  /// @brief recompute the next run time (now + delay)
  auto Task::restart() -> void
  {
    last_run = std::chrono::system_clock::now() + delay;
    next_run = last_run;
  }

  auto Task::setPeriod(milliseconds new_period) -> void
  {
    period = new_period;
  }

  auto Task::setCallback(std::function<void()> new_callback) -> void
  {
    callback = new_callback;
  }

  auto Task::isActive() const -> bool
  {
    return active;
  }

  auto Task::getPeriod() const -> std::chrono::milliseconds
  {
    return period;
  }

  auto Task::getCallback() const -> std::function<void()>
  {
    return callback;
  }

  auto Task::getId() const -> const std::string
  {
    return id;
  }

  auto Task::getAvgTardiness() const -> std::chrono::milliseconds
  {
    if (average_tardiness > period)
    {
      return average_tardiness - period;
    }
    return 0ms;
  }

  auto Task::getRunCounter() const -> unsigned long
  {
    return run_counter;
  }

  auto Task::getDelay() const -> std::chrono::milliseconds
  {
    return delay;
  }

  auto Task::setDelay(std::chrono::milliseconds new_delay) -> void
  {
    delay = new_delay;
  }

  auto Task::getTotalRuntime() const -> std::chrono::milliseconds
  {
    return total_runtime;
  }

  auto Task::getNextRun() const -> std::chrono::time_point<std::chrono::system_clock>
  {
    return next_run;
  }

  /// @brief Wait for a delay, allowing OTA updates
  /// @param delay period to wait (should be > 50 ms)
  auto task_delay(const std::chrono::milliseconds duration) -> void
  {
    if (duration < 50ms)
    {
      delay(duration.count());
      return;
    }

    const auto start = std::chrono::system_clock::now();
    do
    {
      delay(50);
      ArduinoOTA.handle();
    } while (std::chrono::system_clock::now() - start < duration);
  }
} // namespace fablabbg::Tasks