#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

// Hardware pin definitions for Light Crystals
namespace HardwareConfig {
    // LED Configuration
    constexpr uint8_t LED_DATA_PIN = 6;  // GPIO6 is valid on ESP32-S3 (different from ESP32)
    constexpr uint16_t NUM_LEDS = 81;
    constexpr uint8_t NUM_STRIPS = 1;
    
    // Control pins
    constexpr uint8_t BUTTON_PIN = 0;
    constexpr uint8_t POWER_PIN = 5;
    
    // I2C bus (shared for external peripherals like M5ROTATE8)
    // LC hardware wiring: SDA=3, SCL=4
    constexpr uint8_t I2C_SDA = 3;
    constexpr uint8_t I2C_SCL = 4;
    
    // Audio input pins (for future use)
    constexpr uint8_t I2S_SCK = 3;
    constexpr uint8_t I2S_WS  = 2;
    constexpr uint8_t I2S_DIN = 4;
    
    // Performance settings
    constexpr uint16_t DEFAULT_FPS = 120;
    constexpr uint8_t DEFAULT_BRIGHTNESS = 96;
    constexpr uint32_t BUTTON_DEBOUNCE_MS = 500;
    
    // Memory limits
    constexpr size_t MAX_EFFECTS = 48;
    constexpr size_t TRANSITION_BUFFER_SIZE = NUM_LEDS * 3; // RGB bytes
}

// Legacy macros removed in favour of constexpr values in HardwareConfig.

#endif // HARDWARE_CONFIG_H
