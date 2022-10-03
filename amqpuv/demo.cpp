#include <iostream>

#include "consumer.hpp"
#include "messaging.hpp"

int main()
{
    Messaging messaging("amqp://guest:guest@localhost/", "demo");
    std::cout << "Running messaging loop" << std::endl;
    messaging.Run();
    // TODO:
    // messaging->Subscribe()
    // messaging->Publish()
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "Stopping messaging" << std::endl;
    messaging.Stop();

    return 0;
}

