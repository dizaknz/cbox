#include <uv.h>
#include <amqpcpp.h>
#include <amqpcpp/libuv.h>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>

class ConnHandler : public AMQP::LibUvHandler
{
    private:
        virtual void onError(AMQP::TcpConnection *connection, const char *message) override
        {
            std::cout << "error: " << message << std::endl;
            if (!connected)
            {
                return;
            }
            // close the connection
            connection->close();
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

class Messaging
{
    public:
        void Run() {
            thread = std::thread([this]() { 
                loop = uv_default_loop();
                ConnHandler handler(loop);
                connection = std::make_shared<AMQP::TcpConnection> (&handler, AMQP::Address(address));

                
                uv_run(loop, UV_RUN_DEFAULT);
            });
        }
        void Stop() {
            // TODO: stop consumers

            connection->close();
            uv_loop_close(loop);
            running = false;
            if (thread.joinable())
            {
                thread.join();
            }
        }
        Messaging(std::string address, std::string exchange) 
        : address(address), exchange(exchange)
        {}
    private:
        std::atomic_bool running = {false};
        std::thread thread;
        uv_loop_t *loop;
        std::shared_ptr<AMQP::TcpConnection> connection;
        std::string address;
        std::string exchange;
};
