#include <cstddef>
#include <cstdint>

#include <sys/mman.h>
#include <unistd.h>

#include "main_protocol_bridge.hpp"
#include "signal_guard.hpp"

namespace {

constexpr int kMainCommandSigsegv = 11;
constexpr int kMainCommandLock = 12;
constexpr int kMainCommandHandtest = 13;

static sigjmp_buf main_command_guard_env;

static void catch_main_command_guard_signal(int signal_number) {
    (void) signal_number;
    siglongjmp(main_command_guard_env, 1);
}

static bool main_protocol_invoke_guarded_packet(
    const uint8_t *prefix, size_t prefix_len, unsigned int len
) {
    const long page_size = sysconf(_SC_PAGESIZE);
    REQUIRE(page_size > 0);

    const size_t mapping_size = static_cast<size_t>(page_size) * 2u;
    uint8_t *mapping = static_cast<uint8_t *>(
        mmap(nullptr, mapping_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
    );
    REQUIRE(mapping != MAP_FAILED);

    uint8_t *guard = mapping + page_size;
    if (mprotect(guard, static_cast<size_t>(page_size), PROT_NONE) != 0) {
        munmap(mapping, mapping_size);
        REQUIRE(false);
    }

    REQUIRE(prefix_len <= static_cast<size_t>(page_size));
    uint8_t *packet = guard - prefix_len;
    for (size_t i = 0; i < prefix_len; ++i) {
        packet[i] = prefix[i];
    }

    bool ok = run_without_signal(
        kMainCommandSigsegv,
        &main_command_guard_env,
        catch_main_command_guard_signal,
        [&]() { vesc_if_fake_invoke_app_data_handler(packet, len); }
    );
    munmap(mapping, mapping_size);
    return ok;
}

}  // namespace

TEST_CASE("main rejects short lock and handtest packets", "[main][protocol]") {
    Data *data = nullptr;
    REQUIRE(main_protocol_start(&data));

    const uint8_t lock_without_payload[] = {101, static_cast<uint8_t>(kMainCommandLock)};
    const bool lock_ok = main_protocol_invoke_guarded_packet(
        lock_without_payload, sizeof(lock_without_payload), 2u
    );

    data->state.state = STATE_READY;
    data->state.mode = MODE_NORMAL;
    const uint8_t handtest_without_payload[] = {101, static_cast<uint8_t>(kMainCommandHandtest)};
    const bool handtest_ok = main_protocol_invoke_guarded_packet(
        handtest_without_payload, sizeof(handtest_without_payload), 2u
    );

    CHECK(lock_ok);
    CHECK(handtest_ok);
}
