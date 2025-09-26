#include "globals.h"
#include <esp_pm.h>
#include "core/restart_utils.h"

#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT && !defined(USBSerial)
#define USBSerial Serial
#endif

#ifndef SB_SYSTEM_IMPL
extern uint32_t timing_start;
#else
uint32_t timing_start = 0;
#endif
extern void run_sweet_spot();
extern void show_leds();
extern void init_palette_luts();
extern void init_serial(uint32_t baud_rate);
extern void init_fs();
extern void restore_defaults();
extern void save_config();
extern void init_leds();
extern void init_secondary_leds();
extern void init_p2p();
extern void intro_animation();

#ifndef SB_SYSTEM_IMPL
void reboot();
#else
void reboot() {
  lock_leds();
  USBSerial.println("--- ! REBOOTING to apply changes (You may need to restart the Serial Monitor)");
  USBSerial.flush();
  for(float i = 1.0; i >= 0.0; i-=0.05){
    MASTER_BRIGHTNESS = i;
    run_sweet_spot();
    show_leds();
    FastLED.delay(12); // Takes ~250ms total
  }
  FastLED.setBrightness(0);
  FastLED.show();
  SB_RESTART("reboot()");
}
#endif


#ifndef SB_SYSTEM_IMPL
void start_timing(const char* func_name);
#else
void start_timing(const char* func_name) {
  USBSerial.print(func_name);
  USBSerial.print(": ");
  USBSerial.flush();
  timing_start = micros();
}
#endif


#ifndef SB_SYSTEM_IMPL
void end_timing();
#else
void end_timing() {
  uint32_t timing_end = micros();
  uint32_t t_delta = timing_end - timing_start;

  USBSerial.print("DONE IN ");
  USBSerial.print(t_delta / 1000.0, 3);
  USBSerial.println(" MS");
}
#endif


#ifndef SB_SYSTEM_IMPL
void check_current_function();
#else
void check_current_function() {
  function_hits[function_id]++;
}
#endif


static void usb_event_callback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == ARDUINO_USB_EVENTS) {
    //arduino_usb_event_data_t * data = (arduino_usb_event_data_t*)event_data;
    switch (event_id) {
      case ARDUINO_USB_STARTED_EVENT:
        //Serial0.println("USB PLUGGED");
        // POWER MANAGEMENT FIX: Maintain consistent CPU frequency regardless of USB status
        {
          esp_pm_config_esp32s3_t pm_config = {
            .max_freq_mhz = 240,
            .min_freq_mhz = 240,
            .light_sleep_enable = false
          };
          esp_pm_configure(&pm_config);
        }
        break;
      case ARDUINO_USB_STOPPED_EVENT:
        //Serial0.println("USB UNPLUGGED");
        // POWER MANAGEMENT FIX: Force same power management when USB disconnected
        {
          esp_pm_config_esp32s3_t pm_config = {
            .max_freq_mhz = 240,
            .min_freq_mhz = 240,
            .light_sleep_enable = false
          };
          esp_pm_configure(&pm_config);
        }
        break;
      case ARDUINO_USB_SUSPEND_EVENT:
        //Serial0.printf("USB SUSPENDED: remote_wakeup_en: %u\n", data->suspend.remote_wakeup_en);
        break;
      case ARDUINO_USB_RESUME_EVENT:
        //Serial0.println("USB RESUMED");
        break;

      default:
        break;
    }
  } else if (event_base == ARDUINO_FIRMWARE_MSC_EVENTS) {
    //arduino_firmware_msc_event_data_t * data = (arduino_firmware_msc_event_data_t*)event_data;
    switch (event_id) {
      case ARDUINO_FIRMWARE_MSC_START_EVENT:
        //Serial0.println("MSC Update Start");
        msc_update_started = true;
        break;
      case ARDUINO_FIRMWARE_MSC_WRITE_EVENT:
        //HWSerial.printf("MSC Update Write %u bytes at offset %u\n", data->write.size, data->write.offset);
        //Serial0.print(".");
        break;
      case ARDUINO_FIRMWARE_MSC_END_EVENT:
        //Serial0.printf("\nMSC Update End: %u bytes\n", data->end.size);
        break;
      case ARDUINO_FIRMWARE_MSC_ERROR_EVENT:
        //Serial0.printf("MSC Update ERROR! Progress: %u bytes\n", data->error.size);
        break;
      case ARDUINO_FIRMWARE_MSC_POWER_EVENT:
        //Serial0.printf("MSC Update Power: power: %u, start: %u, eject: %u", data->power.power_condition, data->power.start, data->power.load_eject);
        break;

      default:
        break;
    }
  }
}

#ifndef SB_SYSTEM_IMPL
void enable_usb_update_mode();
#else
void enable_usb_update_mode() {
  USB.onEvent(usb_event_callback);

  MSC_Update.onEvent(usb_event_callback);
  MSC_Update.begin();

  MASTER_BRIGHTNESS = 1.0;

  uint8_t led_index = 0;
  uint8_t sweet_index = 0;

  const uint8_t sweet_order[3][3] = {
    {1, 0, 0},
    {0, 1, 0},
    {0, 0, 1}
  };

  while (true) {
    for (uint8_t i = 0; i < NATIVE_RESOLUTION; i++) {
      leds_16[i] = {0, 0, 0};
    }

    if (msc_update_started == false) {
      leds_16[led_index] = {0, 0, 0.25};
      #ifndef ARDUINO_ESP32S3_DEV
      ledcWrite(SWEET_SPOT_LEFT_CHANNEL,   sweet_order[sweet_index][0] * 512);
      ledcWrite(SWEET_SPOT_CENTER_CHANNEL, sweet_order[sweet_index][1] * 512);
      ledcWrite(SWEET_SPOT_RIGHT_CHANNEL,  sweet_order[sweet_index][2] * 512);
      #endif
    }
    else {
      leds_16[NATIVE_RESOLUTION-1-led_index] = {0, 0.25, 0};
      #ifndef ARDUINO_ESP32S3_DEV
      ledcWrite(SWEET_SPOT_LEFT_CHANNEL,   sweet_order[sweet_index][2] * 4095);
      ledcWrite(SWEET_SPOT_CENTER_CHANNEL, sweet_order[sweet_index][1] * 4095);
      ledcWrite(SWEET_SPOT_RIGHT_CHANNEL,  sweet_order[sweet_index][0] * 4095);
      #endif
    }


    show_leds();

    if(led_index == 0 || led_index == NATIVE_RESOLUTION/2){
      sweet_index++;
      if (sweet_index >= 3) {
        sweet_index = 0;
      }
    }

    led_index++;
    if (led_index >= NATIVE_RESOLUTION) {
      led_index = 0;      
    }
    yield();
  }
}
#endif


#ifndef SB_SYSTEM_IMPL
void init_usb();
#else
void init_usb() {
  #ifdef ARDUINO_ESP32S3_DEV
    // S3 with CDC on boot doesn't need USB.begin()
    delay(100);
  #else
    USB.begin();
    delay(500);
  #endif
}
#endif


#ifndef SB_SYSTEM_IMPL
void init_sweet_spot();
#else
void init_sweet_spot() {
  #ifndef ARDUINO_ESP32S3_DEV
  // Only initialize sweet spot LEDs on S2 (S3 has no sweet spot LEDs)
  ledcSetup(SWEET_SPOT_LEFT_CHANNEL, 500, 12);
  ledcAttachPin(SWEET_SPOT_LEFT_PIN, SWEET_SPOT_LEFT_CHANNEL);

  ledcSetup(SWEET_SPOT_CENTER_CHANNEL, 500, 12);
  ledcAttachPin(SWEET_SPOT_CENTER_PIN, SWEET_SPOT_CENTER_CHANNEL);

  ledcSetup(SWEET_SPOT_RIGHT_CHANNEL, 500, 12);
  ledcAttachPin(SWEET_SPOT_RIGHT_PIN, SWEET_SPOT_RIGHT_CHANNEL);
  #endif
}
#endif


#ifndef SB_SYSTEM_IMPL
void generate_a_weights();
#else
void generate_a_weights() {}
#endif


#ifndef SB_SYSTEM_IMPL
void generate_window_lookup();
#else
void generate_window_lookup() {}
#endif


#ifndef SB_SYSTEM_IMPL
void precompute_goertzel_constants();
#else
void precompute_goertzel_constants() {}
#endif


#ifndef SB_SYSTEM_IMPL
void debug_function_timing(uint32_t t_now);
#else
void debug_function_timing(uint32_t t_now) {
  static uint32_t last_timing_print = t_now;

  if (t_now - last_timing_print >= 30000) {
    USBSerial.println("------------");
    for (uint8_t i = 0; i < 16; i++) {
      USBSerial.print(i);
      USBSerial.print(": ");
      USBSerial.println(function_hits[i]);

      function_hits[i] = 0;
    }

    last_timing_print = t_now;
  }
}
#endif


#ifndef SB_SYSTEM_IMPL
void init_system();
#else
void init_system() {
  // SINGLE-CORE OPTIMIZATION: Mutex creation removed
  // Both threads run on Core 0, eliminating need for synchronization
  
  // BUTTON CORRUPTION IMMUNITY: Force clean button state initialization
  noise_button.pin = NOISE_CAL_PIN;
  mode_button.pin = MODE_PIN;
  
  // SURGICAL ADDITION: Force clean button state (ESP32-S3 immunity)
  noise_button.pressed = false;
  noise_button.last_down = 0;
  noise_button.last_up = 0;
  mode_button.pressed = false;
  mode_button.last_down = 0;
  mode_button.last_up = 0;
  
  // CRITICAL: Ensure transition flags start clean
  noise_transition_queued = false;
  mode_transition_queued = false;

  #ifndef ARDUINO_ESP32S3_DEV
  // Only configure button pins on S2 (S3 has no physical buttons)
  pinMode(noise_button.pin, INPUT_PULLUP);
  pinMode(mode_button.pin, INPUT_PULLUP);
  #endif

  // SURGICAL ADDITION: Force clean button state to prevent corruption
  noise_button.pressed = false;
  noise_button.last_down = 0;
  noise_button.last_up = 0;
  mode_button.pressed = false;
  mode_button.last_down = 0;
  mode_button.last_up = 0;
  noise_transition_queued = false;  // CRITICAL: Prevent phantom noise cal trigger

  memcpy(&CONFIG_DEFAULTS, &CONFIG, sizeof(CONFIG)); // Copy defaults values to second CONFIG object

  init_usb();  // Initialize USB first for ESP32-S3
  init_serial(SERIAL_BAUD);

  #ifndef ARDUINO_ESP32S3_DEV
  init_sweet_spot();  // S3 has no sweet spot hardware
  #endif
  
  init_fs();

  #ifndef ARDUINO_ESP32S3_DEV
  // NOISE and MODE held down on boot (S2 only - S3 has no physical buttons)
  if (digitalRead(noise_button.pin) == LOW && digitalRead(mode_button.pin) == LOW) {
    restore_defaults();
  }
  #endif

  init_leds();

  // AUDIO FIX [2025-09-20]: Initialize secondary LEDs immediately after primary LEDs
  // This prevents FastLED GPIO interference with I2S audio initialization
  if (ENABLE_SECONDARY_LEDS) {
    init_secondary_leds();
  }

  #ifndef ARDUINO_ESP32S3_DEV
  // MODE held down on boot (S2 only - S3 has no physical buttons)
  if (digitalRead(mode_button.pin) == LOW) {
    enable_usb_update_mode();
  }
  #endif

  // Temporarily disable ESP-NOW/P2P to reduce memory pressure during diagnostics
  // init_p2p();
  // Audio pipeline initialization handled by new AudioSystem

  // PALETTE SYSTEM INITIALIZATION: Convert FastLED gradients to CRGB16 LUTs
  // This initializes all 33 LED-calibrated palettes from your curated collection
  init_palette_luts();
  g_palette_ready = true;

  if (USBSerial) {
    USBSerial.println("SYSTEM INIT COMPLETE!");
  }

  if (CONFIG.BOOT_ANIMATION == true) {
    intro_animation();
  }
}
#endif


#ifndef SB_SYSTEM_IMPL
void log_fps(uint32_t t_now_us);
#else
void log_fps(uint32_t t_now_us) {
  static uint32_t t_last = t_now_us;
  static float fps_history[10] = {0};
  static uint8_t fps_history_index = 0;

  uint32_t frame_delta_us = t_now_us - t_last;
  float fps_now = 1000000.0 / float(frame_delta_us);

  fps_history[fps_history_index] = fps_now;

  fps_history_index++;
  if (fps_history_index >= 10) {
    fps_history_index = 0;
  }

  float fps_sum = 0;
  for (uint8_t i = 0; i < 10; i++) {
    fps_sum += fps_history[i];
  }

  SYSTEM_FPS = fps_sum / 10.0;

  if (stream_fps == true) {
    USBSerial.print("sbs((fps=");
    USBSerial.print(SYSTEM_FPS);
    USBSerial.println("))");
  }

  t_last = t_now_us;
}
#endif


// This is to prevent overuse of internal flash writes!
// Instead of writing every single setting change to
// LittleFS, we wait until no settings have been altered
// for more than 3 seconds before attempting to update
// the flash with changes. This helps in scenarios where
// you're rapidly cycling through modes for example.
#ifndef SB_SYSTEM_IMPL
void check_settings(uint32_t t_now);
#else
void check_settings(uint32_t t_now) {
  if (settings_updated) {
    if (t_now >= next_save_time) {
      if(debug_mode == true){
        USBSerial.println("QUEUED CONFIG SAVE TRIGGERED");
      }
      save_config();
      settings_updated = false;
    }
  }
}
#endif
