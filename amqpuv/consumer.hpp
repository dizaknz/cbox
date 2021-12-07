#include <uv.h>
#include <amqpcpp.h>
#include <amqpcpp/libuv.h>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>

class AMQPHandler : public AMQP::LibUvHandler
{
    private:
        virtual void onError(AMQP::TcpConnection *connection, const char *message) override
        {
            std::cout << "error: " << message << std::endl;
            if (!connected)
            {
                return;
            }
            channel->close().onSuccess([&connection, this]() {
                std::cout << "channel closed" << std::endl;
                // close the connection
                connection->close();
            });
        }
        virtual void onConnected(AMQP::TcpConnection *connection) override 
        {
            std::cout << "Connected" << std::endl;
            connected = true;
        }
        virtual void onReady(AMQP::TcpConnection *connection) override 
        {
            std::cout << "Connection ready" << std::endl;
            channel = std::make_shared<AMQP::TcpChannel>(connection);
            channel->onError([](const char *message) {
                // report error
                std::cout << "channel error: " << message << std::endl;
            });
            channel->onReady([this]() {
                // create a temporary queue
                channel->declareQueue(AMQP::exclusive).
                    onSuccess([this](const std::string &name, uint32_t messagecount, uint32_t consumercount) 
                    {
                        std::cout << "declared queue " << name << std::endl;
                    });
                // consume
            });
        }
        virtual void onClosed(AMQP::TcpConnection *connection) override 
        {
            std::cout << "Connection closed" << std::endl;
        }

        // TODO: functions to create channel, queue etc as callbacks
        // support durable vs temporary queues etc
        void declareTempQueue(std::shared_ptr<AMQP::Channel> channel) {

        }
        void declareDurableQueue(std::shared_ptr<AMQP::Channel> channel) {}

    public:
        AMQPHandler(uv_loop_t *loop) : 
            AMQP::LibUvHandler(loop)
        {}

        virtual ~AMQPHandler() = default;
    private:
        std::shared_ptr<AMQP::TcpChannel> channel;
        std::string exchange;
        std::string key;
        std::string queue;
        std::atomic<bool> connected = { false };
};

class Consumer
{
    public:
        void Run() {
            thread = std::thread([this]() { 
                loop = uv_default_loop();
                AMQPHandler handler(loop);
                connection = std::make_shared<AMQP::TcpConnection> (&handler, AMQP::Address(address));
                uv_run(loop, UV_RUN_DEFAULT);
            });
        }
        void Stop() {
            connection->close();
            uv_loop_close(loop);
            running = false;
            if (thread.joinable())
            {
                thread.join();
            }
        }
        Consumer(std::string address) : address(address)
        {}
    private:
        std::atomic_bool running = {false};
        std::thread thread;
        uv_loop_t *loop;
        std::shared_ptr<AMQP::TcpConnection> connection;
        std::string address;
};


// TODO:
// Extend consumer and add callbacks
// - set transient or durable queues + properties - just pass in AMQP::Table
// DurableQueueConsumer
// TransientQueueConsumer

/*

// callback function that is called when the consume operation starts
auto startCb = [](const std::string &consumertag) {

    std::cout << "consume operation started" << std::endl;
};

// callback function that is called when the consume operation failed
auto errorCb = [](const char *message) {

    std::cout << "consume operation failed" << std::endl;
};

// callback operation when a message was received
auto messageCb = [&channel](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered) {

    std::cout << "message received" << std::endl;

    // acknowledge the message
    channel.ack(deliveryTag);
};

// start consuming from the queue, and install the callbacks
channel.consume("my-queue")
    .onReceived(messageCb)
    .onSuccess(startCb)
    .onError(errorCb);

*/
