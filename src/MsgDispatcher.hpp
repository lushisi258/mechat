#pragma once
#include "message.pb.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

class WssSession;

class MsgDispatcher {
  public:
    MsgDispatcher();
    using msg_handler =
        std::function<void(std::shared_ptr<WssSession>, const std::string &)>;

    // Registers a handler for a specific message type
    void bind_handler(mechat::TYPE type, msg_handler handler);

    // Decodes and routes the payload to the corresponding handler
    void dispatch(std::shared_ptr<WssSession> session,
                  const std::string &raw_payload);

  private:
    // Maps message types to their respective functional handlers
    std::unordered_map<mechat::TYPE, msg_handler> handlers_;
};