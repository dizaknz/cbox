#include <uv.h>
#include <amqpcpp.h>
#include <amqpcpp/libuv.h>

class MessageHandler : public AMQP::LibUvHandler
{
private:
    virtual void onError(AMQP::TcpConnection *connection, const char *message) override
    {
        std::cout << "error: " << message << std::endl;
    }

    virtual void onConnected(AMQP::TcpConnection *connection) override 
    {
        std::cout << "connected" << std::endl;
    }
    
public:
    MessageHandler(uv_loop_t *loop) : AMQP::LibUvHandler(loop) {}

    virtual ~MessageHandler() = default;
};

int main()
{
    auto *loop = uv_default_loop();
    MessageHandler handler(loop);
    AMQP::TcpConnection connection(&handler, AMQP::Address("amqp://guest:guest@localhost/"));
    AMQP::TcpChannel channel(&connection);
    // create a temporary queue
    channel.declareQueue(AMQP::exclusive).onSuccess([&connection](const std::string &name, uint32_t messagecount, uint32_t consumercount) {
        
        std::cout << "declared queue " << name << std::endl;
    });
    
    // run the loop
    uv_run(loop, UV_RUN_DEFAULT);

    // done
    return 0;
}

