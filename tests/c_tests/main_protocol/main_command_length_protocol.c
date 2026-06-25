enum {
    MAIN_COMMAND_LOCK = 12,
    MAIN_COMMAND_HANDTEST = 13,
};

typedef struct {
    const uint8_t *prefix;
    size_t prefix_len;
    unsigned int len;
} MainCommandGuardedPacket;

static bool run_main_command_guarded_packet(void *ctx) {
    MainCommandGuardedPacket *guard = ctx;
    long page_size = sysconf(_SC_PAGESIZE);
    EXPECT_TRUE(page_size > 0);

    size_t mapping_size = (size_t) page_size * 2u;
    uint8_t *mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    EXPECT_TRUE(mapping != MAP_FAILED);

    uint8_t *guard_page = mapping + page_size;
    if (mprotect(guard_page, (size_t) page_size, PROT_NONE) != 0) {
        munmap(mapping, mapping_size);
        EXPECT_TRUE(false);
    }

    EXPECT_TRUE(guard->prefix_len <= (size_t) page_size);
    uint8_t *packet = guard_page - guard->prefix_len;
    for (size_t i = 0; i < guard->prefix_len; ++i) {
        packet[i] = guard->prefix[i];
    }

    vesc_if_fake_invoke_app_data_handler(packet, guard->len);
    munmap(mapping, mapping_size);
    return true;
}

static bool main_protocol_invoke_guarded_packet(const uint8_t *prefix, size_t prefix_len, unsigned int len) {
    MainCommandGuardedPacket guard = {.prefix = prefix, .prefix_len = prefix_len, .len = len};
    return test_expect_no_signal(SIGSEGV, run_main_command_guarded_packet, &guard);
}

static bool test_main_rejects_short_lock_and_handtest_payloads(void) {
    MainProtocolFixture fixture = {0};
    EXPECT_TRUE(main_protocol_fixture_start(&fixture));
    Data *d = fixture.data;

    uint8_t lock_without_payload[] = {101, MAIN_COMMAND_LOCK};
    bool lock_ok = main_protocol_invoke_guarded_packet(lock_without_payload, sizeof(lock_without_payload), 2u);

    d->state.state = STATE_READY;
    d->state.mode = MODE_NORMAL;
    uint8_t handtest_without_payload[] = {101, MAIN_COMMAND_HANDTEST};
    bool handtest_ok =
        main_protocol_invoke_guarded_packet(handtest_without_payload, sizeof(handtest_without_payload), 2u);

    main_protocol_fixture_stop(&fixture);

    XEXPECT_TRUE(lock_ok);
    XEXPECT_TRUE(handtest_ok);
    return true;
}
