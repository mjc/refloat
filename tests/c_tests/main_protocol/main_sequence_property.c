typedef enum {
    MAIN_SEQUENCE_NEUTRAL,
    MAIN_SEQUENCE_IMU_WAIT,
    MAIN_SEQUENCE_ENGAGE,
    MAIN_SEQUENCE_RELEASE,
    MAIN_SEQUENCE_LEFT_SENSOR,
    MAIN_SEQUENCE_FORWARD,
    MAIN_SEQUENCE_REVERSE,
    MAIN_SEQUENCE_PITCH_FAULT,
    MAIN_SEQUENCE_ROLL_FAULT,
    MAIN_SEQUENCE_CHARGING,
    MAIN_SEQUENCE_HANDTEST,
    MAIN_SEQUENCE_FLYWHEEL,
    MAIN_SEQUENCE_LOCK,
    MAIN_SEQUENCE_SHORT_PACKET,
    MAIN_SEQUENCE_VESC_FAULT,
    MAIN_SEQUENCE_EVENT_COUNT,
} MainSequenceEvent;

enum {
    MAIN_SEQUENCE_STEPS = 64
};

typedef struct {
    RunState state;
    Mode mode;
    StopCondition stop_condition;
    bool charging;
    float motor_current;
    float motor_brake_current;
    float motor_duty;
} MainSequenceObservation;

static const char *main_sequence_event_name(MainSequenceEvent event) {
    static const char *const names[] = {
        "neutral",
        "imu-wait",
        "engage",
        "release",
        "left-sensor",
        "forward",
        "reverse",
        "pitch",
        "roll",
        "charging",
        "handtest",
        "flywheel",
        "lock",
        "short",
        "vesc-fault",
    };
    return names[event];
}

static bool main_sequence_require(
    bool condition,
    uint32_t seed,
    size_t step,
    RunState prior_state,
    Mode prior_mode,
    MainSequenceEvent event,
    const Data *d,
    const char *invariant
) {
    if (!condition) {
        fprintf(
            stderr,
            "seed=0x%08x step=%zu prior=%u/%u event=%s result=%u/%u "
            "motor=%g brake=%g duty=%g: %s\n",
            (unsigned int) seed,
            step,
            (unsigned int) prior_state,
            (unsigned int) prior_mode,
            main_sequence_event_name(event),
            (unsigned int) d->state.state,
            (unsigned int) d->state.mode,
            (double) vesc_if_fake_last_current(),
            (double) vesc_if_fake_last_brake_current(),
            (double) vesc_if_fake_last_duty(),
            invariant
        );
    }
    return condition;
}

static bool main_sequence_observation_equal(
    const MainSequenceObservation *actual, const MainSequenceObservation *expected
) {
    return actual->state == expected->state && actual->mode == expected->mode &&
        actual->stop_condition == expected->stop_condition &&
        actual->charging == expected->charging &&
        actual->motor_current == expected->motor_current &&
        actual->motor_brake_current == expected->motor_brake_current &&
        actual->motor_duty == expected->motor_duty;
}

static bool main_run_event_sequence(uint32_t seed, MainSequenceObservation *expected, bool record) {
    MainProtocolFixture fixture = {0};
    if (!main_protocol_fixture_start(&fixture)) {
        return false;
    }
    Data *d = fixture.data;
    uint32_t random = seed;
    vesc_if_fake_set_eeprom_read_enabled(true);
    vesc_if_fake_set_eeprom_store_enabled(true);

    for (size_t step = 0; step < MAIN_SEQUENCE_STEPS; ++step) {
        test_xorshift32(&random);
        MainSequenceEvent event = step < MAIN_SEQUENCE_EVENT_COUNT
            ? (MainSequenceEvent) ((step + seed) % MAIN_SEQUENCE_EVENT_COUNT)
            : (MainSequenceEvent) (random % MAIN_SEQUENCE_EVENT_COUNT);
        RunState prior_state = d->state.state;
        Mode prior_mode = d->state.mode;
        systime_t ticks = (systime_t) ((step + 1u) * SYSTEM_TICK_RATE_HZ / 4u);

        vesc_if_fake_set_ticks(ticks);
        vesc_if_fake_set_seconds((float) ticks / SYSTEM_TICK_RATE_HZ);
        vesc_if_fake_set_fault(FAULT_CODE_NONE);
        vesc_if_fake_set_imu_startup_done(true);
        d->imu.balance_pitch = 0.0f;
        d->imu.pitch = 0.0f;
        d->imu.roll = 0.0f;
        float sensor = d->state.state == STATE_RUNNING ? 2.0f : 0.0f;
        vesc_if_fake_set_analog(sensor, sensor);
        vesc_if_fake_set_motor_telemetry(0, 0, 0, 0, 0, 0.1f, 0, 50, 25, 25);

        switch (event) {
        case MAIN_SEQUENCE_NEUTRAL:
            break;
        case MAIN_SEQUENCE_IMU_WAIT:
            vesc_if_fake_set_imu_startup_done(false);
            break;
        case MAIN_SEQUENCE_ENGAGE:
            vesc_if_fake_set_analog(2.0f, 2.0f);
            break;
        case MAIN_SEQUENCE_RELEASE:
            vesc_if_fake_set_analog(0.0f, 0.0f);
            break;
        case MAIN_SEQUENCE_LEFT_SENSOR:
            vesc_if_fake_set_analog(2.0f, 0.0f);
            break;
        case MAIN_SEQUENCE_FORWARD:
            vesc_if_fake_set_motor_telemetry(2500, 10, 2.5f, 5, 5, 0.2f, 1, 50, 25, 25);
            break;
        case MAIN_SEQUENCE_REVERSE:
            vesc_if_fake_set_motor_telemetry(-2500, -10, -2.5f, -5, -5, 0.2f, -1, 50, 25, 25);
            break;
        case MAIN_SEQUENCE_PITCH_FAULT:
            d->imu.balance_pitch = 60.0f;
            d->imu.pitch = 60.0f;
            break;
        case MAIN_SEQUENCE_ROLL_FAULT:
            d->imu.roll = 80.0f;
            break;
        case MAIN_SEQUENCE_CHARGING: {
            uint8_t packet[] = {151, d->state.charging ? 0 : 1, 0, 120, 0, 30};
            charging_state_request(&d->charging, packet, sizeof(packet), &d->state);
            break;
        }
        case MAIN_SEQUENCE_HANDTEST: {
            uint8_t packet[] = {101, MAIN_COMMAND_HANDTEST, d->state.mode != MODE_HANDTEST};
            vesc_if_fake_invoke_app_data_handler(packet, sizeof(packet));
            break;
        }
        case MAIN_SEQUENCE_FLYWHEEL: {
            uint8_t packet[] = {0x82, 90, 50, 30, 20, 0, 20};
            d->imu.pitch = d->state.mode == MODE_FLYWHEEL ? 0.0f : 80.0f;
            main_send_flywheel(packet, sizeof(packet));
            d->imu.pitch = 0.0f;
            break;
        }
        case MAIN_SEQUENCE_LOCK: {
            uint8_t packet[] = {101, MAIN_COMMAND_LOCK, d->state.state != STATE_DISABLED};
            vesc_if_fake_invoke_app_data_handler(packet, sizeof(packet));
            break;
        }
        case MAIN_SEQUENCE_SHORT_PACKET: {
            uint8_t packet[] = {101, MAIN_COMMAND_HANDTEST};
            State before = d->state;
            vesc_if_fake_invoke_app_data_handler(packet, sizeof(packet));
            if (!main_sequence_require(
                    d->state.state == before.state && d->state.mode == before.mode &&
                        d->state.charging == before.charging,
                    seed,
                    step,
                    prior_state,
                    prior_mode,
                    event,
                    d,
                    "short packet mutated state"
                )) {
                main_protocol_fixture_stop(&fixture);
                return false;
            }
            break;
        }
        case MAIN_SEQUENCE_VESC_FAULT:
            vesc_if_fake_set_fault(FAULT_CODE_OVER_VOLTAGE);
            break;
        case MAIN_SEQUENCE_EVENT_COUNT:
            break;
        }

        size_t prior_current_calls = vesc_if_fake_mc_set_current_calls();
        size_t prior_brake_calls = vesc_if_fake_mc_set_brake_current_calls();
        size_t prior_duty_calls = vesc_if_fake_mc_set_duty_calls();
        vesc_if_fake_set_terminate_after_sleep(true);
        refloat_main_run_loop(d);

        float current_limit = d->state.mode == MODE_HANDTEST
            ? 7.0f
            : (d->state.mode == MODE_FLYWHEEL
                   ? 40.0f
                   : (d->motor.braking ? d->motor.current_min : d->motor.current_max));
        bool valid = d->state.state >= STATE_DISABLED && d->state.state <= STATE_RUNNING &&
            d->state.mode >= MODE_NORMAL && d->state.mode <= MODE_FLYWHEEL &&
            isfinite(d->setpoint) && isfinite(d->setpoint_target) &&
            isfinite(d->setpoint_target_interpolated) &&
            (vesc_if_fake_mc_set_current_calls() == prior_current_calls ||
             (isfinite(vesc_if_fake_last_current()) &&
              fabsf(vesc_if_fake_last_current()) <= fabsf(current_limit))) &&
            (vesc_if_fake_mc_set_brake_current_calls() == prior_brake_calls ||
             (isfinite(vesc_if_fake_last_brake_current()) &&
              fabsf(vesc_if_fake_last_brake_current()) <= d->motor_control.brake_current)) &&
            (vesc_if_fake_mc_set_duty_calls() == prior_duty_calls ||
             (isfinite(vesc_if_fake_last_duty()) && fabsf(vesc_if_fake_last_duty()) <= 1.0f));
        if (!main_sequence_require(
                valid,
                seed,
                step,
                prior_state,
                prior_mode,
                event,
                d,
                "state or motor-output invariant failed"
            )) {
            main_protocol_fixture_stop(&fixture);
            return false;
        }

        MainSequenceObservation actual = {
            .state = d->state.state,
            .mode = d->state.mode,
            .stop_condition = d->state.stop_condition,
            .charging = d->state.charging,
            .motor_current = vesc_if_fake_last_current(),
            .motor_brake_current = vesc_if_fake_last_brake_current(),
            .motor_duty = vesc_if_fake_last_duty(),
        };
        if (record) {
            expected[step] = actual;
        } else if (!main_sequence_require(
                       main_sequence_observation_equal(&actual, &expected[step]),
                       seed,
                       step,
                       prior_state,
                       prior_mode,
                       event,
                       d,
                       "replay diverged"
                   )) {
            main_protocol_fixture_stop(&fixture);
            return false;
        }
    }

    MainSequenceObservation final = expected[MAIN_SEQUENCE_STEPS - 1u];
    main_protocol_fixture_stop(&fixture);
    if (vesc_if_fake_free_calls() != vesc_if_fake_malloc_calls() ||
        vesc_if_fake_request_terminate_calls() != 2u ||
        vesc_if_fake_set_app_data_handler_calls() < 2u ||
        vesc_if_fake_imu_set_read_callback_calls() < 2u ||
        vesc_if_fake_conf_custom_clear_configs_calls() == 0u) {
        fprintf(
            stderr,
            "seed=0x%08x step=%u prior=%u/%u event=stop result=%u/%u motor=%g: "
            "teardown invariant failed\n",
            (unsigned int) seed,
            MAIN_SEQUENCE_STEPS,
            (unsigned int) final.state,
            (unsigned int) final.mode,
            (unsigned int) final.state,
            (unsigned int) final.mode,
            (double) final.motor_current
        );
        return false;
    }
    return true;
}

static bool test_main_loop_replays_bounded_event_sequences(void) {
    const uint32_t seeds[] = {1u, 0x13579bdfu, 0x2468ace1u, 0x9e3779b9u, 0xc001d00du};
    MainSequenceObservation expected[MAIN_SEQUENCE_STEPS];

    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        EXPECT_TRUE(main_run_event_sequence(seeds[i], expected, true));
        EXPECT_TRUE(main_run_event_sequence(seeds[i], expected, false));
    }
    return true;
}
