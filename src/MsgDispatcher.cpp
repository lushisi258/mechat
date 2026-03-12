#include "MsgDispatcher.hpp"
#include "MsgHandler/TestMsg.hpp"
#include <iostream>

MsgDispatcher::MsgDispatcher() {
    // Binding types with functions during construction
    bind_handler(mechat::TEST, test_msg_handler);
}

void MsgDispatcher::bind_handler(mechat::TYPE type, msg_handler handler) {
    // Registers a handler for a specific message type
    handlers_[type] = std::move(handler);
}

void MsgDispatcher::dispatch(std::shared_ptr<WssSession> session,
                             const std::string &raw_payload) {
    std::cout << "msg dispatcher 1" << std::endl;
    mechat::Message wrapper;
    // Deserialize the raw payload into a message wrapper
    if (!wrapper.ParseFromString(raw_payload)) {
        // Handle parsing failure
        std::cout << "wrong format" << std::endl;
        return;
    }

    // Locate the registered handler for the specific message type
    auto it = handlers_.find(wrapper.type_id());
    if (it != handlers_.end()) {
        // Invoke the handler with the session context and extracted message
        // data
        std::cout << "msg dispatcher 2" << std::endl;
        it->second(session, wrapper.data());
    }
}