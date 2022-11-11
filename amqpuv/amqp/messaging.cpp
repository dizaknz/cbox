#include <uv.h>
#include <amqpcpp.h>
#include <amqpcpp/libuv.h>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>

#include "messaging.h"

#include "consumer.hpp"

class ConnHandler : public AMQP::LibUvHandler
{
    private:
        virtual void onError(AMQP::TcpConnection *connection, const char *message) override
        {
            std::cout << "error: " << message << std::endl;
        }
        virtual void onConnected(AMQP::TcpConnection *connection) override 
        {
            std::cout << "Connected" << std::endl;
            connected = true;
        }
        virtual void onReady(AMQP::TcpConnection *connection) override 
        {
            std::cout << "Connection ready" << std::endl;
        }
        virtual void onClosed(AMQP::TcpConnection *connection) override 
        {
            std::cout << "Connection closed" << std::endl;
        }

    public:
        ConnHandler(uv_loop_t *loop) : 
            AMQP::LibUvHandler(loop)
        {}

        virtual ~ConnHandler() = default;
    private:
        std::atomic<bool> connected = { false };
};

class Messaging::Impl
{
    public:
        void Run()
        {
            thread = std::thread([this]() { 
                loop = uv_default_loop();
                handler = std::make_unique<ConnHandler>(loop);
                connection = std::make_shared<AMQP::TcpConnection> (handler.get(), AMQP::Address(address));
                channel = std::make_unique<AMQP::TcpChannel>(connection.get());
                running = true;
                uv_run(loop, UV_RUN_DEFAULT);
            });
            auto timeoutTime = std::chrono::system_clock::now() + std::chrono::milliseconds(100);
            while (!running)
            {
                if (std::chrono::system_clock::now() > timeoutTime)
                {
                    std::cerr << "Timed out waiting for message loop to start" << std::endl;
                    // timed out
                    break;
                }
                std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            }
        }
        void Stop()
        {
            {
                // stop consumers
                std::lock_guard<std::mutex> lock(consumerMutex);
                for (auto consumer : consumers)
                {
                    consumer->Stop();
                    consumer.reset();
                }
                consumers.clear();
            }
            channel->close();
            connection->close();
            uv_loop_close(loop);
            running = false;
            if (thread.joinable())
            {
                thread.join();
            }
        }
        void CreateExchange(std::string exchange, bool durable = false)
        {
            if (!running)
            {
                return;
            }
            channel->declareExchange(exchange, AMQP::direct, durable ? AMQP::durable : AMQP::autodelete)
                .onError([](const char *message) {
                    std::cerr << "failed to create exchange, error: " << message << std::endl;
                })
                .onSuccess([exchange]() {
                    std::cout << "created exchange " << exchange << std::endl;
                });
        }
        void Subscribe(std::string exchange, std::string routingKey)
        {
            if (!running)
            {
                return;
            }
            std::lock_guard<std::mutex> lock(consumerMutex);
            auto consumer = std::make_shared<Consumer>(connection, exchange, routingKey);
            if (consumer->Initialise() && consumer->Start())
            {
                consumers.push_back(consumer);
            }
        }
        void Publish(std::string exchange, std::string routingKey, std::string message)
        {
            if (!running)
            {
                return;
            }
            channel->publish(exchange, routingKey, message);
        }
        Impl(std::string address) 
        : address(address)
        {}
        ~Impl();
    private:
        std::atomic_bool running = {false};
        std::thread thread;
        uv_loop_t *loop;
        std::shared_ptr<AMQP::TcpConnection> connection;
        std::unique_ptr<AMQP::TcpChannel> channel;
        std::unique_ptr<ConnHandler> handler;
        std::string address;
        std::mutex consumerMutex;
        std::vector<std::shared_ptr<Consumer>> consumers;
};

Messaging::Impl::~Impl() {}

Messaging::Messaging(std::string address)
{
    impl = std::make_unique<Messaging::Impl>(address);
}

Messaging::~Messaging() {}

void Messaging::Run()
{
    impl->Run();
}

void Messaging::Stop()
{
    impl->Stop();
}

void Messaging::CreateExchange(std::string exchange, bool durable)
{
    impl->CreateExchange(exchange, durable);
}

void Messaging::Subscribe(std::string exchange, std::string routingKey)
{
    impl->Subscribe(exchange, routingKey);
}

void Messaging::Publish(std::string exchange, std::string routingKey, std::string message)
{
    impl->Publish(exchange, routingKey, message);
}
 