#include <uv.h>
#include <amqpcpp.h>
#include <amqpcpp/libuv.h>
#include <thread>
#include <chrono>
#include <iostream>

#include "consumer.hpp"


int main()
{
    Consumer consumer("amqp://guest:guest@localhost/");
    std::cout << "Running consumer" << std::endl;
    consumer.Run();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "Stopping consumer" << std::endl;
    consumer.Stop();

    return 0;
}

