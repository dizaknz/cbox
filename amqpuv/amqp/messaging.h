#pragma once

#include <string>
#include <memory>

class Messaging
{
    public:
        Messaging(std::string address);
        Messaging() = delete;
        Messaging(const Messaging&) = delete;
        ~Messaging();

        void Run();
        void Stop();
        void CreateExchange(std::string exchange, bool durable = false);
        void Subscribe(std::string exchange, std::string routingKey);
        void Publish(std::string exchange, std::string routingKey, std::string message);
    private:
        class Impl; std::unique_ptr<Impl> impl;
};
