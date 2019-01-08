// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Include auto-generated files
#include "pingpong_apply.h"
#include "pingpong_grpc.h"
#include "pingpong_reflection.h"
#include "pingpong_types.h"

#include <bond/ext/grpc/server.h>
#include <bond/ext/grpc/thread_pool.h>
#include <bond/ext/grpc/unary_call.h>

#include "../../grpc/countdown_event.h"

#include <chrono>
#include <memory>
#include <stdio.h>
#include <string>
#include <thread>

using namespace PingPongNS;

static unit_test::countdown_event Countdown(NumRequests + NumEvents + NumErrors);
static std::atomic<uint32_t> NumRequestsReceived(0);
static std::atomic<uint32_t> NumEventsReceived(0);
static std::atomic<uint32_t> NumErrorsReceived(0);

// Logic and data behind the server's behavior.
class PingPongServiceImpl final : public PingPong::Service
{
public:
    using PingPong::Service::Service;

    void Ping(bond::ext::grpc::unary_call<PingRequest, PingResponse> call) override
    {
        PingRequest request = call.request().Deserialize();

        switch (request.Action)
        {
            case PingAction::Identity:
            {
                printf("Received identity request \"%s\"\n", request.Payload.c_str());
                fflush(stdout);

                PingResponse response;
                response.Payload = request.Payload;

                NumRequestsReceived++;

                call.Finish(response);
                Countdown.set();
                break;
            }

            case PingAction::Error:
            {
                printf("Received error request \"%s\"\n", request.Payload.c_str());
                fflush(stdout);

                NumErrorsReceived++;

                call.Finish({ ::grpc::StatusCode::UNIMPLEMENTED, "Application Exception" });
                Countdown.set();
                break;
            }

            default:
            {
                printf("Received unknown request \"%s\"\n", request.Payload.c_str());
                fflush(stdout);

                call.Finish({ ::grpc::StatusCode::UNIMPLEMENTED, "Unknown PingAction" });
                Countdown.set();
                break;
            }
        }
    }

    void PingEvent(bond::ext::grpc::unary_call<PingRequest, bond::reflection::nothing> call) override
    {
        PingRequest request = call.request().Deserialize();

        printf("Received event \"%s\"\n", request.Payload.c_str());
        fflush(stdout);

        NumEventsReceived++;

        Countdown.set();
    }

};

int main()
{
    bond::ext::grpc::thread_pool threadPool;

    std::unique_ptr<PingPongServiceImpl> service{ new PingPongServiceImpl{ threadPool } };

    const std::string server_address("127.0.0.1:" + std::to_string(Port));

    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, ::grpc::InsecureServerCredentials());

    auto server = bond::ext::grpc::server::Start(builder, std::move(service));

    printf("Server ready\n");
    fflush(stdout);

    bool countdownSet = Countdown.wait_for(std::chrono::seconds(30));

    server.Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(10));
    server.Wait();

    if (!countdownSet ||
        (NumRequestsReceived != NumRequests) ||
        (NumEventsReceived != NumEvents) ||
        (NumErrorsReceived != NumErrors))
    {
        printf("Server failed: Did not receive all expected messages\n");
        fflush(stdout);
        exit(1);
    }

    printf("Server completed\n");
    fflush(stdout);
    return 0;
}
