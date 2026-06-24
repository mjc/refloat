static sigjmp_buf main_command_guard_env;
static volatile int main_command_guard_signal;

enum {
    MAIN_COMMAND_SIGSEGV = 11,
    MAIN_COMMAND_LOCK = 12,
    MAIN_COMMAND_HANDTEST = 13,
};

static void catch_main_command_guard_signal(int signal_number) {
    main_command_guard_signal = signal_number;
    siglongjmp(main_command_guard_env, 1);
}

static bool main_protocol_invoke_guarded_packet(const uint8_t *prefix, size_t prefix_len, unsigned int len) {
    long page_size = sysconf(_SC_PAGESIZE);
    EXPECT_TRUE(page_size > 0);

    size_t mapping_size = (size_t) page_size * 2u;
    uint8_t *mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    EXPECT_TRUE(mapping != MAP_FAILED);

    uint8_t *guard = mapping + page_size;
    if (mprotect(guard, (size_t) page_size, PROT_NONE) != 0) {
        munmap(mapping, mapping_size);
        EXPECT_TRUE(false);
    }

    EXPECT_TRUE(prefix_len <= (size_t) page_size);
    uint8_t *packet = guard - prefix_len;
    for (size_t i = 0; i < prefix_len; ++i) {
        packet[i] = prefix[i];
    }

    SignalHandler previous_sigsegv = signal(MAIN_COMMAND_SIGSEGV, catch_main_command_guard_signal);
    main_command_guard_signal = 0;

    bool ok = true;
    if (sigsetjmp(main_command_guard_env, 1) == 0) {
        vesc_if_fake_invoke_app_data_handler(packet, len);
    } else {
        ok = false;
    }

    signal(MAIN_COMMAND_SIGSEGV, previous_sigsegv);
    munmap(mapping, mapping_size);

    return ok;
}

static bool test_main_rejects_short_lock_and_handtest_payloads(void) {
    lib_info info = {0};
    Data *d = NULL;
    EXPECT_TRUE(main_protocol_start(&info, &d));

    uint8_t lock_without_payload[] = {101, MAIN_COMMAND_LOCK};
    bool lock_ok = main_protocol_invoke_guarded_packet(lock_without_payload, sizeof(lock_without_payload), 2u);

    d->state.state = STATE_READY;
    d->state.mode = MODE_NORMAL;
    uint8_t handtest_without_payload[] = {101, MAIN_COMMAND_HANDTEST};
    bool handtest_ok =
        main_protocol_invoke_guarded_packet(handtest_without_payload, sizeof(handtest_without_payload), 2u);

    info.stop_fun(info.arg);

    XEXPECT_TRUE(lock_ok);
    XEXPECT_TRUE(handtest_ok);
    return true;
}
