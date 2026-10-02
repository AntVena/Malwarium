// audio_esp32.h — ISoundOut on the ES8311 codec + NS4150B amp over I2S.
#pragma once

#include <Arduino.h>

#include "config.h"
#include "platform/platform.h"

#if AUDIO_ENABLED && AUDIO_USE_I2S
#include <Wire.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#endif

namespace mal {

#if AUDIO_ENABLED && AUDIO_USE_I2S

class Esp32Sound : public ISoundOut {
public:
    bool begin() {
        pinMode(PIN_AUDIO_PA_CTRL, OUTPUT);
        digitalWrite(PIN_AUDIO_PA_CTRL, LOW);

        i2s_config_t cfg = {};
        cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
        cfg.sample_rate = kRate;
        cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
        cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
        cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
        cfg.intr_alloc_flags = 0;
        cfg.dma_buf_count = 4;
        cfg.dma_buf_len = kChunkFrames;
        cfg.use_apll = false;
        cfg.tx_desc_auto_clear = true;
        cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
        if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) {
            Serial.println("[audio] i2s install failed - sound off");
            return false;
        }
        i2s_pin_config_t pins = {};
        pins.mck_io_num = PIN_I2S_MCLK;
        pins.bck_io_num = PIN_I2S_BCLK;
        pins.ws_io_num = PIN_I2S_LRCLK;
        pins.data_out_num = PIN_I2S_DOUT;
        pins.data_in_num = I2S_PIN_NO_CHANGE;
        i2s_set_pin(I2S_NUM_0, &pins);
        i2s_zero_dma_buffer(I2S_NUM_0);

        // The codec must see MCLK before it is configured.
        Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, static_cast<uint32_t>(400000));
        if (!initCodec()) {
            Serial.printf("[audio] no ES8311 at 0x%02x - sound off\n", kCodecAddr);
            i2s_driver_uninstall(I2S_NUM_0);
            return false;
        }
        i2s_stop(I2S_NUM_0);

        queue_ = xQueueCreate(1, sizeof(Request));
        if (!queue_ ||
            xTaskCreatePinnedToCore(&Esp32Sound::taskEntry, "sound", 4096, this, 1, nullptr,
                                    0) != pdPASS) {
            Serial.println("[audio] task start failed - sound off");
            return false;
        }
        ready_ = true;
        Serial.printf("[audio] ES8311 ready, %dHz\n", kRate);
        return true;
    }

    void play(Sound s, int volumePercent) override {
        if (soundTier(s) == SoundTier::Alert) alertWake_ = true;
        if (!ready_ || !soundDef(s)) return;
        if (volumePercent < 0) volumePercent = 0;
        if (volumePercent > 100) volumePercent = 100;
        const Request r{static_cast<uint8_t>(s), static_cast<uint8_t>(volumePercent)};
        xQueueOverwrite(queue_, &r);
    }

    bool busy() const {
        return ready_ && (playing_ || uxQueueMessagesWaiting(queue_) > 0);
    }

    bool takeAlertWake() {
        if (!alertWake_) return false;
        alertWake_ = false;
        return true;
    }

    // Hold the amp enable low; it floats in deep sleep otherwise.
    void end() { digitalWrite(PIN_AUDIO_PA_CTRL, LOW); }

private:
    struct Request {
        uint8_t sound;
        uint8_t volume;
    };

    static constexpr int kRate = 16000;
    static constexpr int kChunkFrames = 256;
    static constexpr uint8_t kCodecAddr = 0x18;   // ES8311 with CE low
    static constexpr int kLeadInMs = 20;
    static constexpr int kTailMs = 20;
    static constexpr int kRampFrames = kRate / 1000 * 2;

    QueueHandle_t queue_ = nullptr;
    volatile bool ready_ = false;
    volatile bool playing_ = false;
    volatile bool alertWake_ = false;
    int16_t buf_[kChunkFrames * 2];

    bool writeReg(uint8_t reg, uint8_t val) {
        Wire.beginTransmission(kCodecAddr);
        Wire.write(reg);
        Wire.write(val);
        return Wire.endTransmission() == 0;
    }

    // Register values from Espressif's esp_codec_dev driver: slave, MCLK = 256fs, 16-bit.
    bool initCodec() {
        Wire.beginTransmission(kCodecAddr);
        if (Wire.endTransmission() != 0) return false;
        static const uint8_t kInit[][2] = {
            {0x00, 0x1F}, {0x00, 0x00},
            {0x01, 0x30}, {0x02, 0x00}, {0x03, 0x10}, {0x16, 0x24}, {0x04, 0x10},
            {0x05, 0x00}, {0x0B, 0x00}, {0x0C, 0x00}, {0x10, 0x1F}, {0x11, 0x7F},
            {0x00, 0x80},
            {0x01, 0x3F},
            {0x02, 0x00}, {0x03, 0x10}, {0x04, 0x10}, {0x05, 0x00},
            {0x06, 0x03}, {0x07, 0x00}, {0x08, 0xFF},
            {0x09, 0x0C}, {0x0A, 0x0C},
            {0x13, 0x10}, {0x1B, 0x0A}, {0x1C, 0x6A},
            {0x0D, 0x01}, {0x0E, 0x02}, {0x12, 0x00},
            {0x14, 0x1A}, {0x15, 0x40}, {0x37, 0x08}, {0x45, 0x00},
            {0x32, 0xBF},
            {0x31, 0x00},
        };
        for (const auto& rv : kInit) {
            if (!writeReg(rv[0], rv[1])) return false;
            if (rv[0] == 0x00 && rv[1] == 0x1F) delay(20);
        }
        return true;
    }

    static void taskEntry(void* self) { static_cast<Esp32Sound*>(self)->run(); }

    void run() {
        Request r{};
        for (;;) {
            // Peek before taking, so busy() never sees an empty queue with playing_ false.
            if (xQueuePeek(queue_, &r, portMAX_DELAY) != pdTRUE) continue;
            playing_ = true;
            if (xQueueReceive(queue_, &r, 0) != pdTRUE) { playing_ = false; continue; }
            i2s_start(I2S_NUM_0);
            digitalWrite(PIN_AUDIO_PA_CTRL, HIGH);
            writeSilence(kLeadInMs);
            do {
                render(r);
            } while (xQueueReceive(queue_, &r, 0) == pdTRUE);
            writeSilence(kTailMs);
            digitalWrite(PIN_AUDIO_PA_CTRL, LOW);
            i2s_zero_dma_buffer(I2S_NUM_0);
            i2s_stop(I2S_NUM_0);
            playing_ = false;
        }
    }

    static int amplitudeFor(int volumePercent) {
        return AUDIO_PEAK_AMPLITUDE * volumePercent * volumePercent / 10000;
    }

    void writeFrames(int frames) {
        size_t written = 0;
        i2s_write(I2S_NUM_0, buf_, static_cast<size_t>(frames) * 2 * sizeof(int16_t),
                  &written, portMAX_DELAY);
    }

    void writeSilence(int ms) {
        int frames = kRate * ms / 1000;
        for (int i = 0; i < kChunkFrames * 2; ++i) buf_[i] = 0;
        while (frames > 0) {
            const int n = frames < kChunkFrames ? frames : kChunkFrames;
            writeFrames(n);
            frames -= n;
        }
    }

    void render(const Request& r) {
        const SoundDef* d = soundDef(static_cast<Sound>(r.sound));
        if (!d) return;
        const int amp = amplitudeFor(r.volume);
        for (int n = 0; n < d->noteCount; ++n) {
            const SoundNote& note = d->notes[n];
            const int frames = kRate * note.ms / 1000;
            const uint32_t step = note.hz
                ? static_cast<uint32_t>((static_cast<uint64_t>(note.hz) << 32) / kRate)
                : 0;
            uint32_t phase = 0;
            int done = 0;
            while (done < frames) {
                if (uxQueueMessagesWaiting(queue_) > 0) return;
                const int count = frames - done < kChunkFrames ? frames - done : kChunkFrames;
                for (int i = 0; i < count; ++i) {
                    int16_t v = 0;
                    if (step) {
                        const int at = done + i;
                        int env = amp;
                        if (at < kRampFrames) env = amp * at / kRampFrames;
                        else if (frames - at < kRampFrames) env = amp * (frames - at) / kRampFrames;
                        v = static_cast<int16_t>((phase & 0x80000000u) ? env : -env);
                        phase += step;
                    }
                    buf_[i * 2] = v;
                    buf_[i * 2 + 1] = v;
                }
                writeFrames(count);
                done += count;
            }
        }
    }
};

#else

class Esp32Sound : public ISoundOut {
public:
    bool begin() { return false; }
    void play(Sound s, int) override {
        if (soundTier(s) == SoundTier::Alert) alertWake_ = true;
    }
    bool busy() const { return false; }
    bool takeAlertWake() {
        if (!alertWake_) return false;
        alertWake_ = false;
        return true;
    }
    void end() {}

private:
    bool alertWake_ = false;
};

#endif

}
