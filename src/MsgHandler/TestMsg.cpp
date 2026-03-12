#include "TestMsg.hpp"
#include "message.pb.h"
#include <iostream>

void test_msg_handler(std::shared_ptr<WssSession> session,
                      const std::string &data) {
    mechat::TestMessage t_msg;
    if (t_msg.ParseFromString(data)) {
        std::cout << "TestMsg: " << t_msg.content() << std::endl;
    }
}