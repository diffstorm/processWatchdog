#include "gtest/gtest.h"

extern "C" {
    #include "server.h"
}

TEST(ServerTest, UdpStart) {
    int socket;
    int port = 12345;
    ASSERT_EQ(udp_start(&socket, port), 0);
    udp_stop(socket);
}
