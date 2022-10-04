#include <iostream>

#include "consumer.hpp"
#include "messaging.hpp"

int main()
{
    Messaging messaging("amqp://guest:guest@localhost/");
    std::cout << "Running messaging loop" << std::endl;
    messaging.Run();
    std::string exchange = "demo";
    messaging.CreateExchange(exchange, false);
    messaging.Subscribe(exchange, "sub1");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "Publishing messages" << std::endl;
    messaging.Publish(exchange, "sub1", "test1");
    messaging.Publish(exchange, "sub1", "test2");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "Stopping messaging" << std::endl;
    messaging.Stop();

    return 0;
}

