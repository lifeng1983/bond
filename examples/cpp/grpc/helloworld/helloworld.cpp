#include "helloworld_grpc.h"
#include "helloworld_types.h"

#include <bond/ext/grpc/io_manager.h>
#include <bond/ext/grpc/server.h>
#include <bond/ext/grpc/thread_pool.h>
#include <bond/ext/grpc/unary_call.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

using namespace helloworld;

// Logic and data behind the server's behavior.
class GreeterServiceImpl final : public Greeter::Service
{
public:
    using Greeter::Service::Service;

private:
    void SayHello(bond::ext::grpc::unary_call<HelloRequest, HelloReply> call) override
    {
        HelloRequest request = call.request().Deserialize();

        HelloReply reply;
        reply.message = "hello " + request.name;

        call.Finish(reply);
    }
};

int main()
{
    auto ioManager = std::make_shared<bond::ext::grpc::io_manager>();
    bond::ext::grpc::thread_pool threadPool;

    std::unique_ptr<GreeterServiceImpl> service{ new GreeterServiceImpl{ threadPool } };

    const std::string server_address("127.0.0.1:50051");

    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, ::grpc::InsecureServerCredentials());

    auto server = bond::ext::grpc::server::Start(builder, std::move(service));

    Greeter::Client greeter(
        ::grpc::CreateChannel(server_address, ::grpc::InsecureChannelCredentials()),
        ioManager,
        threadPool);

    const std::string user("world");

    HelloRequest request;
    request.name = user;

    auto result = greeter.AsyncSayHello(request);
    if (result.wait_for(std::chrono::seconds(10)) == std::future_status::timeout)
    {
        std::cout << "timeout ocurred";
        return 1;
    }

    HelloReply reply;
    try
    {
        result.get().response().Deserialize(reply);
    }
    catch (const bond::ext::grpc::UnaryCallException& e)
    {
        std::cout << "request failed: " << e.status().error_message();
        return 1;
    }

    if (reply.message.compare("hello world") != 0)
    {
        std::cout << "Wrong response: " << reply.message;
        return 1;
    }

    std::cout << "Correct response: " << reply.message;
    return 0;
}
