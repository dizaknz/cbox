#include <amqpcpp.h>
#include <amqpcpp/libuv.h>
#include <thread>
#include <atomic>
#include <memory>

class Consumer
{
    public:
        Consumer(std::shared_ptr<AMQP::TcpConnection> connection,
            std::string exchange,
            std::string routingKey) 
            : connection(connection), exchange(exchange), routingKey(routingKey)
        {}
        virtual ~Consumer()
        {
            if (!consuming)
            {
                return;
            }
            Stop();
        }

        bool Initialise() 
        {
            channel = std::make_unique<AMQP::TcpChannel>(connection.get());
            channel->onError([this](const char *message) {
                // report error
                std::cerr << "channel error: " << message << std::endl;
            });
            // create a temporary queue
            channel->declareQueue(AMQP::exclusive).
                onSuccess([this](const std::string &name, uint32_t messagecount, uint32_t consumercount) 
                {
                    std::cout << "declared queue " << name << std::endl;

                    queue = name;
                });
            channel->bindQueue(exchange, queue, routingKey).
                onSuccess([this](){
                    std::cout << "bound queue " << queue << " to " << routingKey << std::endl;
                    initialised = true;
                });
            auto timeoutTime = std::chrono::system_clock::now() + std::chrono::milliseconds(100);
            while (!initialised)
            {
                if (std::chrono::system_clock::now() > timeoutTime)
                {
                    // timed out
                    break;
                }
                std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            }
            return initialised;
        }

        bool Start()
        {
            if (!initialised && !Initialise())
            {
                return false;
            }
            auto startCb = [this](const std::string &consumertag) {
                std::cout << "consume operation started" << std::endl;
                consuming = true;
            };
            auto errorCb = [](const char *message) {
                std::cerr << "consume operation failed" << std::endl;
            };
            auto messageCb = [this](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered) {
                std::cout << "message received" << std::endl;

                // TODO: call message handler
                channel->ack(deliveryTag);
            };

            std::string consumerTag = "consumer_" + queue;
            channel->consume(queue, consumerTag)
                .onReceived(messageCb)
                .onSuccess(startCb)
                .onError(errorCb);
            
            auto timeoutTime = std::chrono::system_clock::now() + std::chrono::milliseconds(100);
            while (!consuming)
            {
                if (std::chrono::system_clock::now() > timeoutTime)
                {
                    // timed out
                    break;
                }
                std::this_thread::sleep_for(std::chrono::nanoseconds(100));
            }
            return consuming;
        }

        void Stop()
        {
            channel->unbindQueue(exchange, queue, routingKey);
            channel->close();
        }

    private:
        std::shared_ptr<AMQP::TcpConnection> connection;
        std::atomic_bool initialised = {false};
        std::atomic_bool consuming = {false};
        std::unique_ptr<AMQP::TcpChannel> channel;
        
        std::string exchange;
        std::string routingKey;
        std::string queue;
};
