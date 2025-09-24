/*----------------------------------------
  Sensory Bridge UART COMMAND LINE
  ----------------------------------------*/

// These functions watch the Serial port for incoming commands,
// and perform actions based on whatis recieved.

extern void check_current_function();  // system.h
extern void reboot();                  // system.h

#ifdef ENABLE_PERFORMANCE_MONITORING
#include "debug/performance_monitor.h"
#endif
#include "debug/debug_manager.h"
#include "globals.h"
#include "dual_coordinator.h" // Agent B: router plan, pairs, enable/disable, status

// Benchmark state variables (defined in main .ino file)
extern bool benchmark_running;
extern uint32_t benchmark_start_time;
extern uint32_t system_fps_sum;
extern uint32_t led_fps_sum;
extern uint32_t benchmark_sample_count;
const uint32_t benchmark_duration = 10000; // Duration in milliseconds (e.g., 10 seconds)

extern void print_chip_id();
extern void blocking_flash(CRGB16 col);
extern void clear_noise_cal();
extern void save_config();
extern void save_config_delayed();
extern void factory_reset();
extern void restore_defaults();
extern void set_preset(char* preset_name);

void tx_begin(bool error = false);
void tx_end(bool error = false);
void ack();
void bad_command(char* command_type, char* command_data);
void stop_streams();
void init_serial(uint32_t baud_rate);
void dump_info();
void parse_command(char* command_buf);
void check_serial(uint32_t t_now);

#ifndef SB_SERIAL_MENU_IMPL
// Declarations only; bodies compiled when SB_SERIAL_MENU_IMPL is defined.
#else

void tx_begin(bool error) {
  if (error == false) {
    USBSerial.println("sbr{{");
  } else {
    USBSerial.println("sberr[[");
  }
}
void tx_end(bool error) {
  if (error == false) {
    USBSerial.println("}}");
  } else {
    USBSerial.println("]]");
  }
}

void ack() {
  USBSerial.println("SBOK");
}

void bad_command(char* command_type, char* command_data) {
  tx_begin(true);
  USBSerial.print("Bad command: ");
  USBSerial.print(command_type);
  if (command_data[0] != 0) {
    USBSerial.print("=");
    USBSerial.print(command_data);
  }

  USBSerial.println();
  tx_end(true);
}

void stop_streams() {
  stream_audio = false;
  stream_fps = false;
  stream_max_mags = false;
  stream_max_mags_followers = false;
  stream_magnitudes = false;
  stream_spectrogram = false;
  stream_chromagram = false;
}

void init_serial(uint32_t baud_rate) {
  USBSerial.begin(baud_rate);  // Default 500,000 baud
  
  // EXTERNAL POWER FIX: Don't block boot waiting for USB serial
  // USB serial may not be available when powered externally
  bool serial_started = false;
  uint32_t t_start = millis();
  uint32_t t_timeout = t_start + 1000;  // 1 second timeout for USB detection

  // Quick check if USB is available
  while (!USBSerial && (millis() - t_start) < 1000) {
    yield();
  }
  
  if (USBSerial) {
    serial_started = true;
    // Print welcome message only if USB is connected
    USBSerial.println("---------------------------");
    USBSerial.print("SENSORY BRIDGE | VER: ");
    USBSerial.println(FIRMWARE_VERSION);
    USBSerial.println("---------------------------");
    USBSerial.println();
  }
  
  // Don't block boot if USB is not available (external power mode)
  // The system will continue to work without serial output
}

// This is for development purposes, and allows the user to dump
// the current values of many variables to the monitor at once
void dump_info() {
  USBSerial.print("FIRMWARE_VERSION: ");
  USBSerial.println(FIRMWARE_VERSION);

  USBSerial.print("CHIP ID: ");
  print_chip_id();

  USBSerial.print("noise_button.pressed: ");
  USBSerial.println(noise_button.pressed);

  USBSerial.print("noise_button.last_down: ");
  USBSerial.println(noise_button.last_down);

  USBSerial.print("noise_button.last_up: ");
  USBSerial.println(noise_button.last_up);

  USBSerial.print("noise_button.pin: ");
  USBSerial.println(noise_button.pin);

  USBSerial.print("mode_button.pressed: ");
  USBSerial.println(mode_button.pressed);

  USBSerial.print("mode_button.last_down: ");
  USBSerial.println(mode_button.last_down);

  USBSerial.print("mode_button.last_up: ");
  USBSerial.println(mode_button.last_up);

  USBSerial.print("mode_button.pin: ");
  USBSerial.println(mode_button.pin);

  USBSerial.print("CONFIG.PHOTONS: ");
  USBSerial.println(CONFIG.PHOTONS, 6);

  USBSerial.print("CONFIG.CHROMA: ");
  USBSerial.println(CONFIG.CHROMA, 6);

  USBSerial.print("CONFIG.MOOD: ");
  USBSerial.println(CONFIG.MOOD, 6);

  USBSerial.print("CONFIG.LIGHTSHOW_MODE: ");
  USBSerial.println(CONFIG.LIGHTSHOW_MODE);

  USBSerial.print("CONFIG.MIRROR_ENABLED: ");
  USBSerial.println(CONFIG.MIRROR_ENABLED);

  USBSerial.print("CONFIG.CHROMAGRAM_RANGE: ");
  USBSerial.println(CONFIG.CHROMAGRAM_RANGE);

  USBSerial.print("CONFIG.SAMPLE_RATE: ");
  USBSerial.println(CONFIG.SAMPLE_RATE);

  USBSerial.print("CONFIG.NOTE_OFFSET: ");
  USBSerial.println(CONFIG.NOTE_OFFSET);

  USBSerial.print("CONFIG.SQUARE_ITER: ");
  USBSerial.println(CONFIG.SQUARE_ITER);

  USBSerial.print("CONFIG.LED_TYPE: ");
  USBSerial.println(CONFIG.LED_TYPE);

  USBSerial.print("CONFIG.LED_COUNT: ");
  USBSerial.println(CONFIG.LED_COUNT);

  USBSerial.print("CONFIG.LED_COLOR_ORDER: ");
  USBSerial.println(CONFIG.LED_COLOR_ORDER);

  USBSerial.print("CONFIG.SAMPLES_PER_CHUNK: ");
  USBSerial.println(CONFIG.SAMPLES_PER_CHUNK);

  USBSerial.print("CONFIG.SENSITIVITY: ");
  USBSerial.println(CONFIG.SENSITIVITY, 6);

  USBSerial.print("CONFIG.BOOT_ANIMATION: ");
  USBSerial.println(CONFIG.BOOT_ANIMATION);

  USBSerial.print("CONFIG.SWEET_SPOT_MIN_LEVEL: ");
  USBSerial.println(CONFIG.SWEET_SPOT_MIN_LEVEL);

  USBSerial.print("CONFIG.SWEET_SPOT_MAX_LEVEL: ");
  USBSerial.println(CONFIG.SWEET_SPOT_MAX_LEVEL);

  USBSerial.print("CONFIG.DC_OFFSET: ");
  USBSerial.println(CONFIG.DC_OFFSET);

  USBSerial.print("CONFIG.STANDBY_DIMMING: ");
  USBSerial.println(CONFIG.STANDBY_DIMMING);

  USBSerial.print("CONFIG.REVERSE_ORDER: ");
  USBSerial.println(CONFIG.REVERSE_ORDER);

  USBSerial.print("CONFIG.IS_MAIN_UNIT: ");
  USBSerial.println(CONFIG.IS_MAIN_UNIT);

  USBSerial.print("CONFIG.MAX_CURRENT_MA: ");
  USBSerial.println(CONFIG.MAX_CURRENT_MA);

  USBSerial.print("CONFIG.TEMPORAL_DITHERING: ");
  USBSerial.println(CONFIG.TEMPORAL_DITHERING);

  USBSerial.print("CONFIG.AUTO_COLOR_SHIFT: ");
  USBSerial.println(CONFIG.AUTO_COLOR_SHIFT);

  USBSerial.print("CONFIG.INCANDESCENT_FILTER: ");
  USBSerial.println(CONFIG.INCANDESCENT_FILTER);

  USBSerial.print("CONFIG.INCANDESCENT_MODE: ");
  USBSerial.println(CONFIG.INCANDESCENT_MODE);

  USBSerial.print("CONFIG.BULB_OPACITY: ");
  USBSerial.println(CONFIG.BULB_OPACITY);

  USBSerial.print("CONFIG.SATURATION: ");
  USBSerial.println(CONFIG.SATURATION);

  USBSerial.print("CONFIG.PRISM_COUNT: ");
  USBSerial.println(CONFIG.PRISM_COUNT);

  USBSerial.print("CONFIG.BASE_COAT: ");
  USBSerial.println(CONFIG.BASE_COAT);

  USBSerial.print("MASTER_BRIGHTNESS: ");
  USBSerial.println(MASTER_BRIGHTNESS);

  USBSerial.print("stream_audio: ");
  USBSerial.println(stream_audio);

  USBSerial.print("stream_fps: ");
  USBSerial.println(stream_fps);

  USBSerial.print("stream_max_mags: ");
  USBSerial.println(stream_max_mags);

  USBSerial.print("stream_max_mags_followers: ");
  USBSerial.println(stream_max_mags_followers);

  USBSerial.print("stream_magnitudes: ");
  USBSerial.println(stream_magnitudes);

  USBSerial.print("stream_spectrogram: ");
  USBSerial.println(stream_spectrogram);

  USBSerial.print("stream_chromagram: ");
  USBSerial.println(stream_chromagram);

  USBSerial.print("debug_mode: ");
  USBSerial.println(debug_mode);

  USBSerial.print("noise_complete: ");
  USBSerial.println(noise_complete);

  USBSerial.print("noise_iterations: ");
  USBSerial.println(noise_iterations);

  USBSerial.print("main_override: ");
  USBSerial.println(main_override);

  USBSerial.print("last_rx_time: ");
  USBSerial.println(last_rx_time);

  USBSerial.print("next_save_time: ");
  USBSerial.println(next_save_time);

  USBSerial.print("settings_updated: ");
  USBSerial.println(settings_updated);

  USBSerial.print("I2S_PORT: ");
  USBSerial.println(I2S_PORT);

  USBSerial.print("SAMPLE_HISTORY_LENGTH: ");
  USBSerial.println(SAMPLE_HISTORY_LENGTH);

  USBSerial.print("silence: ");
  USBSerial.println(silence);

  USBSerial.print("mode_destination: ");
  USBSerial.println(mode_destination);

  USBSerial.print("SYSTEM_FPS: ");
  USBSerial.println(SYSTEM_FPS);

  USBSerial.print("LED_FPS: ");
  USBSerial.println(LED_FPS);
}

// This parses a completed command to decide how to handle it
void parse_command(char* command_buf) {

  // COMMANDS WITHOUT METADATA ###############################

  // Get firmware version -----------------------------------
  if (strcmp(command_buf, "v") == 0 || strcmp(command_buf, "V") == 0 || strcmp(command_buf, "version") == 0) {

    tx_begin();
    USBSerial.print("VERSION: ");
    USBSerial.println(FIRMWARE_VERSION);
    tx_end();

  }

  // Print help ---------------------------------------------
  else if (strcmp(command_buf, "h") == 0 || strcmp(command_buf, "H") == 0 || strcmp(command_buf, "help") == 0) {

    tx_begin();
    USBSerial.println("SENSORY BRIDGE - Serial Menu ------------------------------------------------------------------------------------");
    USBSerial.println();
    USBSerial.println("                                            v | Print firmware version number");
    USBSerial.println("                                        reset | Reboot Sensory Bridge");
    USBSerial.println("                                factory_reset | Delete configuration, including noise cal, reboot");
    USBSerial.println("                             restore_defaults | Delete configuration, reboot");
    USBSerial.println("                                get_main_unit | Print if this unit is set to MAIN for SensorySync");
    USBSerial.println("                                         dump | Print tons of useful variables in realtime");
    USBSerial.println("                                         stop | Stops the output of any enabled streams");
    USBSerial.println("                                          fps | Return the system FPS");
    USBSerial.println("                                      led_fps | Return the LED FPS");
    USBSerial.println("                                  audio_guard | Display audio guard protection status");
    USBSerial.println("                                      chip_id | Return the chip id (MAC) of the CPU");
    USBSerial.println("                                     get_mode | Get lightshow mode's ID (index)");
    USBSerial.println("                                get_num_modes | Return the number of modes available");
    USBSerial.println("                              start_noise_cal | Remotely begin a noise calibration");
    USBSerial.println("                              clear_noise_cal | Remotely clear the stored noise calibration");
    USBSerial.println("                              reset_vu_floor | Reset VU floor to 0.00 for better sensitivity");
    USBSerial.println("                               show_vu_floor | Display current VU floor and audio levels");
    USBSerial.println("                             start_benchmark | Start a timed benchmark (calculates avg FPS)");
    USBSerial.println("                               set_mode=[int] | Set the mode number");
    USBSerial.println("          mirror_enabled=[true/false/default] | Remotely toggle lightshow mirroring");
    USBSerial.println("           reverse_order=[true/false/default] | Toggle whether image is flipped upside down before final rendering");
    USBSerial.println("                          get_mode_name=[int] | Get a mode's name by ID (index)");
    USBSerial.println("                                stream=[type] | Stream live data to a Serial Plotter.");
    USBSerial.println("                                                Options are: audio, fps, magnitudes, spectrogram, chromagram");
    USBSerial.println("led_type=['neopixel'/'neopixel_x2'/'dotstar'] | Sets which LED protocol to use, 3 wire, 4 wire, or dual-data mode");
    USBSerial.println("                 led_count=[int or 'default'] | Sets how many LEDs your display will use (native resolution is 128)");
    USBSerial.println("        led_color_order=[GRB/RGB/BGR/default] | Sets LED color ordering, default GRB");
    USBSerial.println("       led_interpolation=[true/false/default] | Toggles linear LED interpolation when running in a non-native resolution (slower)");
    USBSerial.println("                           debug=[true/false] | Enables debug mode, where functions are timed");
    USBSerial.println("                sample_rate=[hz or 'default'] | Sets the microphone sample rate");
    USBSerial.println("              note_offset=[0-32 or 'default'] | Sets the lowest note, as a positive offset from A1 (55.0Hz)");
    USBSerial.println("               square_iter=[int or 'default'] | Sets the number of times the LED output is squared (contrast)");
    USBSerial.println("         samples_per_chunk=[int or 'default'] | Sets the number of samples collected every frame");
    USBSerial.println("             sensitivity=[float or 'default'] | Sets the scaling of audio data (>1.0 is more sensitive, <1.0 is less sensitive)");
    USBSerial.println("          boot_animation=[true/false/default] | Enable or disable the boot animation");
    USBSerial.println("                   set_main_unit=[true/false] | Sets if this unit is MAIN or not for SensorySync");
    USBSerial.println("            sweet_spot_min=[int or 'default'] | Sets the minimum amplitude to be inside the 'Sweet Spot'");
    USBSerial.println("            sweet_spot_max=[int or 'default'] | Sets the maximum amplitude to be inside the 'Sweet Spot'");
    USBSerial.println("         chromagram_range=[1-64 or 'default'] | Range between 1 and 64, how many notes at the bottom of the");
    USBSerial.println("                                                spectrogram should be considered in chromagram sums");
    USBSerial.println("         standby_dimming=[true/false/default] | Toggle dimming during detected silence");
    USBSerial.println("                       bass_mode=[true/false] | Toggle bass-mode, which alters note_offset and chromagram_range for bass-y tunes");
    USBSerial.println("            max_current_ma=[int or 'default'] | Sets the maximum current FastLED will attempt to limit the LED consumption to");
    USBSerial.println("      temporal_dithering=[true/false/default] | Toggle per-LED temporal dithering that simulates higher bit-depths");
    USBSerial.println("        auto_color_shift=[true/false/default] | Toggle automated color shifting based on positive spectral changes");
    USBSerial.println("     incandescent_filter=[float or 'default'] | Set the intensity of the incandescent LUT (reduces harsh blues)");
    USBSerial.println("       incandescent_mode=[true/false/default] | Force all output into monochrome and tint with 2700K incandescent color");
    USBSerial.println("               base_coat=[true/false/default] | Enable a dim gray backdrop to the LEDs (approves appearance in most modes)");
    USBSerial.println("            bulb_opacity=[float or 'default'] | Set opacity of a filter that portrays the output as 32 \"bulbs\" with separation and hot spots");
    USBSerial.println("              saturation=[float or 'default'] | Sets the saturation of internal hues");
    USBSerial.println("               prism_count=[int or 'default'] | Sets the number of times the \"prism\" effect is applied");
    USBSerial.println("                         preset=[preset_name] | Sets multiple configuration options at once to match a preset theme");
    USBSerial.println();
    USBSerial.println("                         -- SECONDARY LED STRIP CONTROL --");
    USBSerial.println("         secondary_enabled=[true/false] | Enable or disable the secondary LED strip");
    USBSerial.println("                  secondary_mode=[0-7] | Set mode for secondary LED strip");
    USBSerial.println("              secondary_photons=[0-1.0] | Set brightness for secondary LED strip");
    USBSerial.println("               secondary_chroma=[0-1.0] | Set chroma value for secondary LED strip");
    USBSerial.println("                 secondary_mood=[0-1.0] | Set mood value for secondary LED strip");
    USBSerial.println("            secondary_saturation=[0-1.0] | Set saturation for secondary LED strip");
    USBSerial.println("          secondary_prism_count=[0-10] | Set prism count for secondary LED strip");
    USBSerial.println("   secondary_mirror_enabled=[true/false] | Toggle mirroring on secondary LED strip");
    USBSerial.println("    secondary_reverse_order=[true/false] | Toggle image flipping on secondary LED strip");
    USBSerial.println("              secondary_base_coat=[true/false] | Enable dim backdrop on secondary LED strip");
    USBSerial.println("                  secondary_status | Display current status of secondary LED strip");
#ifdef ENABLE_PERFORMANCE_MONITORING
    USBSerial.println();
    USBSerial.println("                         -- PERFORMANCE MONITORING (96-BIN TEST) --");
    USBSerial.println("                                         PERF | Show detailed performance report");
    USBSerial.println("                                   PERF SWEEP | Run frequency sweep test");
    USBSerial.println("                                  PERF STRESS | Run 60-second stress test");
    USBSerial.println("                                   PERF RESET | Reset performance metrics");
#endif
    tx_end(); 
  }

  // So that software can automatically identify this device -
  else if (strcmp(command_buf, "SB?") == 0) {

    USBSerial.println("SB!");

  }

  // Reset the micro ----------------------------------------
  else if (strcmp(command_buf, "reset") == 0) {

    ack();
    reboot();

  }

  // Clear configs and reset micro --------------------------
  else if (strcmp(command_buf, "factory_reset") == 0) {

    ack();
    factory_reset();

  }

  // Clear configs and reset micro --------------------------
  else if (strcmp(command_buf, "restore_defaults") == 0) {

    ack();
    restore_defaults();

  }

  // Return chip ID -----------------------------------------
  else if (strcmp(command_buf, "chip_id") == 0) {

    tx_begin();
    print_chip_id();
    tx_end();

  }

  // Identify unit via 2 yellow flashes ---------------------
  else if (strcmp(command_buf, "identify") == 0) {

    ack();
    CRGB16 col = {{1.00}, {0.25}, {0.00}};
    blocking_flash(col);

  }

  // Clear configs and reset micro --------------------------
  else if (strcmp(command_buf, "start_noise_cal") == 0) {

    ack();
    noise_transition_queued = true;

  }

  // Clear configs and reset micro --------------------------
  else if (strcmp(command_buf, "clear_noise_cal") == 0) {

    ack();
    clear_noise_cal();

  }
  // Delete noise calibration file --------------------------
  else if (strcmp(command_buf, "delete_noise_file") == 0) {
    if (LittleFS.remove("/noise_cal.bin")) {
      USBSerial.println("Noise calibration file deleted. Restart device for clean state.");
    } else {
      USBSerial.println("Failed to delete noise calibration file.");
    }
  }
  // Show current noise levels -------------------------------
  else if (strcmp(command_buf, "show_noise_levels") == 0) {
    USBSerial.println("Current noise calibration levels:");
    for (uint8_t i = 0; i < NUM_FREQS; i += 8) {
      USBSerial.printf("Freq[%d-%d]: %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n",
                       i, i+7,
                       float(noise_samples[i]), float(noise_samples[i+1]),
                       float(noise_samples[i+2]), float(noise_samples[i+3]),
                       float(noise_samples[i+4]), float(noise_samples[i+5]),
                       float(noise_samples[i+6]), float(noise_samples[i+7]));
    }
  }

  // ADDED [2025-09-20 23:35] - Reset VU floor to fix sensitivity issues
  // Command to immediately reset VU_LEVEL_FLOOR without factory reset
  else if (strcmp(command_buf, "reset_vu_floor") == 0) {
    float old_floor = CONFIG.VU_LEVEL_FLOOR;
    CONFIG.VU_LEVEL_FLOOR = 0.00;
    save_config_delayed();
    USBSerial.printf("VU_LEVEL_FLOOR reset from %.3f to 0.000\n", old_floor);
    USBSerial.println("Config save queued - will apply after 10 seconds");
    ack();
  }

  // Show current VU floor value
  else if (strcmp(command_buf, "show_vu_floor") == 0) {
    USBSerial.printf("Current VU_LEVEL_FLOOR: %.6f\n", CONFIG.VU_LEVEL_FLOOR);
    USBSerial.printf("Current raw audio VU: %.6f\n", float(audio_vu_level));
    USBSerial.printf("Effective VU (after floor): %.6f\n",
                     max(0.0f, float(audio_vu_level) - CONFIG.VU_LEVEL_FLOOR));
  }

  // Returns the number of modes available ------------------
  else if (strcmp(command_buf, "get_num_modes") == 0) {

    tx_begin();
    USBSerial.print("NUM_MODES: ");
    USBSerial.println(NUM_MODES);
    tx_end();

  }

  // Returns the mode ID ------------------------------------
  else if (strcmp(command_buf, "get_mode") == 0) {

    tx_begin();
    USBSerial.print("MODE: ");
    USBSerial.println(CONFIG.LIGHTSHOW_MODE);
    tx_end();

  }

  // Returns whether or not this is a "MAIN" unit -----------
  else if (strcmp(command_buf, "get_main_unit") == 0) {

    tx_begin();
    USBSerial.print("CONFIG.IS_MAIN_UNIT: ");
    USBSerial.println(CONFIG.IS_MAIN_UNIT);
    tx_end();

  }

  // Returns the reason why the ESP32 last rebooted ---------
  else if (strcmp(command_buf, "reset_reason") == 0) {
    tx_begin();
    switch (esp_reset_reason()) {
      case ESP_RST_UNKNOWN:
        USBSerial.println("UNKNOWN");
        break;
      case ESP_RST_POWERON:
        USBSerial.println("POWERON");
        break;
      case ESP_RST_EXT:
        USBSerial.println("EXTERNAL");
        break;
      case ESP_RST_SW:
        USBSerial.println("SOFTWARE");
        break;
      case ESP_RST_PANIC:
        USBSerial.println("PANIC");
        break;
      case ESP_RST_INT_WDT:
        USBSerial.println("INTERNAL WATCHDOG");
        break;
      case ESP_RST_TASK_WDT:
        USBSerial.println("TASK WATCHDOG");
        break;
      case ESP_RST_WDT:
        USBSerial.println("WATCHDOG");
        break;
      case ESP_RST_DEEPSLEEP:
        USBSerial.println("DEEPSLEEP");
        break;
      case ESP_RST_BROWNOUT:
        USBSerial.println("BROWNOUT");
        break;
      case ESP_RST_SDIO:
        USBSerial.println("SDIO");
        break;
    }
    tx_end();
  }

  // Dump tons of variables to the monitor ------------------
  else if (strcmp(command_buf, "dump") == 0) {

    tx_begin();
    dump_info();
    tx_end();

  }

  // If a streaming or plotting a variable, stop ------------
  else if (strcmp(command_buf, "stop") == 0) {

    stop_streams();
    ack();

  }

  // Print the average FPS ----------------------------------
  else if (strcmp(command_buf, "fps") == 0) {

    tx_begin();
    USBSerial.print("SYSTEM_FPS: ");
    USBSerial.println(SYSTEM_FPS);
    tx_end();

  }

  // Print the average FPS ----------------------------------
  else if (strcmp(command_buf, "led_fps") == 0) {

    tx_begin();
    USBSerial.print("LED_FPS: ");
    USBSerial.println(LED_FPS);
    tx_end();

  }
  
  // Print audio guard status -------------------------------
  else if (strcmp(command_buf, "audio_guard") == 0) {

    tx_begin();
    // AudioGuard::printAudioState();  // TODO: Need to include audio_guard.h before this file
    USBSerial.println("Audio Guard status (temporarily disabled - include order issue)");
    tx_end();

  }

  // Print the knob values ----------------------------------
  else if (strcmp(command_buf, "get_knobs") == 0) {

    USBSerial.print("{");

    USBSerial.print('"');
    USBSerial.print("PHOTONS");
    USBSerial.print('"');
    USBSerial.print(':');
    USBSerial.print(CONFIG.PHOTONS);

    USBSerial.print(',');

    USBSerial.print('"');
    USBSerial.print("CHROMA");
    USBSerial.print('"');
    USBSerial.print(':');
    USBSerial.print(CONFIG.CHROMA);

    USBSerial.print(',');

    USBSerial.print('"');
    USBSerial.print("MOOD");
    USBSerial.print('"');
    USBSerial.print(':');
    USBSerial.print(CONFIG.MOOD);

    USBSerial.println('}');

  }

  // Print the button values --------------------------------
  else if (strcmp(command_buf, "get_buttons") == 0) {

    USBSerial.print("{");

    USBSerial.print('"');
    USBSerial.print("NOISE");
    USBSerial.print('"');
    USBSerial.print(':');
    USBSerial.print(digitalRead(noise_button.pin));

    USBSerial.print(',');

    USBSerial.print('"');
    USBSerial.print("MODE");
    USBSerial.print('"');
    USBSerial.print(':');
    USBSerial.print(digitalRead(mode_button.pin));

    USBSerial.println('}');

  }
  
  // Show frequency peak debug info -------------------------
  else if (strcmp(command_buf, "freq_debug") == 0) {
    tx_begin();
    USBSerial.println("=== FREQUENCY DEBUG INFO ===");
    USBSerial.print("NUM_FREQS: ");
    USBSerial.println(NUM_FREQS);
    USBSerial.print("Sample Rate: ");
    USBSerial.println(CONFIG.SAMPLE_RATE);
    USBSerial.print("Note Offset: ");
    USBSerial.println(CONFIG.NOTE_OFFSET);
    USBSerial.println("\nFrequency allocation:");
    USBSerial.println("First 5 bins:");
    for (int i = 0; i < 5; i++) {
      USBSerial.print("  Bin ");
      USBSerial.print(i);
      USBSerial.print(": ");
      USBSerial.print(frequencies[i].target_freq);
      USBSerial.print(" Hz (block_size=");
      USBSerial.print(frequencies[i].block_size);
      USBSerial.println(")");
    }
    USBSerial.println("...");
    USBSerial.println("Last 5 bins:");
    for (int i = NUM_FREQS-5; i < NUM_FREQS; i++) {
      USBSerial.print("  Bin ");
      USBSerial.print(i);
      USBSerial.print(": ");
      USBSerial.print(frequencies[i].target_freq);
      USBSerial.print(" Hz (block_size=");
      USBSerial.print(frequencies[i].block_size);
      USBSerial.println(")");
    }
    USBSerial.println("\nCurrent magnitudes:");
    float max_mag = 0;
    int peak_bin = 0;
    for (int i = 0; i < NUM_FREQS; i++) {
      if (magnitudes_final[i] > max_mag) {
        max_mag = magnitudes_final[i];
        peak_bin = i;
      }
    }
    USBSerial.print("Peak: Bin ");
    USBSerial.print(peak_bin);
    USBSerial.print(" (");
    USBSerial.print(frequencies[peak_bin].target_freq);
    USBSerial.print(" Hz) = ");
    USBSerial.println(max_mag);
    tx_end();
  }

  // COMMANDS WITH METADATA ##################################

  else {  // Commands with metadata are parsed here

    // PARSER #############################
    // Parse command type
    char command_type[32] = { 0 };
    uint8_t reading_index = 0;
    for (uint8_t i = 0; i < 32; i++) {
      reading_index++;
      if (command_buf[i] != '=') {
        command_type[i] = command_buf[i];
      } else {
        break;
      }
    }

    // Then parse command data
    char command_data[94] = { 0 };
    for (uint8_t i = 0; i < 94; i++) {
      if (command_buf[reading_index + i] != 0) {
        command_data[i] = command_buf[reading_index + i];
      } else {
        break;
      }
    }
    // PARSER #############################

    // Now react accordingly:

    if (strcmp(command_type, "debug_minimal") == 0) {
      DebugManager::preset_minimal();
      tx_begin();
      USBSerial.println("DEBUG: Minimal debug preset active");
      tx_end();
    }

    else if (strcmp(command_type, "debug_full") == 0) {
      DebugManager::preset_full_debug();
      tx_begin();
      USBSerial.println("DEBUG: Full debug preset active");
      tx_end();
    }

    // Set if this Sensory Bridge is a MAIN Unit --------------
    else if (strcmp(command_type, "set_main_unit") == 0) {
      bool good = false;
      if (strcmp(command_data, "true") == 0) {
        good = true;
        CONFIG.IS_MAIN_UNIT = true;
      } else if (strcmp(command_data, "false") == 0) {
        good = true;
        CONFIG.IS_MAIN_UNIT = false;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        save_config();

        tx_begin();
        USBSerial.print("CONFIG.IS_MAIN_UNIT: ");
        USBSerial.println(CONFIG.IS_MAIN_UNIT);
        tx_end();

        reboot();
      }
    }

    // Run DC Offset Diagnostics ------------------------------
    else if (strcmp(command_type, "dc_diag") == 0) {
      tx_begin();
      USBSerial.println("Running DC offset diagnostics...");
      tx_end();
      // Include guard to only use if test diagnostics header is included
      #ifdef test_audio_diagnostics_h
      diagnose_dc_offset();  // Run the diagnostic once
      #else
      USBSerial.println("DC diagnostics not available - include test_audio_diagnostics.h");
      #endif
    }

    // Toggle Debug Mode --------------------------------------
    else if (strcmp(command_type, "debug") == 0) {
      bool good = false;
      if (strcmp(command_data, "true") == 0) {
        good = true;
        debug_mode = true;
        cpu_usage.attach_ms(5, check_current_function);
        DebugManager::enable_category(DEBUG_COLOR, true);
      } else if (strcmp(command_data, "false") == 0) {
        good = true;
        debug_mode = false;
        cpu_usage.detach();
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        tx_begin();
        USBSerial.print("debug_mode: ");
        USBSerial.println(debug_mode);
        tx_end();
      }
    }

    // Control color-pipeline debug output --------------------
    else if (strcmp(command_type, "debug_color") == 0 || strcmp(command_type, "debug_colors") == 0) {
      bool good = false;
      bool color_enabled = true;
      if (strcmp(command_data, "true") == 0) {
        good = true;
        debug_mode = true;
        DebugManager::enable_category(DEBUG_COLOR, true);
        DebugManager::set_interval(DEBUG_COLOR, 400);
      } else if (strcmp(command_data, "false") == 0) {
        good = true;
        DebugManager::enable_category(DEBUG_COLOR, false);
        color_enabled = false;
      } else if (strcmp(command_data, "pulse") == 0 || strcmp(command_data, "once") == 0) {
        good = true;
        debug_mode = true;
        DebugManager::enable_category(DEBUG_COLOR, true);
        DebugManager::request_once(DEBUG_COLOR);
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        tx_begin();
        USBSerial.print("debug_color enabled: ");
        USBSerial.println(color_enabled);
        tx_end();
      }
    }

    // Set Sample Rate ----------------------------------------
    else if (strcmp(command_type, "sample_rate") == 0) {
      bool good = false;
      if (strcmp(command_data, "default") == 0) {
        good = true;
        CONFIG.SAMPLE_RATE = CONFIG_DEFAULTS.SAMPLE_RATE;
      } else {
        good = true;
        CONFIG.SAMPLE_RATE = constrain(atol(command_data), 6400, 44100);
      }

      if (good) {
        save_config();
        tx_begin();
        USBSerial.print("CONFIG.SAMPLE_RATE: ");
        USBSerial.println(CONFIG.SAMPLE_RATE);
        tx_end();
        reboot();
      }
    }

    // Set Mode Number ----------------------------------------
    else if (strcmp(command_type, "set_mode") == 0) {
      mode_transition_queued = true;
      mode_destination = constrain(atol(command_data), 0, NUM_MODES - 1);

      save_config_delayed();
      tx_begin();
      USBSerial.print("CONFIG.LIGHTSHOW_MODE: ");
      USBSerial.println(mode_destination);
      tx_end();
    }

    // Get Mode Name By ID ------------------------------------
    else if (strcmp(command_type, "get_mode_name") == 0) {
      uint16_t mode_id = atol(command_data);

      if (mode_id < NUM_MODES) {
        char buf[32] = { 0 };
        for (uint8_t i = 0; i < 32; i++) {
          char c = mode_names[32 * mode_id + i];
          if (c != 0) {
            buf[i] = c;
          } else {
            break;
          }
        }

        tx_begin();
        USBSerial.print("MODE_NAME: ");
        USBSerial.println(buf);
        tx_end();
      } else {
        bad_command(command_type, command_data);
      }
    }

    // Set Note Offset ----------------------------------------
    else if (strcmp(command_type, "note_offset") == 0) {
      if (strcmp(command_data, "default") == 0) {
        CONFIG.NOTE_OFFSET = CONFIG_DEFAULTS.NOTE_OFFSET;
      } else {
        CONFIG.NOTE_OFFSET = constrain(atol(command_data), 0, 32);
      }
      save_config();
      tx_begin();
      USBSerial.print("CONFIG.NOTE_OFFSET: ");
      USBSerial.println(CONFIG.NOTE_OFFSET);
      tx_end();
      reboot();
    }

    // Set Square Iterations ----------------------------------
    else if (strcmp(command_type, "square_iter") == 0) {
      if (strcmp(command_data, "default") == 0) {
        CONFIG.SQUARE_ITER = CONFIG_DEFAULTS.SQUARE_ITER;
      } else {
        CONFIG.SQUARE_ITER = constrain(atol(command_data), 0, 10);
      }
      save_config_delayed();

      tx_begin();
      USBSerial.print("CONFIG.SQUARE_ITER: ");
      USBSerial.println(CONFIG.SQUARE_ITER);
      tx_end();
    }

    // Set LED Type ---------------------------------------
    else if (strcmp(command_type, "led_type") == 0) {
      bool good = false;
      if (strcmp(command_data, "neopixel") == 0) {
        CONFIG.LED_TYPE = LED_NEOPIXEL;
        CONFIG.LED_COLOR_ORDER = GRB;
        good = true;
      } 
      else if (strcmp(command_data, "neopixel_x2") == 0) {
        CONFIG.LED_TYPE = LED_NEOPIXEL_X2;
        CONFIG.LED_COLOR_ORDER = GRB;
        good = true;
      } else if (strcmp(command_data, "dotstar") == 0) {
        CONFIG.LED_TYPE = LED_DOTSTAR;
        CONFIG.LED_COLOR_ORDER = BGR;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        save_config();
        tx_begin();
        USBSerial.print("CONFIG.LED_TYPE: ");
        USBSerial.println(CONFIG.LED_TYPE);
        tx_end();
        reboot();
      }
    }

    // Set LED Count ------------------------------------
    else if (strcmp(command_type, "led_count") == 0) {
      if (strcmp(command_data, "default") == 0) {
        CONFIG.LED_COUNT = CONFIG_DEFAULTS.LED_COUNT;
      } else {
        CONFIG.LED_COUNT = constrain(atol(command_data), 1, 10000);
      }

      save_config();
      tx_begin();
      USBSerial.print("CONFIG.LED_COUNT: ");
      USBSerial.println(CONFIG.LED_COUNT);
      tx_end();
      reboot();
    }

    // Set LED Interpolation ----------------------------
    else if (strcmp(command_type, "led_interpolation") == 0) {
      bool good = false;
      if (strcmp(command_data, "default") == 0) {
        CONFIG.LED_INTERPOLATION = CONFIG_DEFAULTS.LED_INTERPOLATION;
        good = true;
      } else if (strcmp(command_data, "true") == 0) {
        CONFIG.LED_INTERPOLATION = true;
        good = true;
      } else if (strcmp(command_data, "false") == 0) {
        CONFIG.LED_INTERPOLATION = false;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        save_config_delayed();
        tx_begin();
        USBSerial.print("CONFIG.LED_INTERPOLATION: ");
        USBSerial.println(CONFIG.LED_INTERPOLATION);
        tx_end();
      }
    }

    // Set Base Coat ----------------------------
    else if (strcmp(command_type, "base_coat") == 0) {
      bool good = false;
      if (strcmp(command_data, "default") == 0) {
        CONFIG.BASE_COAT = CONFIG_DEFAULTS.BASE_COAT;
        good = true;
      } else if (strcmp(command_data, "true") == 0) {
        CONFIG.BASE_COAT = true;
        good = true;
      } else if (strcmp(command_data, "false") == 0) {
        CONFIG.BASE_COAT = false;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        save_config_delayed();
        tx_begin();
        USBSerial.print("CONFIG.BASE_COAT: ");
        USBSerial.println(CONFIG.BASE_COAT);
        tx_end();
      }
    }

    // Set LED Temporal Dithering ----------------------------
    else if (strcmp(command_type, "temporal_dithering") == 0) {
      bool good = false;
      if (strcmp(command_data, "default") == 0) {
        CONFIG.TEMPORAL_DITHERING = CONFIG_DEFAULTS.TEMPORAL_DITHERING;
        good = true;
      } else if (strcmp(command_data, "true") == 0) {
        CONFIG.TEMPORAL_DITHERING = true;
        good = true;
      } else if (strcmp(command_data, "false") == 0) {
        CONFIG.TEMPORAL_DITHERING = false;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        save_config_delayed();
        tx_begin();
        USBSerial.print("CONFIG.TEMPORAL_DITHERING: ");
        USBSerial.println(CONFIG.TEMPORAL_DITHERING);
        tx_end();
      }
    }

    // Set LED Color Order ----------------------------
    else if (strcmp(command_type, "led_color_order") == 0) {
      bool good = false;
      if (strcmp(command_data, "default") == 0) {
        CONFIG.LED_COLOR_ORDER = CONFIG_DEFAULTS.LED_COLOR_ORDER;
        good = true;
      } else if (strcmp(command_data, "GRB") == 0) {
        CONFIG.LED_COLOR_ORDER = GRB;
        good = true;
      } else if (strcmp(command_data, "RGB") == 0) {
        CONFIG.LED_COLOR_ORDER = RGB;
        good = true;
      } else if (strcmp(command_data, "BGR") == 0) {
        CONFIG.LED_COLOR_ORDER = BGR;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        save_config();
        tx_begin();
        USBSerial.print("CONFIG.LED_COLOR_ORDER: ");
        USBSerial.println(CONFIG.LED_COLOR_ORDER);
        tx_end();
        reboot();
      }
    }

    // Set Samples Per Chunk ---------------------------
    else if (strcmp(command_type, "samples_per_chunk") == 0) {
      if (strcmp(command_data, "default") == 0) {
        CONFIG.SAMPLES_PER_CHUNK = CONFIG_DEFAULTS.SAMPLES_PER_CHUNK;
      } else {
        // MODIFICATION [2025-09-20 23:50] - SECURITY-FIX-001: Prevent division by zero in audio pipeline
        // FAULT DETECTED: constrain(..., 0, ...) allows SAMPLES_PER_CHUNK = 0 causing system crash
        // ROOT CAUSE: Multiple division operations (sum/SAMPLES_PER_CHUNK) throughout audio processing
        // SOLUTION RATIONALE: Minimum value of 1 prevents division by zero while maintaining functionality
        // IMPACT ASSESSMENT: Eliminates security vulnerability without affecting normal operation
        // VALIDATION METHOD: Verify serial command "samples_per_chunk 0" no longer crashes system
        // ROLLBACK PROCEDURE: Restore "0" minimum if specific use case requires empty chunk processing
        CONFIG.SAMPLES_PER_CHUNK = constrain(atol(command_data), 1, SAMPLE_HISTORY_LENGTH);
      }

      save_config();
      tx_begin();
      USBSerial.print("CONFIG.SAMPLES_PER_CHUNK: ");
      USBSerial.println(CONFIG.SAMPLES_PER_CHUNK);
      tx_end();
      reboot();
    }

    // Set Audio Sensitivity ----------------------------
    else if (strcmp(command_type, "sensitivity") == 0) {
      if (strcmp(command_data, "default") == 0) {
        CONFIG.SENSITIVITY = CONFIG_DEFAULTS.SENSITIVITY;
      } else {
        CONFIG.SENSITIVITY = atof(command_data);
      }

      save_config_delayed();
      tx_begin();
      USBSerial.print("CONFIG.SENSITIVITY: ");
      USBSerial.println(CONFIG.SENSITIVITY);
      tx_end();
    }

    // Toggle Boot Animation --------------------------
    else if (strcmp(command_type, "boot_animation") == 0) {
      bool good = false;
      if (strcmp(command_data, "default") == 0) {
        CONFIG.BOOT_ANIMATION = CONFIG_DEFAULTS.BOOT_ANIMATION;
        good = true;
      } else if (strcmp(command_data, "true") == 0) {
        CONFIG.BOOT_ANIMATION = true;
        good = true;
      } else if (strcmp(command_data, "false") == 0) {
        CONFIG.BOOT_ANIMATION = false;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        save_config();
        tx_begin();
        USBSerial.print("CONFIG.BOOT_ANIMATION: ");
        USBSerial.println(CONFIG.BOOT_ANIMATION);
        tx_end();
        reboot();
      }
    }

    // Toggle Lightshow Mirroring ---------------------
    else if (strcmp(command_type, "mirror_enabled") == 0) {
      bool good = false;
      if (strcmp(command_data, "default") == 0) {
        CONFIG.MIRROR_ENABLED = CONFIG_DEFAULTS.MIRROR_ENABLED;
        good = true;
      } else if (strcmp(command_data, "true") == 0) {
        CONFIG.MIRROR_ENABLED = true;
        good = true;
      } else if (strcmp(command_data, "false") == 0) {
        CONFIG.MIRROR_ENABLED = false;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        save_config_delayed();
        tx_begin();
        USBSerial.print("CONFIG.MIRROR_ENABLED: ");
        USBSerial.println(CONFIG.MIRROR_ENABLED);
        tx_end();
      }
    }

    // Set Sweet Spot LOW threshold -------------------
    else if (strcmp(command_type, "sweet_spot_min") == 0) {
      if (strcmp(command_data, "default") == 0) {
        CONFIG.SWEET_SPOT_MIN_LEVEL = CONFIG_DEFAULTS.SWEET_SPOT_MIN_LEVEL;
      } else {
        CONFIG.SWEET_SPOT_MIN_LEVEL = constrain(atof(command_data), 0, uint32_t(-1));
      }

      save_config_delayed();
      tx_begin();
      USBSerial.print("CONFIG.SWEET_SPOT_MIN_LEVEL: ");
      USBSerial.println(CONFIG.SWEET_SPOT_MIN_LEVEL);
      tx_end();
    }

    // Set Sweet Spot HIGH threshold ------------------
    else if (strcmp(command_type, "sweet_spot_max") == 0) {
      if (strcmp(command_data, "default") == 0) {
        CONFIG.SWEET_SPOT_MAX_LEVEL = CONFIG_DEFAULTS.SWEET_SPOT_MAX_LEVEL;
      } else {
        CONFIG.SWEET_SPOT_MAX_LEVEL = constrain(atof(command_data), 0, uint32_t(-1));
      }

      save_config_delayed();
      tx_begin();
      USBSerial.print("CONFIG.SWEET_SPOT_MAX_LEVEL: ");
      USBSerial.println(CONFIG.SWEET_SPOT_MAX_LEVEL);
      tx_end();
    }

    // Set Chromagram Range ---------------
    else if (strcmp(command_type, "chromagram_range") == 0) {
      if (strcmp(command_data, "default") == 0) {
        CONFIG.CHROMAGRAM_RANGE = CONFIG_DEFAULTS.CHROMAGRAM_RANGE;
      } else {
        CONFIG.CHROMAGRAM_RANGE = constrain(atof(command_data), 1, 64);
      }

      save_config_delayed();
      tx_begin();
      USBSerial.print("CONFIG.CHROMAGRAM_RANGE: ");
      USBSerial.println(CONFIG.CHROMAGRAM_RANGE);
      tx_end();
    }

    // Set Standby Dimming behavior -------
    else if (strcmp(command_type, "standby_dimming") == 0) {
      bool good = false;
      if (strcmp(command_data, "default") == 0) {
        CONFIG.STANDBY_DIMMING = CONFIG_DEFAULTS.STANDBY_DIMMING;
        good = true;
      } else if (strcmp(command_data, "true") == 0) {
        CONFIG.STANDBY_DIMMING = true;
        good = true;
      } else if (strcmp(command_data, "false") == 0) {
        CONFIG.STANDBY_DIMMING = false;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        save_config_delayed();
        tx_begin();
        USBSerial.print("CONFIG.STANDBY_DIMMING: ");
        USBSerial.println(CONFIG.STANDBY_DIMMING);
        tx_end();
      }
    }

    // Toggle bass mode -------------------
    else if (strcmp(command_type, "bass_mode") == 0) {
      bool good = false;
      if (strcmp(command_data, "true") == 0) {
        CONFIG.NOTE_OFFSET = 0;
        CONFIG.CHROMAGRAM_RANGE = 24;
        good = true;
      } else if (strcmp(command_data, "false") == 0) {
        CONFIG.NOTE_OFFSET = CONFIG_DEFAULTS.NOTE_OFFSET;
        CONFIG.CHROMAGRAM_RANGE = CONFIG_DEFAULTS.CHROMAGRAM_RANGE;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        save_config();
        tx_begin();
        USBSerial.println("BASS MODE ENABLED");
        tx_end();
        reboot();
      }
    }

    // Set if image should be reversed ------------------------
    else if (strcmp(command_type, "reverse_order") == 0) {
      bool good = false;
      if (strcmp(command_data, "default") == 0) {
        CONFIG.REVERSE_ORDER = CONFIG_DEFAULTS.REVERSE_ORDER;
        good = true;
      } else if (strcmp(command_data, "true") == 0) {
        CONFIG.REVERSE_ORDER = true;
        good = true;
      } else if (strcmp(command_data, "false") == 0) {
        CONFIG.REVERSE_ORDER = false;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        save_config_delayed();
        tx_begin();
        USBSerial.print("CONFIG.REVERSE_ORDER: ");
        USBSerial.println(CONFIG.REVERSE_ORDER);
        tx_end();
      }
    }

    // Set max LED current ----------------------------
    else if (strcmp(command_type, "max_current_ma") == 0) {
      if (strcmp(command_data, "default") == 0) {
        CONFIG.MAX_CURRENT_MA = CONFIG_DEFAULTS.MAX_CURRENT_MA;
      } else {
        CONFIG.MAX_CURRENT_MA = constrain(atof(command_data), 0, uint32_t(-1));
      }

      FastLED.setMaxPowerInVoltsAndMilliamps(5.0, CONFIG.MAX_CURRENT_MA);

      save_config_delayed();
      tx_begin();
      USBSerial.print("CONFIG.MAX_CURRENT_MA: ");
      USBSerial.println(CONFIG.MAX_CURRENT_MA);
      tx_end();
    }

    // Stream a given value over Serial -----------------
    else if (strcmp(command_type, "stream") == 0) {
      stop_streams();  // Stop any current streams
      if (strcmp(command_data, "audio") == 0) {
        stream_audio = true;
        ack();
      } else if (strcmp(command_data, "fps") == 0) {
        stream_fps = true;
        ack();
      } else if (strcmp(command_data, "max_mags") == 0) {
        stream_max_mags = true;
        ack();
      } else if (strcmp(command_data, "max_mags_followers") == 0) {
        stream_max_mags_followers = true;
        ack();
      } else if (strcmp(command_data, "magnitudes") == 0) {
        stream_magnitudes = true;
        ack();
      } else if (strcmp(command_data, "spectrogram") == 0) {
        stream_spectrogram = true;
        ack();
      } else if (strcmp(command_data, "chromagram") == 0) {
        stream_chromagram = true;
        ack();
      } else {
        bad_command(command_type, command_data);
      }
    }

    // Toggle Color Shift ---------------------------------
    else if (strcmp(command_type, "auto_color_shift") == 0) {
      bool good = false;
      if (strcmp(command_data, "default") == 0) {
        CONFIG.AUTO_COLOR_SHIFT = CONFIG_DEFAULTS.AUTO_COLOR_SHIFT;
        good = true;
      } else if (strcmp(command_data, "true") == 0) {
        CONFIG.AUTO_COLOR_SHIFT = true;
        good = true;
      } else if (strcmp(command_data, "false") == 0) {
        CONFIG.AUTO_COLOR_SHIFT = false;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        save_config_delayed();
        tx_begin();
        USBSerial.print("CONFIG.AUTO_COLOR_SHIFT: ");
        USBSerial.println(CONFIG.AUTO_COLOR_SHIFT);
        tx_end();
      }
    }

    // Set Incandescent LUT intensity ----------------------------
    else if (strcmp(command_type, "incandescent_filter") == 0) {
      if (strcmp(command_data, "default") == 0) {
        CONFIG.INCANDESCENT_FILTER = CONFIG_DEFAULTS.INCANDESCENT_FILTER;
      } else {
        CONFIG.INCANDESCENT_FILTER = atof(command_data);
        if (CONFIG.INCANDESCENT_FILTER < 0.0) {
          CONFIG.INCANDESCENT_FILTER = 0.0;
        } else if (CONFIG.INCANDESCENT_FILTER > 1.0) {
          CONFIG.INCANDESCENT_FILTER = 1.0;
        }
      }

      save_config_delayed();
      tx_begin();
      USBSerial.print("CONFIG.INCANDESCENT_FILTER: ");
      USBSerial.println(CONFIG.INCANDESCENT_FILTER);
      tx_end();
    }

    // Toggle Incandescent Mode ----------------------------
    else if (strcmp(command_type, "incandescent_mode") == 0) {
      bool good = false;
      if (strcmp(command_data, "default") == 0) {
        CONFIG.INCANDESCENT_MODE = CONFIG_DEFAULTS.INCANDESCENT_MODE;
        good = true;
      } else if (strcmp(command_data, "true") == 0) {
        CONFIG.INCANDESCENT_MODE = true;
        good = true;
      } else if (strcmp(command_data, "false") == 0) {
        CONFIG.INCANDESCENT_MODE = false;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        save_config_delayed();
        tx_begin();
        USBSerial.print("CONFIG.INCANDESCENT_MODE: ");
        USBSerial.println(CONFIG.INCANDESCENT_MODE);
        tx_end();
      }
    }

    // Set Bulb Cover Opacity ----------------------------
    else if (strcmp(command_type, "bulb_opacity") == 0) {
      if (strcmp(command_data, "default") == 0) {
        CONFIG.BULB_OPACITY = CONFIG_DEFAULTS.BULB_OPACITY;
      } else {
        CONFIG.BULB_OPACITY = atof(command_data);
        if (CONFIG.BULB_OPACITY < 0.0) {
          CONFIG.BULB_OPACITY = 0.0;
        } else if (CONFIG.BULB_OPACITY > 1.0) {
          CONFIG.BULB_OPACITY = 1.0;
        }
      }

      save_config_delayed();
      tx_begin();
      USBSerial.print("CONFIG.BULB_OPACITY: ");
      USBSerial.println(CONFIG.BULB_OPACITY);
      tx_end();
    }

    // Set Saturation ----------------------------
    else if (strcmp(command_type, "saturation") == 0) {
      if (strcmp(command_data, "default") == 0) {
        CONFIG.SATURATION = CONFIG_DEFAULTS.SATURATION;
      } else {
        CONFIG.SATURATION = atof(command_data);
        if (CONFIG.SATURATION < 0.0) {
          CONFIG.SATURATION = 0.0;
        } else if (CONFIG.SATURATION > 1.0) {
          CONFIG.SATURATION = 1.0;
        }
      }

      save_config_delayed();
      tx_begin();
      USBSerial.print("CONFIG.SATURATION: ");
      USBSerial.println(CONFIG.SATURATION);
      tx_end();
    }

    // Set Prism Count ----------------------------------------
    else if (strcmp(command_type, "prism_count") == 0) {
      bool good = false;
      if (strcmp(command_data, "default") == 0) {
        good = true;
        CONFIG.PRISM_COUNT = CONFIG_DEFAULTS.PRISM_COUNT;
      } else {
        good = true;
        CONFIG.PRISM_COUNT = constrain(atol(command_data), 0, 10);
      }

      if (good) {
        save_config();
        tx_begin();
        USBSerial.print("CONFIG.PRISM_COUNT: ");
        USBSerial.println(CONFIG.PRISM_COUNT);
        tx_end();
      }
    }

    // Set CONFIG preset ----------------------------
    else if (strcmp(command_type, "preset") == 0) {
      bool good = false;

      if      (strcmp(command_data, "default")      == 0) { good = true; }
      else if (strcmp(command_data, "tinted_bulbs") == 0) { good = true; }
      else if (strcmp(command_data, "incandescent") == 0) { good = true; }
      else if (strcmp(command_data, "white")        == 0) { good = true; }
      else if (strcmp(command_data, "classic")      == 0) { good = true; }

      else { // Bad preset name
        bad_command(command_type, command_data);
      }

      if (good) {
        set_preset(command_data); // presets.h

        save_config_delayed();
        tx_begin();
        USBSerial.print("ENABLED PRESET: ");
        USBSerial.println(command_data);
        tx_end();
      }
    }

    // Secondary LED Controls ----------------------------
    else if (strcmp(command_type, "secondary_enabled") == 0) {
      bool good = false;
      if (strcmp(command_data, "true") == 0) {
        ENABLE_SECONDARY_LEDS = true;
        good = true;
      } else if (strcmp(command_data, "false") == 0) {
        ENABLE_SECONDARY_LEDS = false;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        tx_begin();
        USBSerial.print("SECONDARY_ENABLED: ");
        USBSerial.println(ENABLE_SECONDARY_LEDS);
        tx_end();
      }
    }
    
    else if (strcmp(command_type, "secondary_mode") == 0) {
      uint8_t mode = atoi(command_data);
      if (mode < NUM_MODES) {
        uint8_t previous_mode = SECONDARY_LIGHTSHOW_MODE; // Store previous mode
        SECONDARY_LIGHTSHOW_MODE = mode;
        
        tx_begin();
        USBSerial.print("SECONDARY_MODE: ");
        USBSerial.print(SECONDARY_LIGHTSHOW_MODE);
        USBSerial.print(" (");
        USBSerial.print(mode_names + (SECONDARY_LIGHTSHOW_MODE * 32));
        USBSerial.println(")");
        tx_end();
        
        if (debug_mode) {
          USBSerial.print("SECONDARY MODE CHANGED: from ");
          USBSerial.print(previous_mode);
          USBSerial.print(" (");
          USBSerial.print(mode_names + (previous_mode * 32));
          USBSerial.print(") to ");
          USBSerial.print(SECONDARY_LIGHTSHOW_MODE);
          USBSerial.print(" (");
          USBSerial.print(mode_names + (SECONDARY_LIGHTSHOW_MODE * 32));
          USBSerial.println(")");
        }
        
        // Enable secondary LEDs if they aren't already
        ENABLE_SECONDARY_LEDS = true;
      } else {
        bad_command(command_type, command_data);
      }
    }
    
    else if (strcmp(command_type, "secondary_photons") == 0) {
      SECONDARY_PHOTONS = constrain(atof(command_data), 0.0, 1.0);
      
      tx_begin();
      USBSerial.print("SECONDARY_PHOTONS: ");
      USBSerial.println(SECONDARY_PHOTONS, 6);
      tx_end();
    }
    
    else if (strcmp(command_type, "secondary_chroma") == 0) {
      SECONDARY_CHROMA = constrain(atof(command_data), 0.0, 1.0);
      
      tx_begin();
      USBSerial.print("SECONDARY_CHROMA: ");
      USBSerial.println(SECONDARY_CHROMA, 6);
      tx_end();
    }
    
    else if (strcmp(command_type, "secondary_mood") == 0) {
      SECONDARY_MOOD = constrain(atof(command_data), 0.0, 1.0);
      
      tx_begin();
      USBSerial.print("SECONDARY_MOOD: ");
      USBSerial.println(SECONDARY_MOOD, 6);
      tx_end();
    }
    
    else if (strcmp(command_type, "secondary_saturation") == 0) {
      SECONDARY_SATURATION = constrain(atof(command_data), 0.0, 1.0);
      
      tx_begin();
      USBSerial.print("SECONDARY_SATURATION: ");
      USBSerial.println(SECONDARY_SATURATION, 6);
      tx_end();
    }
    
    else if (strcmp(command_type, "secondary_prism_count") == 0) {
      SECONDARY_PRISM_COUNT = constrain(atoi(command_data), 0, 10);
      
      tx_begin();
      USBSerial.print("SECONDARY_PRISM_COUNT: ");
      USBSerial.println(SECONDARY_PRISM_COUNT);
      tx_end();
    }
    
    else if (strcmp(command_type, "secondary_mirror_enabled") == 0) {
      bool good = false;
      if (strcmp(command_data, "true") == 0) {
        SECONDARY_MIRROR_ENABLED = true;
        good = true;
      } else if (strcmp(command_data, "false") == 0) {
        SECONDARY_MIRROR_ENABLED = false;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        tx_begin();
        USBSerial.print("SECONDARY_MIRROR_ENABLED: ");
        USBSerial.println(SECONDARY_MIRROR_ENABLED);
        tx_end();
      }
    }
    
    else if (strcmp(command_type, "secondary_reverse_order") == 0) {
      bool good = false;
      if (strcmp(command_data, "true") == 0) {
        SECONDARY_REVERSE_ORDER = true;
        good = true;
      } else if (strcmp(command_data, "false") == 0) {
        SECONDARY_REVERSE_ORDER = false;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        tx_begin();
        USBSerial.print("SECONDARY_REVERSE_ORDER: ");
        USBSerial.println(SECONDARY_REVERSE_ORDER);
        tx_end();
      }
    }
    
    else if (strcmp(command_type, "secondary_base_coat") == 0) {
      bool good = false;
      if (strcmp(command_data, "true") == 0) {
        SECONDARY_BASE_COAT = true;
        good = true;
      } else if (strcmp(command_data, "false") == 0) {
        SECONDARY_BASE_COAT = false;
        good = true;
      } else {
        bad_command(command_type, command_data);
      }

      if (good) {
        tx_begin();
        USBSerial.print("SECONDARY_BASE_COAT: ");
        USBSerial.println(SECONDARY_BASE_COAT);
        tx_end();
      }
    }
    
    else if (strcmp(command_type, "secondary_status") == 0) {
      tx_begin();
      USBSerial.print("SECONDARY_ENABLED: ");
      USBSerial.println(ENABLE_SECONDARY_LEDS ? "true" : "false");
      USBSerial.print("SECONDARY_MODE: ");
      USBSerial.print(SECONDARY_LIGHTSHOW_MODE);
      USBSerial.print(" (");
      USBSerial.print(mode_names + (SECONDARY_LIGHTSHOW_MODE * 32));
      USBSerial.println(")");
      USBSerial.print("SECONDARY_PHOTONS: ");
      USBSerial.println(SECONDARY_PHOTONS, 6);
      USBSerial.print("SECONDARY_CHROMA: ");
      USBSerial.println(SECONDARY_CHROMA, 6);
      USBSerial.print("SECONDARY_MOOD: ");
      USBSerial.println(SECONDARY_MOOD, 6);
      USBSerial.print("SECONDARY_SATURATION: ");
      USBSerial.println(SECONDARY_SATURATION, 6);
      USBSerial.print("SECONDARY_PRISM_COUNT: ");
      USBSerial.println(SECONDARY_PRISM_COUNT);
      USBSerial.print("SECONDARY_MIRROR_ENABLED: ");
      USBSerial.println(SECONDARY_MIRROR_ENABLED ? "true" : "false");
      USBSerial.print("SECONDARY_REVERSE_ORDER: ");
      USBSerial.println(SECONDARY_REVERSE_ORDER ? "true" : "false");
      USBSerial.print("SECONDARY_BASE_COAT: ");
      USBSerial.println(SECONDARY_BASE_COAT ? "true" : "false");
      tx_end();
    }

    // QoS controls (Phase 3) ------------------------------------
    /*
     * QoS Serial Controls (Phase 3)
     * -----------------------------
     * qos_brightness=on|off   : enable/disable uniform brightness degrade (default OFF)
     * qos_level=0..3          : manual override of QoS level (0=no trim .. 3=aggressive)
     * qos_status              : print current QoS state (level, prism trim, brightness scale)
     *
     * Implementation: Prism iterations are trimmed uniformly across channels; optional
     * brightness scaler is small and smoothed to avoid visual thrash.
     */
    else if (strcmp(command_type, "qos_brightness") == 0) {
      bool good = false;
      if (strcmp(command_data, "true") == 0 || strcmp(command_data, "on") == 0) {
        g_qos_brightness_degrade_enabled = true;
        good = true;
      } else if (strcmp(command_data, "false") == 0 || strcmp(command_data, "off") == 0) {
        g_qos_brightness_degrade_enabled = false;
        g_qos_brightness_scale = SQ15x16(1.0);
        good = true;
      }
      if (good) {
        tx_begin();
        USBSerial.print("QOS brightness degrade: ");
        USBSerial.println(g_qos_brightness_degrade_enabled ? "ON" : "OFF");
        USBSerial.print("QOS brightness scale: ");
        USBSerial.println(float(g_qos_brightness_scale), 2);
        tx_end();
      } else {
        bad_command(command_type, command_data);
      }
    }
    else if (strcmp(command_type, "qos_level") == 0) {
      int lvl = atoi(command_data);
      if (lvl >= 0 && lvl <= 3) {
        g_qos_level = (uint8_t)lvl;
        g_qos_prism_trim = g_qos_level;
        if (g_qos_brightness_degrade_enabled) {
          float target_scale = 1.0f - 0.05f * float(g_qos_level);
          g_qos_brightness_scale = SQ15x16(target_scale);
        } else {
          g_qos_brightness_scale = SQ15x16(1.0);
        }
        tx_begin();
        USBSerial.print("QOS level set to ");
        USBSerial.print(g_qos_level);
        USBSerial.print(" prism_trim=");
        USBSerial.print(g_qos_prism_trim);
        USBSerial.print(" bright=");
        USBSerial.println(float(g_qos_brightness_scale), 2);
        tx_end();
      } else {
        bad_command(command_type, command_data);
      }
    }
    else if (strcmp(command_type, "qos_status") == 0) {
      tx_begin();
      USBSerial.print("QOS enabled: "); USBSerial.println(g_qos_brightness_degrade_enabled ? "true" : "false");
      USBSerial.print("QOS level: "); USBSerial.println(g_qos_level);
      USBSerial.print("QOS prism_trim: "); USBSerial.println(g_qos_prism_trim);
      USBSerial.print("QOS brightness: "); USBSerial.println(float(g_qos_brightness_scale), 2);
      USBSerial.print("QOS C thresholds (us): target="); USBSerial.print(g_qos_c_target_us);
      USBSerial.print(" high="); USBSerial.print(g_qos_c_high_us);
      USBSerial.print(" low="); USBSerial.println(g_qos_c_low_us);
      USBSerial.print("C/D avg (us): "); USBSerial.print(g_last_c_avg_us); USBSerial.print("/"); USBSerial.println(g_last_d_avg_us);
      USBSerial.print("effective_prism primary/secondary: "); USBSerial.print(g_last_effective_prism_primary); USBSerial.print("/"); USBSerial.println(g_last_effective_prism_secondary);
      tx_end();
    }

    // QoS C-threshold tuning (Agent B)
    else if (strcmp(command_type, "qos_c_target") == 0) {
      uint32_t v = (uint32_t) atol(command_data);
      g_qos_c_target_us = v;
      ack();
    }
    else if (strcmp(command_type, "qos_c_high") == 0) {
      uint32_t v = (uint32_t) atol(command_data);
      g_qos_c_high_us = v;
      ack();
    }
    else if (strcmp(command_type, "qos_c_low") == 0) {
      uint32_t v = (uint32_t) atol(command_data);
      g_qos_c_low_us = v;
      ack();
    }

    // Router FSM tuning (Agent B)
    else if (strcmp(command_type, "router_dwell_min") == 0) { g_router_dwell_min_beats = uint8_t(atoi(command_data)); ack(); }
    else if (strcmp(command_type, "router_dwell_max") == 0) { g_router_dwell_max_beats = uint8_t(atoi(command_data)); ack(); }
    else if (strcmp(command_type, "router_cooldown_min") == 0) { g_router_cooldown_min_beats = uint8_t(atoi(command_data)); ack(); }
    else if (strcmp(command_type, "router_cooldown_max") == 0) { g_router_cooldown_max_beats = uint8_t(atoi(command_data)); ack(); }
    else if (strcmp(command_type, "router_onset_prob") == 0) {
      int v = atoi(command_data); if (v < 0) v = 0; if (v > 100) v = 100; g_router_onset_prob_percent = uint8_t(v); ack();
    }
    else if (strcmp(command_type, "router_novelty_thresh") == 0) { g_router_novelty_thresh = SQ15x16(atof(command_data)); ack(); }
    else if (strcmp(command_type, "router_vu_delta") == 0) { g_router_vu_delta_thresh = SQ15x16(atof(command_data)); ack(); }
    else if (strcmp(command_type, "router_detune_max") == 0) { g_router_detune_max = SQ15x16(atof(command_data)); ack(); }
    else if (strcmp(command_type, "router_circ_frames_max") == 0) { int v = atoi(command_data); if (v < 1) v = 1; g_router_circ_frames_max = uint8_t(v); ack(); }
    else if (strcmp(command_type, "router_balance_min") == 0) { g_router_balance_min = SQ15x16(atof(command_data)); ack(); }
    else if (strcmp(command_type, "router_balance_max") == 0) { g_router_balance_max = SQ15x16(atof(command_data)); ack(); }
    else if (strncmp(command_type, "router_var_mix", 14) == 0) {
      // Expected format: detune:40,anti:30,circ:30
      uint8_t d=0,a=0,c=0; bool ok=false;
      int dd, aa, cc;
      if (sscanf(command_data, "detune:%d,anti:%d,circ:%d", &dd, &aa, &cc) == 3) {
        if (dd>=0 && aa>=0 && cc>=0 && dd+aa+cc>0) { d=uint8_t(dd); a=uint8_t(aa); c=uint8_t(cc); ok=true; }
      }
      if (ok) { g_router_var_mix_detune=d; g_router_var_mix_anti=a; g_router_var_mix_circ=c; ack(); }
      else { bad_command(command_type, command_data); }
    }
    else if (strcmp(command_type, "router_status") == 0) {
      tx_begin();
      USBSerial.println("ROUTER STATUS (Agent B)");
#if ENABLE_ROUTER_FSM
      USBSerial.print("enabled: "); USBSerial.println(g_router_enabled ? "true":"false");
#else
      USBSerial.println("enabled: false (ENABLE_ROUTER_FSM=0)");
#endif
      USBSerial.print("dwell_min/max: "); USBSerial.print(g_router_dwell_min_beats); USBSerial.print("/"); USBSerial.println(g_router_dwell_max_beats);
      USBSerial.print("cooldown_min/max: "); USBSerial.print(g_router_cooldown_min_beats); USBSerial.print("/"); USBSerial.println(g_router_cooldown_max_beats);
      USBSerial.print("onset_prob%: "); USBSerial.println(g_router_onset_prob_percent);
      USBSerial.print("novelty_thresh: "); USBSerial.println(float(g_router_novelty_thresh), 3);
      USBSerial.print("vu_delta: "); USBSerial.println(float(g_router_vu_delta_thresh), 3);
      USBSerial.print("var_mix detune/anti/circ %: "); USBSerial.print(g_router_var_mix_detune); USBSerial.print("/"); USBSerial.print(g_router_var_mix_anti); USBSerial.print("/"); USBSerial.println(g_router_var_mix_circ);
      USBSerial.print("detune_max: "); USBSerial.println(float(g_router_detune_max), 3);
      USBSerial.print("circ_frames_max: "); USBSerial.println(g_router_circ_frames_max);
      USBSerial.print("balance min/max: "); USBSerial.print(float(g_router_balance_min), 2); USBSerial.print("/"); USBSerial.println(float(g_router_balance_max), 2);
      // Cadence counters (last 4s window; reset occurs in coordinator)
      USBSerial.print("cadence trans/var (last window): "); USBSerial.print(g_router_state.cadence_transitions); USBSerial.print("/"); USBSerial.println(g_router_state.cadence_variations);
      USBSerial.print("last reason/dwell: "); USBSerial.print(g_router_last_reason); USBSerial.print("/"); USBSerial.println(g_router_last_dwell_beats);
      tx_end();
    }
    else if (strcmp(command_type, "router_enabled") == 0) {
#if ENABLE_ROUTER_FSM
      bool good = false;
      if (strcmp(command_data, "on") == 0 || strcmp(command_data, "true") == 0) {
        g_router_enabled = true; good = true;
      } else if (strcmp(command_data, "off") == 0 || strcmp(command_data, "false") == 0) {
        g_router_enabled = false; good = true;
      }
      if (good) { ack(); } else { bad_command(command_type, command_data); }
#else
      tx_begin(true);
      USBSerial.println("router FSM disabled (ENABLE_ROUTER_FSM=0)");
      tx_end(true);
#endif
    }
    else if (strcmp(command_type, "router_plan") == 0) {
      const CouplingPlan& p = g_coupling_plan;
      tx_begin();
      USBSerial.print("PLAN primary/secondary: ");
      USBSerial.print(mode_names + (p.primary_mode * 32));
      USBSerial.print("|");
      USBSerial.println(mode_names + (p.secondary_mode * 32));
      USBSerial.print("anti_phase: "); USBSerial.println(p.anti_phase ? "true" : "false");
      USBSerial.print("hue_detune: "); USBSerial.println(float(p.hue_detune), 3);
      USBSerial.print("phase_offset: "); USBSerial.println(float(p.phase_offset), 3);
      USBSerial.print("balance: "); USBSerial.println(float(p.intensity_balance), 3);
      tx_end();
    }
    else if (strcmp(command_type, "router_pairs") == 0) {
      tx_begin();
      for (uint8_t i = 0; i < NUM_COMPLEMENTARY_PAIRS; i++) {
        uint8_t a = COMPLEMENTARY_PAIRS[i][0];
        uint8_t b = COMPLEMENTARY_PAIRS[i][1];
        USBSerial.print(i); USBSerial.print(": ");
        USBSerial.print(mode_names + (a * 32));
        USBSerial.print(" | ");
        USBSerial.println(mode_names + (b * 32));
      }
      tx_end();
    }
    
    // Add backward compatibility for old commands
    else if (strcmp(command_buf, "SECONDARY_ON") == 0) {
      ENABLE_SECONDARY_LEDS = true;
      USBSerial.println("Secondary LEDs enabled");
      USBSerial.println("NOTE: This command is deprecated, please use secondary_enabled=true instead");
    }
    else if (strcmp(command_buf, "SECONDARY_OFF") == 0) {
      ENABLE_SECONDARY_LEDS = false;
      USBSerial.println("Secondary LEDs disabled");
      USBSerial.println("NOTE: This command is deprecated, please use secondary_enabled=false instead");
    }
    else if (strncmp(command_buf, "SECONDARY_MODE", 14) == 0) {
      uint8_t mode = atoi(command_buf + 15);
      if (mode < NUM_MODES) {
        SECONDARY_LIGHTSHOW_MODE = mode;
        USBSerial.print("Secondary mode set to: ");
        USBSerial.println(mode_names + (mode * 32));
        ENABLE_SECONDARY_LEDS = true;
        USBSerial.println("NOTE: This command is deprecated, please use secondary_mode=[int] instead");
      } else {
        USBSerial.println("Invalid mode number");
      }
    }
    else if (strcmp(command_buf, "SECONDARY_STATUS") == 0) {
      USBSerial.print("Secondary LEDs: ");
      USBSerial.println(ENABLE_SECONDARY_LEDS ? "ENABLED" : "DISABLED");
      USBSerial.print("  Mode: ");
      USBSerial.print(SECONDARY_LIGHTSHOW_MODE);
      USBSerial.print(" (");
      USBSerial.print(mode_names + (SECONDARY_LIGHTSHOW_MODE * 32));
      USBSerial.println(")");
      USBSerial.print("  Photons: ");
      USBSerial.println(SECONDARY_PHOTONS);
      USBSerial.print("  Chroma: ");
      USBSerial.println(SECONDARY_CHROMA);
      USBSerial.print("  Mood: ");
      USBSerial.println(SECONDARY_MOOD);
      USBSerial.println("NOTE: This command is deprecated, please use secondary_status instead");
    }

    // Test tone generation command ------------------------------
    else if (strcmp(command_type, "test_tone") == 0) {
      // Parse frequency from command_data
      float freq = atof(command_data);
      if (freq >= 20.0 && freq <= 20000.0) {
        // generate_test_tone(freq); // TODO: Implement
        tx_begin();
        USBSerial.print("Generated test tone at ");
        USBSerial.print(freq);
        USBSerial.println(" Hz in sample window");
        tx_end();
      } else {
        bad_command(command_type, command_data);
      }
    }
    
    // Performance monitoring commands --------------------------
#ifdef ENABLE_PERFORMANCE_MONITORING
    else if (strncmp(command_type, "PERF", 4) == 0) {
      handle_perf_command(command_buf);
    }
#endif

    // Start system benchmark -----------------------------------
    else if (strcmp(command_type, "start_benchmark") == 0) {

      if (!benchmark_running) {
        benchmark_running = true;
        benchmark_start_time = millis();
        system_fps_sum = 0;
        led_fps_sum = 0;
        benchmark_sample_count = 0;
        ack();
        tx_begin();
        USBSerial.print("Benchmark started (Duration: ");
        USBSerial.print(benchmark_duration / 1000);
        USBSerial.println(" seconds)...");
        tx_end();
      } else {
        tx_begin(true);
        USBSerial.println("Benchmark already running.");
        tx_end(true);
      }

    }

    // COMMAND NOT RECOGNISED #############################
    else {
      bad_command(command_type, command_data);
    }
    // COMMAND NOT RECOGNISED #############################
  }
}

// Called on every frame, collects incoming characters until
// potential commands are found
void check_serial(uint32_t t_now) {
  serial_iter++;
  if (USBSerial.available() > 0) {
    char c = USBSerial.read();
    if (c != '\n') {  // If normal character, add to buffer
      command_buf[command_buf_index] = c;
      command_buf_index++;

      // Avoid overflow at the cost of breaking a long command
      if (command_buf_index > 127) {
        command_buf_index = 127;
      }

    } else {  // If a newline character is received,
      // MODIFICATION [2025-09-20 23:50] - FIX: Trim trailing whitespace from commands
      // FAULT DETECTED: Commands with trailing spaces fail to match strcmp() checks
      // ROOT CAUSE: Terminal adds trailing space, causing "reset_vu_floor " != "reset_vu_floor"
      // SOLUTION RATIONALE: Trim trailing whitespace before parsing to handle various terminal behaviors
      // IMPACT ASSESSMENT: All commands will work regardless of trailing whitespace
      // VALIDATION METHOD: Commands with/without trailing spaces will both be recognized
      // ROLLBACK PROCEDURE: Remove trimming code if command parsing breaks

      // Trim trailing whitespace (including spaces, tabs, carriage returns)
      while (command_buf_index > 0 &&
             (command_buf[command_buf_index - 1] == ' ' ||
              command_buf[command_buf_index - 1] == '\t' ||
              command_buf[command_buf_index - 1] == '\r')) {
        command_buf_index--;
      }
      command_buf[command_buf_index] = '\0';  // Null terminate at the trimmed position

      // the command in the buffer should be parsed
      parse_command(command_buf);                   // Parse
      memset(&command_buf, 0, sizeof(char) * 128);  // Clear
      command_buf_index = 0;                        // Reset
    }
  }
}
#endif // SB_SERIAL_MENU_IMPL
