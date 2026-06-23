// Copyright 2024 Lukas Hrazky
//
// This file is part of the Refloat VESC package.
//
// Refloat VESC package is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.
//
// Refloat VESC package is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <http://www.gnu.org/licenses/>.

#include "data_recorder.h"
#include "conf/buffer.h"
#include "lib/utils.h"
#include "vesc_c_if.h"

#include <string.h>

static void start_recording(DataRecord *dr) {
    circular_buffer_clear(&dr->buffer);
    dr->decimation_counter = 0;
    dr->last_time = 0;
    dr->recording = true;
}

static void stop_recording(DataRecord *dr) {
    dr->recording = false;
}

typedef struct {
    uint32_t magic;
    uint8_t *buffer;
    size_t length;
} DataBufferInfo;

void data_recorder_init(DataRecord *dr, uint16_t imu_sample_rate) {
    dr->recording = false;
    dr->autostart = true;
    dr->autostop = true;
    dr->decimation_counter = 0;
    dr->last_time = 0;

    // fetch information about the data buffer, it's stored at the end of the
    // VESC interface memory area
    DataBufferInfo buffer_info;
    memcpy(&buffer_info, (uint8_t *) VESC_IF + 2036, sizeof(buffer_info));

    // Magic format: 0xcafe1XVM where X=ignored (reserved for future use),
    // V=major version (4 bits), M=minor version (4 bits)
    // we check that the base magic matches and the version is compatible
    const uint32_t magic_base = buffer_info.magic & 0xfffff000;
    const uint8_t magic_major = (buffer_info.magic >> 4) & 0x0f;
    const uint8_t magic_minor = buffer_info.magic & 0x0f;
    const uint8_t required_major = 1;
    const uint8_t required_minor = 1;
    if (magic_base != 0xcafe1000 || magic_major != required_major || magic_minor < required_minor) {
        if (buffer_info.magic != 0) {
            log_msg("Data Record incompatible magic: 0x%08x", buffer_info.magic);
        }
        dr->enabled = false;
        return;
    }

    dr->enabled = true;
    size_t size = buffer_info.length;
    dr->sample_count = size / sizeof(Sample);
    if (dr->sample_count == 0) {
        dr->enabled = false;
        return;
    }
    uint8_t *buffer = buffer_info.buffer;
    circular_buffer_init(&dr->buffer, sizeof(Sample), dr->sample_count, buffer);

    dr->sample_rate = imu_sample_rate;

    // calculate decimation so that the recorded time period is at least 10 seconds
    dr->decimation = min(max(10u * imu_sample_rate / dr->sample_count, 1u), 255u);

    log_msg("Data Record buffer size: %uB (%u samples)", size, dr->sample_count);
}

void data_recorder_set_sample_rate(DataRecord *dr, uint16_t sample_rate) {
    dr->sample_rate = sample_rate;
    if (dr->sample_count > 0) {
        dr->decimation = min(max(10u * sample_rate / dr->sample_count, 1u), 255u);
    }
}

bool data_recorder_has_capability(const DataRecord *dr) {
    return dr->enabled;
}

void data_recorder_trigger(DataRecord *dr, bool engage) {
    if (!dr->enabled) {
        return;
    }

    if (dr->autostart && engage) {
        start_recording(dr);
    } else if (dr->autostop && !engage) {
        stop_recording(dr);
    }
}

void data_recorder_sample(DataRecord *dr, const Data *d, time_t time) {
    if (!dr->enabled || !dr->recording) {
        return;
    }

    if (++dr->decimation_counter < dr->decimation) {
        return;
    }
    dr->decimation_counter = 0;

    // keep timestamps strictly increasing: the 100us system tick can't
    // distinguish samples at high IMU rates (they arrive in bursts sharing a
    // tick), and duplicate timestamps break the download and plotting
    if (time <= dr->last_time) {
        time = dr->last_time + 1;
    }
    dr->last_time = time;

    uint8_t flags = d->state.sat << 4 | d->footpad.state << 2;
    flags |= d->state.wheelslip << 1 | (d->state.state == STATE_RUNNING);

    Sample sample =
        {.time = time,
         .flags = flags,
         .values = {
#define ARRAY_VALUE(target, id) to_float16(d->target),
             VISIT_REC(RT_DATA_ALL_ITEMS, ARRAY_VALUE)
#undef ARRAY_VALUE
         }};
    circular_buffer_push(&dr->buffer, &sample);
}

static void send_point_vt_experiment(const void *item, void *data) {
    unused(data);

    Sample *sample = (Sample *) item;
    for (uint8_t i = 0; i < ITEMS_COUNT_REC(RT_DATA_ALL_ITEMS); ++i) {
        VESC_IF->plot_set_graph(i);
        VESC_IF->plot_send_points(
            sample->time * (1.0f / SYSTEM_TICK_RATE_HZ), from_float16(sample->values[i])
        );
    }
}

void data_recorder_send_experiment_plot(DataRecord *dr) {
    if (!dr->enabled) {
        return;
    }

    bool recording = dr->recording;
    stop_recording(dr);
    VESC_IF->plot_init("t", "v");

#define ADD_GRAPH(target, id) VESC_IF->plot_add_graph(id);
    VISIT_REC(RT_DATA_ALL_ITEMS, ADD_GRAPH);
#undef ADD_GRAPH

    circular_buffer_iterate(&dr->buffer, &send_point_vt_experiment, 0);
    dr->recording = recording;
}

typedef enum {
    COMMAND_DATA_RECORD = 41,
    COMMAND_DATA_RECORD_HEADER = 42,
    COMMAND_DATA_RECORD_DATA = 43,
} DataRecordCommands;

static void send_status(const DataRecord *dr) {
    uint8_t buf[7];
    int32_t ind = 0;

    buf[ind++] = 101;  // Package ID
    buf[ind++] = COMMAND_DATA_RECORD;
    buf[ind++] = dr->enabled;
    buf[ind++] = dr->autostop << 2 | dr->autostart << 1 | dr->recording;
    buf[ind++] = dr->decimation;
    uint32_t centiseconds =
        dr->sample_rate == 0 ? 0 : (uint32_t) dr->sample_count * 100 / dr->sample_rate;
    buffer_append_uint16(buf, min(centiseconds, 65535u), &ind);

    SEND_APP_DATA(buf, 7, ind);
}

static void send_header(DataRecord *dr) {
    static const int bufsize = 256;
    uint8_t buf[bufsize];
    int32_t ind = 0;

    buf[ind++] = 101;  // Package ID
    buf[ind++] = COMMAND_DATA_RECORD_HEADER;

    buffer_append_uint32(buf, circular_buffer_size(&dr->buffer), &ind);

    buf[ind++] = ITEMS_COUNT_REC(RT_DATA_ALL_ITEMS);
#define ADD_ID(target, id) buffer_append_string(buf, id, &ind);
    VISIT_REC(RT_DATA_ALL_ITEMS, ADD_ID);
#undef ADD_ID

    SEND_APP_DATA(buf, bufsize, ind);
}

static void send_data(const DataRecord *dr, size_t offset) {
    static const int bufsize = SEND_BUF_MAX_SIZE;
    uint8_t buf[bufsize];
    int32_t ind = 0;

    buf[ind++] = 101;  // Package ID
    buf[ind++] = COMMAND_DATA_RECORD_DATA;

    buffer_append_uint32(buf, offset, &ind);

    Sample sample;
    const size_t sample_size = 4u + 1u + 2u * ITEMS_COUNT_REC(RT_DATA_ALL_ITEMS);
    while (circular_buffer_get(&dr->buffer, offset++, &sample)) {
        if ((size_t) ind + sample_size > (size_t) bufsize) {
            break;
        }

        buffer_append_uint32(buf, sample.time, &ind);
        buf[ind++] = sample.flags;

        for (size_t i = 0; i < ITEMS_COUNT_REC(RT_DATA_ALL_ITEMS); ++i) {
            buffer_append_uint16(buf, sample.values[i], &ind);
        }
    }

    SEND_APP_DATA(buf, bufsize, ind);
}

void data_recorder_request(DataRecord *dr, uint8_t *buffer, size_t len) {
    if (len < 2) {
        log_error("Data Record request missing data.");
        return;
    }

    if (!dr->enabled) {
        if (buffer[0] == 1 && buffer[1] == 0) {
            send_status(dr);
        } else {
            log_error("Data Record not supported.");
        }
        return;
    }

    int32_t ind = 0;
    uint8_t mode = buffer[ind++];
    uint8_t sub_mode = buffer[ind++];
    if (mode == 1) {  // control
        if (sub_mode > 0) {
            if (len < 3) {
                log_error("Data Record request missing value, length: %u", len);
                return;
            }
            uint8_t value = buffer[ind++];
            if (sub_mode == 1) {  // start/stop recording
                if (value > 0) {
                    start_recording(dr);
                } else {
                    stop_recording(dr);
                }
            } else if (sub_mode == 2) {  // set autostart on engage
                dr->autostart = value;
            } else if (sub_mode == 3) {  // set autostop on disengage
                dr->autostop = value;
            } else if (sub_mode == 4) {  // set decimation (record every Nth sample)
                dr->decimation = value > 0 ? value : 1;
            }
        }
        // sub_mode 0 is a no-op, just return the status
        send_status(dr);
    } else if (mode == 2) {  // send
        if (sub_mode == 1) {  // header
            // pause recording; in theory a race, but the delay between sending the header
            // and getting a data request should be enough for any writing to end
            stop_recording(dr);
            send_header(dr);
        } else if (sub_mode == 2) {  // data
            if (len < 6) {
                log_error("Data Record request missing offset, length: %u", len);
                return;
            }

            size_t offset = buffer_get_uint32(buffer, &ind);
            stop_recording(dr);
            send_data(dr, offset);
        }
    }
}
