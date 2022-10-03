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

        void Initialise() 
        {
            channel = std::make_unique<AMQP::TcpChannel>(connection.get());
            channel->onError([this](const char *message) {
                // report error
                std::cout << "channel error: " << message << std::endl;
                Stop();
            });
            channel->onReady([this]() {
                // create a temporary queue
                channel->declareQueue(AMQP::exclusive).
                    onSuccess([this](const std::string &name, uint32_t messagecount, uint32_t consumercount) 
                    {
                        std::cout << "declared queue " << name << std::endl;

                        queue = name;
                        channel->bindQueue(exchange, queue, routingKey);
                    });
            });
        }

        void Start()
        {
            auto startCb = [](const std::string &consumertag) {
                std::cout << "consume operation started" << std::endl;
            };
            auto errorCb = [](const char *message) {
                std::cout << "consume operation failed" << std::endl;
            };
            auto messageCb = [this](const AMQP::Message &message, uint64_t deliveryTag, bool redelivered) {
                std::cout << "message received" << std::endl;

                // TODO: call message handler
                channel->ack(deliveryTag);
            };

            channel->consume(queue)
                .onReceived(messageCb)
                .onSuccess(startCb)
                .onError(errorCb);
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
