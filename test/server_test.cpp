#include "gtest/gtest.h"

extern "C" {
#include "cmd.h"
#include "process.h"
#include "status.h"
#include "utils.h"
}

// Unit / contract tests: pure functions and wire formats only. Anything that needs the
// live app array, config files or the daemon loop is an integration test (-t), not here.

// --- Heartbeat wire protocol (cmd_parse_network) ---

TEST(CmdTest, HeartbeatLivenessVsProgress)
{
    net_command_t live = cmd_parse_network("p12345", 6);
    EXPECT_EQ(live.type, NET_CMD_HEARTBEAT);
    EXPECT_EQ(live.pid, 12345);
    EXPECT_FALSE(live.has_progress_flag);

    net_command_t prog = cmd_parse_network("p12345+", 7);
    EXPECT_EQ(prog.type, NET_CMD_HEARTBEAT);
    EXPECT_EQ(prog.pid, 12345);
    EXPECT_TRUE(prog.has_progress_flag);
}

TEST(CmdTest, HeartbeatPidBounds)
{
    // Valid pid range is 0 < pid < INT32_MAX (strict). Everything else is rejected.
    EXPECT_EQ(cmd_parse_network("p0", 2).type, NET_CMD_UNKNOWN);
    EXPECT_EQ(cmd_parse_network("p-1", 3).type, NET_CMD_UNKNOWN);
    EXPECT_EQ(cmd_parse_network("p2147483647", 11).type, NET_CMD_UNKNOWN); // INT32_MAX excluded
    EXPECT_EQ(cmd_parse_network("p1", 2).type, NET_CMD_HEARTBEAT);
}

TEST(CmdTest, MalformedRejected)
{
    // Untrusted UDP: a 'p' with no digit / high-bit bytes must reject, never over-read.
    char nodigit[8] = { 'p', 'x', 'y', 'z', 'q', 'r', 's', 't' }; // not NUL-terminated
    EXPECT_EQ(cmd_parse_network(nodigit, (int)sizeof(nodigit)).type, NET_CMD_UNKNOWN);

    char high[4] = { 'p', (char)0x80, (char)0xFF, 'z' };
    EXPECT_EQ(cmd_parse_network(high, (int)sizeof(high)).type, NET_CMD_UNKNOWN);
}

TEST(CmdTest, ControlCommands)
{
    // a/o/r/s/k map to their command types and carry the app name after the command char.
    EXPECT_EQ(cmd_parse_network("aBot", 4).type, NET_CMD_START);
    EXPECT_EQ(cmd_parse_network("oBot", 4).type, NET_CMD_STOP);
    EXPECT_EQ(cmd_parse_network("rBot", 4).type, NET_CMD_RESTART);
    EXPECT_EQ(cmd_parse_network("kBot", 4).type, NET_CMD_RESUME);

    net_command_t s = cmd_parse_network("sBot", 4);
    EXPECT_EQ(s.type, NET_CMD_STATUS);
    EXPECT_STREQ(s.app_name, "Bot");
}

// --- Number parsing (untrusted input) ---

TEST(UtilsTest, ParseNumber)
{
    int cnt = 0;
    EXPECT_EQ(parse_number("12345", 5, &cnt), 12345);
    EXPECT_EQ(cnt, 5);

    EXPECT_EQ(parse_number("42+", 3, &cnt), 42); // stops at '+'
    EXPECT_EQ(cnt, 2);

    EXPECT_EQ(parse_number("xyz", 3, &cnt), 0);  // no digit
    EXPECT_EQ(parse_number("999", 1, &cnt), 9);  // length-bounded, no over-read

    // A long digit run must clamp at INT32_MAX, not overflow (signed overflow is UB).
    EXPECT_EQ(parse_number("99999999999999999", 17, &cnt), INT32_MAX);
}

TEST(UtilsTest, ParseInt)
{
    int r = 0;
    EXPECT_TRUE(parse_int("30", 0, 100, &r));
    EXPECT_EQ(r, 30);
    EXPECT_FALSE(parse_int("999", 0, 100, &r));  // out of range
    EXPECT_FALSE(parse_int("abc", 0, 100, &r));  // not a number
}

// --- Restart backoff (math + overflow safety) ---

TEST(ProcessTest, BackoffDelay)
{
    EXPECT_EQ(calculate_backoff_delay(1, 1), 1);   // 2^0
    EXPECT_EQ(calculate_backoff_delay(3, 2), 8);   // 2 * 2^2
    EXPECT_EQ(calculate_backoff_delay(5, 1), 16);  // 2^4
    // Extreme inputs must clamp to the 1-hour cap, never overflow / go negative.
    EXPECT_EQ(calculate_backoff_delay(50, 1), 3600);
    EXPECT_EQ(calculate_backoff_delay(1, 100000), 3600);
}

// --- Status JSON contract (client reads what the server emits) ---

TEST(StatusTest, ExtractField)
{
    const char *json =
        "{\"app\":\"Bot\",\"status\":\"running\",\"circuit\":\"closed\",\"pid\":4321,"
        "\"cpu\":0.50,\"mem_kb\":11908,\"cooldown\":0,\"failures\":0,\"ack_required\":false}";
    char v[32];

    EXPECT_EQ(status_extract_field(json, "status", v, sizeof(v)), 0);
    EXPECT_STREQ(v, "running");
    EXPECT_EQ(status_extract_field(json, "pid", v, sizeof(v)), 0);
    EXPECT_STREQ(v, "4321");
    EXPECT_EQ(status_extract_field(json, "ack_required", v, sizeof(v)), 0);
    EXPECT_STREQ(v, "false");
    EXPECT_EQ(status_extract_field(json, "missing", v, sizeof(v)), -1);
}
