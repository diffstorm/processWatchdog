#include "gtest/gtest.h"

extern "C" {
#include "server.h"
#include "cmd.h"
#include "process.h"
#include "utils.h"
#include "status.h"
}

// UDP server can bind and unbind a port.
TEST(ServerTest, UdpStart)
{
    int socket;
    int port = 12345;
    ASSERT_EQ(udp_start(&socket, port), 0);
    udp_stop(socket);
}

// Heartbeat parsing: liveness vs progress, and pid extraction.
TEST(CmdTest, HeartbeatLiveness)
{
    net_command_t c = cmd_parse_network("p12345", 6);
    EXPECT_EQ(c.type, NET_CMD_HEARTBEAT);
    EXPECT_EQ(c.pid, 12345);
    EXPECT_FALSE(c.has_progress_flag);
}

TEST(CmdTest, HeartbeatProgress)
{
    net_command_t c = cmd_parse_network("p12345+", 7);
    EXPECT_EQ(c.type, NET_CMD_HEARTBEAT);
    EXPECT_EQ(c.pid, 12345);
    EXPECT_TRUE(c.has_progress_flag);
}

// Malformed datagrams on the untrusted UDP boundary must be rejected, not parsed.
TEST(CmdTest, MalformedRejected)
{
    char nodigit[8] = { 'p', 'x', 'y', 'z', 'q', 'r', 's', 't' }; // not NUL-terminated
    EXPECT_EQ(cmd_parse_network(nodigit, (int)sizeof(nodigit)).type, NET_CMD_UNKNOWN);

    char high[4] = { 'p', (char)0x80, (char)0xFF, 'z' };
    EXPECT_EQ(cmd_parse_network(high, (int)sizeof(high)).type, NET_CMD_UNKNOWN);
}

// Control commands map to the right types and carry the app name.
TEST(CmdTest, ControlCommands)
{
    EXPECT_EQ(cmd_parse_network("aBot", 4).type, NET_CMD_START);
    EXPECT_EQ(cmd_parse_network("oBot", 4).type, NET_CMD_STOP);
    EXPECT_EQ(cmd_parse_network("rBot", 4).type, NET_CMD_RESTART);

    net_command_t s = cmd_parse_network("sBot", 4);
    EXPECT_EQ(s.type, NET_CMD_STATUS);
    EXPECT_STREQ(s.app_name, "Bot");
}

// parse_number: bounded, sign-safe, correct consumed count.
TEST(UtilsTest, ParseNumber)
{
    int cnt = 0;
    EXPECT_EQ(parse_number("12345", 5, &cnt), 12345);
    EXPECT_EQ(cnt, 5);

    EXPECT_EQ(parse_number("42+", 3, &cnt), 42); // stops at '+'
    EXPECT_EQ(cnt, 2);

    EXPECT_EQ(parse_number("xyz", 3, &cnt), 0);  // no digit
    EXPECT_EQ(parse_number("999", 1, &cnt), 9);  // length-bounded
}

// Exponential backoff: correct for small inputs, capped and UB-safe for large.
TEST(ProcessTest, BackoffDelay)
{
    EXPECT_EQ(calculate_backoff_delay(1, 1), 1);
    EXPECT_EQ(calculate_backoff_delay(2, 1), 2);
    EXPECT_EQ(calculate_backoff_delay(3, 2), 8);
    EXPECT_EQ(calculate_backoff_delay(5, 1), 16);
    EXPECT_EQ(calculate_backoff_delay(40, 1), 3600);     // huge shift -> capped
    EXPECT_EQ(calculate_backoff_delay(1, 100000), 3600); // huge base -> capped
    EXPECT_EQ(calculate_backoff_delay(0, 1), 1);         // shift clamped to 0
}

// Status JSON contract: the client's field reader must round-trip every field the server
// emits, and report a missing key. This is the wire contract between server and client.
TEST(StatusTest, ExtractFieldRoundTrip)
{
    const char *json =
        "{\"app\":\"Bot\",\"status\":\"running\",\"circuit\":\"closed\","
        "\"cooldown\":45,\"failures\":2,\"ack_required\":true}";
    char v[32];

    EXPECT_EQ(status_extract_field(json, "app", v, sizeof(v)), 0);
    EXPECT_STREQ(v, "Bot");
    EXPECT_EQ(status_extract_field(json, "status", v, sizeof(v)), 0);
    EXPECT_STREQ(v, "running");
    EXPECT_EQ(status_extract_field(json, "cooldown", v, sizeof(v)), 0);
    EXPECT_STREQ(v, "45");
    EXPECT_EQ(status_extract_field(json, "ack_required", v, sizeof(v)), 0);
    EXPECT_STREQ(v, "true");

    EXPECT_EQ(status_extract_field(json, "missing", v, sizeof(v)), -1);
}
