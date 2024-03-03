#ifndef MOCK_MOCKMQTTBROKER_HPP_
#define MOCK_MOCKMQTTBROKER_HPP_

#include <atomic>
#include <functional>
#include <queue>
#include <string>

#include "BaseRfidWrapper.hpp"
#include "sMQTTBroker.h"

namespace fablabbg
{
  class MockMQTTBroker final : public sMQTTBroker
  {
  public:
    MockMQTTBroker() : isLocked{false} {};
    ~MockMQTTBroker() = default;

    auto isRunning() const -> bool;
    auto start() -> void;
    auto onEvent(sMQTTEvent *event) -> bool override;
    auto defaultReplies(const std::string &query) const -> const std::string;

    /// @brief set the reply generation function. May be called from a different thread
    /// @param callback
    auto configureReplies(std::function<const std::string(const std::string &, const std::string &)> callback) -> void;

    auto processQueries() -> size_t;

    auto mainLoop() -> void;

  private:
    std::atomic<bool> is_running{false};
    std::string topic = "";
    std::string payload = "";
    struct query
    {
      std::string source_topic{""};
      std::string source_query{""};
      std::string reply_topic{""};
    };
    std::queue<query> queries{};

    std::function<const std::string(const std::string &, const std::string &)> callback = [this](const std::string &topic, const std::string &query)
    { return defaultReplies(query); };

    std::atomic<bool> isLocked;

    auto lock() -> bool { return !isLocked.exchange(true); }
    auto unlock() -> void { isLocked = false; }
  };
} // namespace fablabbg
#endif // MOCK_MOCKMQTTBROKER_HPP_