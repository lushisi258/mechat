#pragma once
#include <memory>
#include <string>

class WssSession;

void test_msg_handler(std::shared_ptr<WssSession> session,
                      const std::string &data);