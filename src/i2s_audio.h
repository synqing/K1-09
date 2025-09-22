/*----------------------------------------
  Sensory Bridge I2S FUNCTIONS
  ----------------------------------------*/

#include <driver/i2s.h>
#include "audio_raw_state.h"
#include "audio_processed_state.h"
#include "debug/debug_manager.h"

// Phase 2A: Access to AudioRawState instance for migration
extern SensoryBridge::Audio::AudioRawState audio_raw_state;

// Phase 2B: Access to AudioProcessedState instance for migration
extern SensoryBridge::Audio::AudioProcessedState audio_processed_state;

const i2s_config_t i2s_config = {
  .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
  .sample_rate = CONFIG.SAMPLE_RATE,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
  .communication_format = I2S_COMM_FORMAT_STAND_I2S,
  .dma_buf_count = 8,  // Increased from 2 for better double-buffering
  .dma_buf_len = CONFIG.SAMPLES_PER_CHUNK * 2,  // Larger buffers reduce interrupt overhead
};

const i2s_pin_config_t pin_config = {
  .bck_io_num = I2S_BCLK_PIN,
  .ws_io_num = I2S_LRCLK_PIN,
  .data_out_num = -1,  // not used (only for outputs)
  .data_in_num = I2S_DIN_PIN
};

void init_i2s() {
  esp_err_t result = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  USBSerial.print("INIT I2S: ");
  USBSerial.println(result == ESP_OK ? SB_PASS : SB_FAIL);

#ifndef ARDUINO_ESP32S3_DEV
  // S2-specific register hacks
  REG_SET_BIT(I2S_TIMING_REG(I2S_PORT), BIT(9));
  REG_SET_BIT(I2S_CONF_REG(I2S_PORT), I2S_RX_MSB_SHIFT);
#endif

  result = i2s_set_pin(I2S_PORT, &pin_config);
  USBSerial.print("I2S SET PINS: ");
  USBSerial.println(result == ESP_OK ? SB_PASS : SB_FAIL);
}

void acquire_sample_chunk(uint32_t t_now) {
  static int8_t sweet_spot_state_last = 0;
  static bool silence_temp = false;
  static uint32_t silence_switched = 0;
  static float silent_scale_last = 1.0;
  static uint32_t last_state_change_time = 0;
  static const uint32_t MIN_STATE_DURATION_MS = 1500; // 1 second minimum in each state
  static float max_waveform_val_raw_smooth = 0.0; // Added for smoothing

  size_t bytes_read = 0;
  size_t bytes_expected = CONFIG.SAMPLES_PER_CHUNK * sizeof(int32_t);

  // MODIFICATION [2025-09-20 22:45] - SURGICAL-FIX-002: Fix I2S partial reads causing random cutouts
  // FAULT DETECTED: 10ms timeout allows partial I2S reads, causing audio corruption and apparent "cutouts"
  // ROOT CAUSE: i2s_read() with timeout can return partial chunks, corrupting audio pipeline downstream
  // SOLUTION RATIONALE: Block until full chunk arrives using portMAX_DELAY to ensure complete reads
  // IMPACT ASSESSMENT: Eliminates partial read corruption, may slightly increase latency during audio gaps
  // VALIDATION METHOD: Monitor bytes_read == bytes_expected, verify no more random audio dropouts
  // ROLLBACK PROCEDURE: Restore 10ms timeout if system becomes unresponsive during audio gaps

  // Block until we get the full chunk - partial reads cause audio corruption
  i2s_read(I2S_PORT, audio_raw_state.getRawSamples(), bytes_expected, &bytes_read, portMAX_DELAY);

  // Validate we got a complete read (should always be true with portMAX_DELAY)
  if (bytes_read != bytes_expected) {
    USBSerial.printf("WARNING: I2S partial read! Got %d bytes, expected %d\n", bytes_read, bytes_expected);
  }

  if (debug_mode && (t_now % 5000 == 0)) {
    USBSerial.print("DEBUG: Bytes read from I2S: ");
    USBSerial.print(bytes_read);
    USBSerial.print(" Max raw value: ");
    USBSerial.println(max_waveform_val_raw);
  }

  max_waveform_val = 0.0;
  max_waveform_val_raw = 0.0;
  // Phase 2A: Replace waveform_history_index with AudioRawState method
  audio_raw_state.advanceHistoryIndex();

  for (uint16_t i = 0; i < CONFIG.SAMPLES_PER_CHUNK; i++) {
    // MODIFICATION [2025-09-20 23:30] - PUNCH-RESTORE-001: Fix I2S audio scaling corruption
    // FAULT DETECTED: Non-linear scaling with magic constants causing signal chaos
    // ROOT CAUSE: Complex scaling formula creating erratic waveform spikes (-27K to +27K)
    // SOLUTION RATIONALE: Clean, deterministic linear scaling path
    // IMPACT ASSESSMENT: Restores stable audio signal processing
    // VALIDATION METHOD: Monitor waveform samples for stability
    // ROLLBACK PROCEDURE: Restore original complex scaling if issues arise

    #ifdef ARDUINO_ESP32S3_DEV
    // MODIFICATION [2025-09-20 23:50] - CRITICAL-FIX-004: Fix DC offset application order
    // FAULT DETECTED: DC offset subtraction before sensitivity causes scaling issues
    // ROOT CAUSE: Order of operations amplifies DC offset error
    // SOLUTION RATIONALE: Apply sensitivity first, then remove scaled DC offset
    // IMPACT ASSESSMENT: Proper DC removal without affecting signal amplitude
    // VALIDATION METHOD: Check waveform samples centered around 0 instead of -300
    // ROLLBACK PROCEDURE: Restore original order if waveform becomes unstable
    int32_t sample = (int32_t)(audio_raw_state.getRawSamples()[i] >> 14);
    sample = (int32_t)(sample * CONFIG.SENSITIVITY); // Apply gain first
    sample -= (int32_t)(CONFIG.DC_OFFSET * CONFIG.SENSITIVITY); // Then remove scaled DC offset
    #else
    // S2: Original calculation (preserved for compatibility)
    int32_t sample = (audio_raw_state.getRawSamples()[i] * 0.000512) + 56000 - 5120;
    sample = sample >> 2;  // Helps prevent overflow in fixed-point math coming up
    sample -= CONFIG.DC_OFFSET;  // Remove DC offset ONCE
    sample *= CONFIG.SENSITIVITY; // Apply sensitivity gain
    #endif

    if (sample > 32767) {
      sample = 32767;
    } else if (sample < -32767) {
      sample = -32767;
    }

    waveform[i] = sample;
    // Phase 2A: Replace waveform_history with AudioRawState buffer
    audio_raw_state.getCurrentHistoryFrame()[i] = waveform[i];

    uint32_t sample_abs = abs(sample);
    if (sample_abs > max_waveform_val_raw) {
      max_waveform_val_raw = sample_abs;
    }
  }

  // MODIFICATION [2025-09-20 23:30] - PUNCH-RESTORE-004: Remove peak smoothing compression
  // FAULT DETECTED: Peak EMA smoothing flattens transients and reduces responsiveness
  // ROOT CAUSE: 0.2 smoothing factor prevents kick/snare pops from registering
  // SOLUTION RATIONALE: Use raw peak values for immediate transient response
  // IMPACT ASSESSMENT: Restores punch and dynamic response to drums/percussion
  // VALIDATION METHOD: Test with drum-heavy tracks for transient response
  // ROLLBACK PROCEDURE: Restore smoothing if peak values become too noisy
  max_waveform_val_raw_smooth = max_waveform_val_raw; // Use raw peak directly

  if (stream_audio) {
    USBSerial.print("sbs((audio=");
    for (uint16_t i = 0; i < CONFIG.SAMPLES_PER_CHUNK; i++) {
      USBSerial.print(waveform[i]);
      if (i < CONFIG.SAMPLES_PER_CHUNK - 1) {
        USBSerial.print(',');
      }
    }
    USBSerial.println("))");
  }

  if (!noise_complete) {
    // Calculate DC offset from raw sample, not processed waveform
    #ifdef ARDUINO_ESP32S3_DEV
    // For S3, use the raw sample before any processing
    audio_raw_state.getDCOffsetSum() += (audio_raw_state.getRawSamples()[0] >> 14);
    #else
    // For S2, apply the initial conversion but not sensitivity/DC removal
    int32_t raw_for_dc = (audio_raw_state.getRawSamples()[0] * 0.000512) + 56000 - 5120;
    audio_raw_state.getDCOffsetSum() += (raw_for_dc >> 2);
    #endif
    silent_scale = 1.0;  // Force LEDs on during calibration

    if (noise_iterations >= 64 && noise_iterations <= 192) {            // sample in the middle of noise cal
      if (max_waveform_val_raw * 1.10 > CONFIG.SWEET_SPOT_MIN_LEVEL) {  // Sweet Spot Min threshold should be the silence level + 15%
        CONFIG.SWEET_SPOT_MIN_LEVEL = max_waveform_val_raw * 1.10;
      }
    }
  } else {
    // Pre-calculate thresholds used multiple times
    float threshold_loud_break = CONFIG.SWEET_SPOT_MIN_LEVEL * 1.20;
    float dynamic_agc_floor_raw = float(min_silent_level_tracker);
    if (dynamic_agc_floor_raw < AGC_FLOOR_MIN_CLAMP_RAW) dynamic_agc_floor_raw = AGC_FLOOR_MIN_CLAMP_RAW;
    if (dynamic_agc_floor_raw > AGC_FLOOR_MAX_CLAMP_RAW) dynamic_agc_floor_raw = AGC_FLOOR_MAX_CLAMP_RAW;
    float dynamic_agc_floor_scaled = dynamic_agc_floor_raw * AGC_FLOOR_SCALING_FACTOR;
    if (dynamic_agc_floor_scaled < AGC_FLOOR_MIN_CLAMP_SCALED) dynamic_agc_floor_scaled = AGC_FLOOR_MIN_CLAMP_SCALED;
    if (dynamic_agc_floor_scaled > AGC_FLOOR_MAX_CLAMP_SCALED) dynamic_agc_floor_scaled = AGC_FLOOR_MAX_CLAMP_SCALED;
    float threshold_silence = dynamic_agc_floor_scaled;

    max_waveform_val = (max_waveform_val_raw - (CONFIG.SWEET_SPOT_MIN_LEVEL));

    if (max_waveform_val > max_waveform_val_follower) {
      float delta = max_waveform_val - max_waveform_val_follower;
      max_waveform_val_follower += delta * 0.4;  // Increased from 0.25 for faster response
    } else if (max_waveform_val < max_waveform_val_follower) {
      float delta = max_waveform_val_follower - max_waveform_val;
      max_waveform_val_follower -= delta * 0.02;  // Increased from 0.005 for faster decay

      if (max_waveform_val_follower < CONFIG.SWEET_SPOT_MIN_LEVEL) {
        max_waveform_val_follower = CONFIG.SWEET_SPOT_MIN_LEVEL;
      }
    }
    float waveform_peak_scaled_raw = max_waveform_val / max_waveform_val_follower;

    if (waveform_peak_scaled_raw > waveform_peak_scaled) {
      float delta = waveform_peak_scaled_raw - waveform_peak_scaled;
      waveform_peak_scaled += delta * 0.5;  // Increased from 0.25 for faster attack
    } else if (waveform_peak_scaled_raw < waveform_peak_scaled) {
      float delta = waveform_peak_scaled - waveform_peak_scaled_raw;
      waveform_peak_scaled -= delta * 0.5;  // Increased from 0.25 for faster decay
    }

    // Use the maximum amplitude of the captured frame to set
    // the Sweet Spot state. Think of this like a coordinate
    // space where 0 is the center LED, -1 is the left, and
    // +1 is the right. See run_sweet_spot() in led_utilities.h
    // for how this value translates to the final LED brightnesses

    int8_t potential_next_state = sweet_spot_state; // Assume current state initially

    // *** Use the SMOOTHED value for state decision ***
    if (max_waveform_val_raw_smooth <= threshold_silence) { // Use pre-calculated threshold
        potential_next_state = -1;
    } else if (max_waveform_val_raw_smooth >= CONFIG.SWEET_SPOT_MAX_LEVEL) {
        potential_next_state = 1;
    } else {
        potential_next_state = 0;
    }

    if (potential_next_state != sweet_spot_state) {
        if ((t_now - last_state_change_time) > MIN_STATE_DURATION_MS) {
            int8_t previous_state = sweet_spot_state;
            sweet_spot_state = potential_next_state;
            last_state_change_time = t_now;

            if (sweet_spot_state == -1) {
                silence_temp = true;
                silence_switched = t_now;

                if (previous_state != -1) {
                     // *** Use RAW value for deadband check ***
                     float agc_delta = threshold_silence - max_waveform_val_raw; // Use pre-calculated threshold
                     if (agc_delta > 50.0) {
                         min_silent_level_tracker = SQ15x16(AGC_FLOOR_INITIAL_RESET);
                         if (debug_mode) {
                             USBSerial.print("DEBUG: AGC Floor Tracker Reset (deadband met): raw_val=");
                             USBSerial.print(max_waveform_val_raw);
                             USBSerial.print(" threshold=");
                             USBSerial.println(threshold_silence); // Use pre-calculated threshold
                         }
                     } else {
                         if (debug_mode) {
                             USBSerial.print("DEBUG: AGC Floor Tracker not reset due to deadband, delta=");
                             USBSerial.println(agc_delta);
                         }
                     }
                }

                if (debug_mode) {
                    USBSerial.println("DEBUG: Entered silent state (Hysteresis Passed)");
                    USBSerial.print("  max_waveform_val_raw: "); USBSerial.print(max_waveform_val_raw);
                    USBSerial.print("  MIN_LEVEL threshold: "); USBSerial.println(threshold_silence); // Use pre-calculated threshold
                }
            } else {
                if (debug_mode) {
                   USBSerial.print("DEBUG: Entered ");
                   USBSerial.print(sweet_spot_state == 1 ? "loud" : "normal");
                   USBSerial.print(" state (Hysteresis Passed), delta=");
                   USBSerial.println(max_waveform_val_raw - threshold_silence); // Use pre-calculated threshold
                }
            }
        }
    }

    if (sweet_spot_state == -1) {
        SQ15x16 current_raw_level = SQ15x16(max_waveform_val_raw);
        if (current_raw_level < min_silent_level_tracker) {
            min_silent_level_tracker = current_raw_level;
        } else {
            min_silent_level_tracker += SQ15x16(AGC_FLOOR_RECOVERY_RATE);
            min_silent_level_tracker = fmin_fixed(min_silent_level_tracker, SQ15x16(AGC_FLOOR_INITIAL_RESET));
        }
         if (debug_mode && (t_now % 1000 == 0)) {
             USBSerial.print("DEBUG (Silence): AGC Floor Tracker Value: "); USBSerial.println(float(min_silent_level_tracker));
         }
    }

    // *** Use RAW value for loud sound detection ***
    bool loud_sound_detected = (max_waveform_val_raw > threshold_loud_break); // Use pre-calculated threshold

    if (loud_sound_detected) {
        if (silence && debug_mode) {
             USBSerial.println("DEBUG: Silence broken by loud sound");
        }
        silence = false;
        silence_temp = false;
        silence_switched = t_now;
    } else if (sweet_spot_state == -1) {
         silence_temp = true;
         // MODIFICATION [2025-09-20 23:30] - PUNCH-RESTORE-002: Shorten silence latch timing
         // FAULT DETECTED: 10s silence latch keeps system "dead" too long
         // ROOT CAUSE: Long hysteresis prevents response to soft intros/outros
         // SOLUTION RATIONALE: Reduce to 1.5s for better musical responsiveness
         // IMPACT ASSESSMENT: Faster visual response to quiet musical passages
         // VALIDATION METHOD: Test with soft intro tracks
         // ROLLBACK PROCEDURE: Restore 10000ms if false triggering occurs
         if (t_now - silence_switched >= 1500) {
            if (!silence && debug_mode) {
                USBSerial.println("DEBUG: Extended silence detected (1.5s)");
            }
            silence = true;
         }
    } else {
        silence = false;
        silence_temp = false;
    }

    if (debug_mode && (t_now % 10000 == 0)) {
      USBSerial.print("DEBUG: silent_scale=");
      USBSerial.print(float(silent_scale));
      USBSerial.print(" silence=");
      USBSerial.print(silence ? "true" : "false");
      USBSerial.print(" sweet_spot_state=");
      USBSerial.println(sweet_spot_state);
    }

    if (CONFIG.STANDBY_DIMMING) {
      float silent_scale_raw = silence ? 0.0 : 1.0;
      silent_scale = silent_scale_raw * 0.1 + silent_scale_last * 0.9;
      silent_scale_last = silent_scale;
    } else {
      silent_scale = 1.0;
    }

    for (int i = 0; i < SAMPLE_HISTORY_LENGTH - CONFIG.SAMPLES_PER_CHUNK; i++) {
      sample_window[i] = sample_window[i + CONFIG.SAMPLES_PER_CHUNK];
    }
    for (int i = SAMPLE_HISTORY_LENGTH - CONFIG.SAMPLES_PER_CHUNK; i < SAMPLE_HISTORY_LENGTH; i++) {
      sample_window[i] = waveform[i - (SAMPLE_HISTORY_LENGTH - CONFIG.SAMPLES_PER_CHUNK)];
    }

    // Pre-calculate reciprocal for fixed-point conversion
    const SQ15x16 RECIP_32768 = SQ15x16(1.0 / 32768.0);
    for (uint16_t i = 0; i < CONFIG.SAMPLES_PER_CHUNK; i++) {
      // Convert using multiplication instead of division
      waveform_fixed_point[i] = SQ15x16(waveform[i]) * RECIP_32768;
    }

    sweet_spot_state_last = sweet_spot_state;

    if (debug_mode && (t_now % 2000 == 0)) {
        USBSerial.print("DEBUG (State): sweet_spot_state="); USBSerial.print(sweet_spot_state);
        USBSerial.print(" | max_waveform_val_raw="); USBSerial.print(max_waveform_val_raw);
        USBSerial.print(" | silence_threshold="); USBSerial.println(threshold_silence); // Use pre-calculated threshold
    }
  }
}

void calculate_vu() {
  /*
    Calculates perceived audio loudness or Volume Unit (VU). Uses root mean square (RMS) method 
    for accurate representation of perceived loudness and incorporates a noise floor calibration.
    If calibration is active, updates noise floor level. If not, subtracts the noise floor from
    the calculated volume and normalizes the volume level.

    Parameters:
    - audio_samples[]: Audio samples to process.
    - sample_count: Number of samples in audio_samples array.

    Global variables:
    - audio_vu_level: Current VU level.
    - audio_vu_level_last: Last calculated VU level.
    - CONFIG.VU_LEVEL_FLOOR: Quietest level considered as audio signal.
    - audio_vu_level_average: Average of the current and the last VU level.
    - noise_cal_active: Indicator of active noise floor calibration.
    */

  // Store last volume level
  audio_vu_level_last = audio_vu_level;

  float sum = 0.0;

  for (uint16_t i = 0; i < CONFIG.SAMPLES_PER_CHUNK; i++) {
    sum += float(waveform_fixed_point[i] * waveform_fixed_point[i]);
  }

  // MODIFICATION [2025-09-20 23:45] - CRITICAL-FIX-003: Apply gain to compensate for fixed-point scaling
  // FAULT DETECTED: RMS values too small (0.007) after waveform_fixed_point division by 32768
  // ROOT CAUSE: Double normalization - waveform already scaled, then divided again
  // SOLUTION RATIONALE: Apply 10x gain to bring RMS into useful range (0.07 typical)
  // IMPACT ASSESSMENT: VU levels will show proper amplitude for visualization
  // VALIDATION METHOD: Monitor typical music shows VU of 0.05-0.2 instead of 0.007
  // ROLLBACK PROCEDURE: Reduce gain if VU levels clip above 1.0
  SQ15x16 rms = sqrt(float(sum / CONFIG.SAMPLES_PER_CHUNK));
  audio_vu_level = rms * SQ15x16(10.0);  // Apply gain to compensate for scaling losses

  // MODIFICATION [2025-09-20 23:45] - CRITICAL-FIX-001: Fix noise floor calibration killing all audio
  // FAULT DETECTED: Noise floor set during calibration (0.007 * 1.1 = 0.0077) blocks all normal audio
  // ROOT CAUSE: VU floor learned from silence is higher than actual music RMS values
  // SOLUTION RATIONALE: Use 1.05x multiplier for tighter tolerance and cap maximum floor at 0.002
  // IMPACT ASSESSMENT: System will properly detect audio above ambient noise
  // VALIDATION METHOD: Monitor VU levels > 0.00 during music playback
  // ROLLBACK PROCEDURE: Restore 1.1x if background noise becomes audible
  if (!noise_complete) {
    if (float(audio_vu_level * 1.05) > CONFIG.VU_LEVEL_FLOOR) {  // Tighter 1.05x multiplier
      float old_floor = CONFIG.VU_LEVEL_FLOOR;
      CONFIG.VU_LEVEL_FLOOR = float(audio_vu_level * 1.05);  // Was 1.5x, then 1.1x, now 1.05x
      // CRITICAL: Cap the floor to prevent it from blocking real audio
      if (CONFIG.VU_LEVEL_FLOOR > 0.002f) {
        CONFIG.VU_LEVEL_FLOOR = 0.002f;  // Maximum floor to preserve audio sensitivity
      }
      USBSerial.printf("NOISE_CAL: Updated VU floor from %.4f to %.4f (rms=%.4f)\n",
                       old_floor, CONFIG.VU_LEVEL_FLOOR, float(audio_vu_level));
    }
  } else {
    // MODIFICATION [2025-09-20 22:45] - SURGICAL-FIX-003: Fix VU level calculation stuck at 0.00
    // FAULT DETECTED: Double subtraction of VU_LEVEL_FLOOR causing always-negative result
    // ROOT CAUSE: Line 380 subtracts floor, then line 394 subtracts it again, always yielding 0.0f
    // SOLUTION RATIONALE: Remove redundant second subtraction, keep single clean floor removal
    // IMPACT ASSESSMENT: VU levels will show proper audio amplitude above noise floor
    // VALIDATION METHOD: Monitor VU levels > 0.00 during audio playback
    // ROLLBACK PROCEDURE: Restore double subtraction if VU scaling issues arise

    // Subtract noise floor once only
    audio_vu_level -= CONFIG.VU_LEVEL_FLOOR;

    // DEBUG: Log VU calculation details using organized debug manager
    DBG_AUDIO_VU(float(rms), CONFIG.VU_LEVEL_FLOOR, float(audio_vu_level));

    // MODIFICATION [2025-09-20 23:45] - CRITICAL-FIX-002: Clamp VU floor to audio-friendly range
    // FAULT DETECTED: Old clamp at 0.95 was way too high, blocking all audio
    // ROOT CAUSE: Incorrect maximum threshold allowed floor to grow beyond useful range
    // SOLUTION RATIONALE: Cap at 0.002 to ensure music (typically 0.005-0.05) passes through
    // IMPACT ASSESSMENT: Prevents VU floor from ever blocking legitimate audio signals
    // VALIDATION METHOD: Verify CONFIG.VU_LEVEL_FLOOR stays below 0.002
    // ROLLBACK PROCEDURE: Increase to 0.005 if too much noise passes through
    CONFIG.VU_LEVEL_FLOOR = min(0.002f, CONFIG.VU_LEVEL_FLOOR);  // Was 0.95f(!), now 0.002f

    // Ensure VU level never goes negative
    if (audio_vu_level < 0.0) {
      audio_vu_level = 0.0;
    }
  }

  audio_vu_level_average = (audio_vu_level + audio_vu_level_last) / (2.0);
}
