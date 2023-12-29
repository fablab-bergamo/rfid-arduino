#include <chrono>
#include <string>
#include <functional>

#include <Arduino.h>
#define UNITY_INCLUDE_PRINT_FORMATTED
#include <unity.h>
#include "Tasks.hpp"

using namespace fablabbg::Tasks;
using namespace std::chrono;
using namespace std::chrono_literals;

// Static variables for testing
constexpr int NB_TASKS = 100;
bool tasks_status[NB_TASKS]{false};
Task *tasks[NB_TASKS]{nullptr};
size_t task_counter{0};
Scheduler scheduler;
auto execute = []()
{ scheduler.execute(); };

void delete_tasks(void)
{
  for (auto &t : tasks)
  {
    if (t != nullptr)
    {
      scheduler.removeTask(*t); // Because Task adds itself on creation
      delete t;
      t = nullptr;
    }
  }
}

void tearDown(void)
{
  delete_tasks();
}

void create_tasks(Scheduler &scheduler, milliseconds period)
{
  // Clean-up
  task_counter = 0;
  for (auto &t : tasks_status)
  {
    t = false;
  }
  delete_tasks();

  // Creation
  for (size_t i = 0; i < NB_TASKS; ++i)
  {
    auto callback = [i]()
    {
      tasks_status[i] = true;
      task_counter++;
    };
    tasks[i] = new Task("T" + std::to_string(i), period, callback, scheduler, true);
    auto task = *tasks[i];
    TEST_ASSERT_MESSAGE(task.isActive(), "Task is not active");
    TEST_ASSERT_EQUAL_MESSAGE(period.count(), task.getPeriod().count(), "Task period is not 150ms");
  }
}

void run_for_duration(std::function<void()> callback, milliseconds duration)
{
  auto start = high_resolution_clock::now();
  while (high_resolution_clock::now() - start < duration)
  {
    callback();
  }
}

void setUp(void)
{
  // set stuff up here
}

void test_execute_runs_all_tasks(void)
{
  create_tasks(scheduler, 150ms);
  TEST_ASSERT_EQUAL_MESSAGE(NB_TASKS, scheduler.taskCount(), "Scheduler does not contain all tasks");

  run_for_duration(execute, 100ms);
  TEST_ASSERT_EQUAL_MESSAGE(NB_TASKS, task_counter, "All tasks were not called once in 100 ms");

  delay(50);

  task_counter = 0;
  run_for_duration(execute, 100ms);
  TEST_ASSERT_EQUAL_MESSAGE(NB_TASKS, task_counter, "All tasks were not called once again after 150 ms");
}

void test_stop_start_tasks(void)
{
  create_tasks(scheduler, 150ms);
  TEST_ASSERT_EQUAL_MESSAGE(NB_TASKS, scheduler.taskCount(), "Scheduler does not contain all tasks");

  for (auto t : tasks)
  {
    t->stop();
  }

  task_counter = 0;
  run_for_duration(execute, 150ms);
  TEST_ASSERT_EQUAL_MESSAGE(0, task_counter, "Stopped tasks has been executed");

  for (auto t : tasks)
  {
    t->start();
  }

  task_counter = 0;
  run_for_duration(execute, 150ms);
  TEST_ASSERT_EQUAL_MESSAGE(NB_TASKS, task_counter, "Started tasks have not all been executed");

  task_counter = 0;
  for (auto t : tasks)
  {
    t->restart();
  }
  execute();
  TEST_ASSERT_EQUAL_MESSAGE(NB_TASKS, task_counter, "Restarted tasks did not run immediately");
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_execute_runs_all_tasks);
  RUN_TEST(test_stop_start_tasks);
  UNITY_END(); // stop unit testing
}

void loop()
{
}