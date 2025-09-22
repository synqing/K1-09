extern void propagate_noise_reset();

void start_noise_cal() {
  // EMERGENCY FIX PRESERVATION: Store critical AGC parameters before reset
  float preserved_vu_floor = CONFIG.VU_LEVEL_FLOOR;
  int16_t preserved_dc_offset = CONFIG.DC_OFFSET;
  uint16_t preserved_sweet_spot = CONFIG.SWEET_SPOT_MIN_LEVEL;

  noise_complete = false;
  max_waveform_val = 0;
  max_waveform_val_raw = 0;
  noise_iterations = 0;
  audio_raw_state.getDCOffsetSum() = 0;

  // CRITICAL: Only reset these if they're zero (uncalibrated)
  // Preserve emergency fixes and working calibrations
  if (CONFIG.DC_OFFSET == 0) {
    CONFIG.DC_OFFSET = 0;  // Reset only if uncalibrated
  } else {
    CONFIG.DC_OFFSET = preserved_dc_offset;  // Preserve working value
    USBSerial.println("NOISE CAL: Preserving existing DC_OFFSET");
  }

  if (CONFIG.VU_LEVEL_FLOOR == 0.0) {
    CONFIG.VU_LEVEL_FLOOR = 0.0;  // Reset only if uncalibrated
  } else {
    CONFIG.VU_LEVEL_FLOOR = preserved_vu_floor;  // Preserve working value
    USBSerial.println("NOISE CAL: Preserving existing VU_LEVEL_FLOOR");
  }

  if (CONFIG.SWEET_SPOT_MIN_LEVEL == 0) {
    CONFIG.SWEET_SPOT_MIN_LEVEL = 0;  // Reset only if uncalibrated
  } else {
    CONFIG.SWEET_SPOT_MIN_LEVEL = preserved_sweet_spot;  // Preserve working value
    USBSerial.println("NOISE CAL: Preserving existing SWEET_SPOT_MIN_LEVEL");
  }

  // Always reset noise samples and UI mask for fresh calibration
  for (uint8_t i = 0; i < NUM_FREQS; i++) {
    noise_samples[i] = 0;
  }
  for (uint8_t i = 0; i < NATIVE_RESOLUTION; i++) {
    ui_mask[i] = 0;
  }
  USBSerial.println("STARTING NOISE CAL (with AGC preservation)");
}

void clear_noise_cal() {
  USBSerial.println("DEBUG: clear_noise_cal() called - stack trace would be helpful");
  propagate_noise_reset();
  for (uint16_t i = 0; i < NUM_FREQS; i++) {
    noise_samples[i] = 0;
  }
  save_config();
  save_ambient_noise_calibration();
  USBSerial.println("NOISE CAL CLEARED");
}