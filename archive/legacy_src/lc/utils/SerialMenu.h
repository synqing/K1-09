#ifndef SERIAL_MENU_H
#define SERIAL_MENU_H

#include <Arduino.h>
#include <FastLED.h>
#include "../config/hardware_config.h"
#include "../config/features.h"
#include "../core/FxEngine.h"
#include "../core/PaletteManager.h"
#include "../core/RuntimeTunables.h"
#include "../hardware/PerformanceMonitor.h"

// External variables
extern FxEngine fxEngine;
extern PaletteManager paletteManager;
extern uint8_t brightnessVal;
extern uint8_t fadeAmount;
extern uint16_t fps;
extern uint8_t paletteSpeed;
extern PerformanceMonitor perfMon;

class SerialMenu {
private:
  String inputBuffer = "";
  bool menuActive = false;
  uint8_t currentMenuLevel = 0; // 0=main, 1=effects, 2=palettes, 3=settings
  
public:
  void begin() {
    Serial.println(F("\n=== Light Crystals Control System ==="));
    Serial.println(F("Type 'h' or 'help' for menu"));
    Serial.println(F("Type 'm' or 'menu' for main menu"));
    Serial.println(F("======================================"));
  }
  
  void update() {
    if (Serial.available()) {
      char c = Serial.read();
      
      if (c == '\n' || c == '\r') {
        if (inputBuffer.length() > 0) {
          processCommand(inputBuffer);
          inputBuffer = "";
        }
      } else if (c == 8 || c == 127) { // Backspace
        if (inputBuffer.length() > 0) {
          inputBuffer.remove(inputBuffer.length() - 1);
          Serial.print(F("\b \b"));
        }
      } else if (c >= 32 && c <= 126) { // Printable characters
        inputBuffer += c;
        Serial.print(c);
      }
    }
  }
  
private:
  void processCommand(String cmd) {
    cmd.trim();
    cmd.toLowerCase();
    
    Serial.println(); // New line after command
    
    if (cmd == "h" || cmd == "help") {
      showHelp();
    } else if (cmd == "m" || cmd == "menu") {
      showMainMenu();
    } else if (cmd == "s" || cmd == "status") {
      showStatus();
    } else if (cmd == "e" || cmd == "effects") {
      showEffectsMenu();
    } else if (cmd == "p" || cmd == "palettes") {
      showPalettesMenu();
    } else if (cmd == "ptype" || cmd == "paltype") {
      showPaletteTypesMenu();
    } else if (cmd == "c" || cmd == "config") {
      showConfigMenu();
    } else if (cmd == "perf" || cmd == "performance") {
      showPerformanceInfo();
    } else if (cmd == "perfdetail" || cmd == "pd") {
      showDetailedPerformance();
    } else if (cmd == "perfgraph" || cmd == "pg") {
      showPerformanceGraph();
    } else if (cmd == "perfreset") {
      resetPerformanceMetrics();
    } else if (cmd.startsWith("autocycle")) {
      handleAutoCycle(cmd.substring(9));
    } else if (cmd.startsWith("effect ")) {
      setEffect(cmd.substring(7));
    } else if (cmd.startsWith("palette ")) {
      setPalette(cmd.substring(8));
    } else if (cmd.startsWith("ptype ")) {
      setPaletteType(cmd.substring(6));
    } else if (cmd.startsWith("brightness ")) {
      setBrightness(cmd.substring(11));
    } else if (cmd.startsWith("fade ")) {
      setFade(cmd.substring(5));
    } else if (cmd.startsWith("speed ")) {
      setSpeed(cmd.substring(6));
    } else if (cmd.startsWith("fps ")) {
      setFPS(cmd.substring(4));
    } else if (cmd.startsWith("transition ")) {
      setTransition(cmd.substring(11));
    } else if (cmd.startsWith("guardrail")) {
      handleGuardrail(cmd.substring(9));
    } else if (cmd.startsWith("spork")) {
      handleSpork(cmd.substring(5));
    } else if (cmd == "next") {
      fxEngine.nextEffect(0, 800);
      Serial.println(F("Switched to next effect"));
    } else if (cmd == "prev") {
      fxEngine.prevEffect(0, 800);
      Serial.println(F("Switched to previous effect"));
    } else if (cmd == "reset") {
      resetToDefaults();
    } else if (cmd == "clear") {
      clearScreen();
    } else if (cmd == "wave") {
      showWaveMenu();
    } else if (cmd == "pipeline" || cmd == "pipe") {
      showPipelineMenu();
    } else {
      Serial.println(F("Unknown command. Type 'help' for commands."));
    }
  }
  
  void showHelp() {
    Serial.println(F("\n=== COMMAND HELP ==="));
    Serial.println(F("Navigation:"));
    Serial.println(F("  h, help       - Show this help"));
    Serial.println(F("  m, menu       - Show main menu"));
    Serial.println(F("  s, status     - Show current status"));
    Serial.println(F("  clear         - Clear screen"));
    Serial.println(F(""));
    Serial.println(F("Quick Commands:"));
    Serial.println(F("  next          - Next effect"));
    Serial.println(F("  prev          - Previous effect"));
    Serial.println(F("  effect <index> - Set effect by number"));
    Serial.println(F("  palette <0-n> - Set palette by number"));
    Serial.println(F("  ptype <index> - Set palette type"));
    Serial.println(F("  brightness <0-255> - Set brightness"));
    Serial.println(F("  fade <0-255>  - Set fade amount"));
    Serial.println(F("  speed <1-50>  - Set palette speed"));
    Serial.println(F("  fps <10-120>  - Set frame rate"));
    Serial.println(F("  reset         - Reset to defaults"));
    Serial.println(F(""));
    Serial.println(F("Menus:"));
    Serial.println(F("  e, effects    - Effects menu"));
    Serial.println(F("  p, palettes   - Palettes menu"));
    Serial.println(F("  ptype         - Palette type menu"));
    Serial.println(F("  c, config     - Configuration menu"));
    Serial.println(F("  wave          - Wave effects menu"));
    Serial.println(F("  pipe, pipeline- Pipeline effects menu"));
    Serial.println(F(""));
    Serial.println(F("Performance:"));
    Serial.println(F("  perf          - Quick performance info"));
    Serial.println(F("  pd, perfdetail- Detailed performance report"));
    Serial.println(F("  pg, perfgraph - Performance graph"));
    Serial.println(F("  perfreset     - Reset peak metrics"));
    Serial.println(F("  autocycle [nav|edit|play] [on|off] - Toggle per-layer palette auto-cycle"));
    Serial.println(F("  autocycle dwell <ms> - Set dwell time per palette (e.g. 8000)"));
    Serial.println(F("  guardrail [on|off|status] - Toggle brown guardrail"));
    Serial.println(F("  guardrail green <0-100> - Set max green % of red"));
    Serial.println(F("  guardrail blue <0-100>  - Set max blue % of red"));
    #if FEATURE_PIXELSPORK
    Serial.println(F("  spork status            - Show PixelSpork tunables"));
    Serial.println(F("  spork offset <ms>       - Set gradient offset rate (0=auto)"));
    Serial.println(F("  spork len <pixels>      - Set gradient length override (0=auto)"));
    Serial.println(F("  spork step <1-8>        - Set gradient offset step"));
    Serial.println(F("  spork fade <ms>         - Set EffectSet fade duration"));
    #endif
    Serial.println(F("=================="));
  }
  
  void showMainMenu() {
    Serial.println(F("\n=== MAIN MENU ==="));
    Serial.println(F("1. Effects Menu     (e)"));
    Serial.println(F("2. Palettes Menu    (p)"));
    Serial.println(F("3. Palette Types    (ptype)"));
    Serial.println(F("4. Configuration    (c)"));
    Serial.println(F("5. Wave Effects     (wave)"));
    Serial.println(F("6. Performance Info (perf)"));
    Serial.println(F("7. Status           (s)"));
    Serial.println(F("8. Help             (h)"));
    Serial.println(F("================"));
  }
  
  void showStatus() {
    Serial.println(F("\n=== CURRENT STATUS ==="));
    Serial.print(F("Effect: "));
    Serial.print(fxEngine.getCurrentEffectIndex());
    Serial.print(F(" - "));
    Serial.println(fxEngine.getCurrentEffectName());
    
    Serial.print(F("Palette: "));
    Serial.print(paletteManager.getCurrentTypeName());
    Serial.print(F(" #"));
    Serial.print(paletteManager.getCurrentIndex());
    Serial.print(F(" - "));
    Serial.println(paletteManager.getCurrentName());
    
    Serial.print(F("Brightness: "));
    Serial.println(brightnessVal);
    
    Serial.print(F("Fade Amount: "));
    Serial.println(fadeAmount);
    
    Serial.print(F("Palette Speed: "));
    Serial.println(paletteSpeed);
    
    Serial.print(F("Target FPS: "));
    Serial.println(fps);

    Serial.print(F("AutoCycle (NAV/EDIT/PLAY): "));
    Serial.print(rt_autocycle_nav ? F("ON") : F("OFF")); Serial.print('/');
    Serial.print(rt_autocycle_edit ? F("ON") : F("OFF")); Serial.print('/');
    Serial.print(rt_autocycle_play ? F("ON") : F("OFF"));
    Serial.print(F("  Dwell(ms): "));
    Serial.println(rt_palette_dwell_ms);

    Serial.print(F("Transition Type: "));
    switch (rt_transition_type) {
      case 0: Serial.println(F("Fade")); break;
      case 1: Serial.println(F("Wipe")); break;
      case 2: Serial.println(F("Blend")); break;
      default: Serial.println(F("Unknown")); break;
    }

    Serial.print(F("Brown Guardrail: "));
    Serial.print(rt_enable_brown_guardrail ? F("ON") : F("OFF"));
    Serial.print(F(" (G<=")); Serial.print(rt_max_g_percent_of_r);
    Serial.print(F("%, B<=")); Serial.print(rt_max_b_percent_of_r);
    Serial.println(F("%)"));

    #if FEATURE_PIXELSPORK
    Serial.println(F("PixelSpork:"));
    Serial.print(F("  Offset Rate (ms): ")); Serial.println(rt_spork_offset_rate_ms);
    Serial.print(F("  Offset Step:      ")); Serial.println(rt_spork_offset_step);
    Serial.print(F("  Grad Len Override:")); Serial.println(rt_spork_grad_len_override);
    Serial.print(F("  Fade Duration (ms):")); Serial.println(rt_spork_fade_ms);
    #endif
    
    Serial.print(F("Actual FPS: "));
    Serial.println(fxEngine.getApproximateFPS(), 1);
    
    if (fxEngine.getIsTransitioning()) {
      Serial.print(F("Transitioning: "));
      Serial.print(fxEngine.getTransitionProgress() * 100, 0);
      Serial.println(F("%"));
    }
    
    Serial.println(F("===================="));
  }

  void handleAutoCycle(String args) {
    args.trim();
    if (args.length() == 0) {
      Serial.println(F("\n=== AUTOCYCLE STATUS ==="));
      Serial.print(F("NAV: ")); Serial.println(rt_autocycle_nav ? F("ON") : F("OFF"));
      Serial.print(F("EDIT: ")); Serial.println(rt_autocycle_edit ? F("ON") : F("OFF"));
      Serial.print(F("PLAY: ")); Serial.println(rt_autocycle_play ? F("ON") : F("OFF"));
      Serial.print(F("Dwell (ms): ")); Serial.println(rt_palette_dwell_ms);
      Serial.println(F("========================"));
      return;
    }
    if (args.startsWith("dwell ")) {
      String val = args.substring(6);
      uint32_t ms = (uint32_t) val.toInt();
      if (ms < 1000) ms = 1000; if (ms > 60000) ms = 60000;
      rt_palette_dwell_ms = ms;
      Serial.print(F("Dwell set to ")); Serial.print(ms); Serial.println(F(" ms"));
      return;
    }
    // format: "nav on", "play off", etc.
    int sp = args.indexOf(' ');
    if (sp <= 0) { Serial.println(F("Usage: autocycle [nav|edit|play] [on|off]")); return; }
    String which = args.substring(0, sp); which.trim(); which.toLowerCase();
    String val = args.substring(sp+1); val.trim(); val.toLowerCase();
    bool on = (val == "on");
    if (which == "nav") rt_autocycle_nav = on;
    else if (which == "edit") rt_autocycle_edit = on;
    else if (which == "play") rt_autocycle_play = on;
    else { Serial.println(F("Invalid layer (nav|edit|play)")); return; }
    Serial.print(F("AutoCycle for ")); Serial.print(which); Serial.print(F(" set to ")); Serial.println(on ? F("ON") : F("OFF"));
  }
  
  void showEffectsMenu() {
    Serial.println(F("\n=== EFFECTS MENU ==="));
    for (uint8_t i = 0; i < fxEngine.getNumEffects(); i++) {
      Serial.print(i);
      Serial.print(F(". "));

      uint8_t currentEffect = fxEngine.getCurrentEffectIndex();
      if (i == currentEffect) {
        Serial.print(F(">>> "));
      }

      const char* effectName = fxEngine.getEffectName(i);
      if (!effectName || effectName[0] == '\0') {
        Serial.print(F("Effect "));
        Serial.print(i);
      } else {
        Serial.print(effectName);
      }

      if (i == currentEffect) {
        Serial.print(F(" <<<"));
      }
      Serial.println();
    }
    Serial.println(F(""));
    Serial.println(F("Commands:"));
    Serial.println(F("  effect <index> - Select effect"));
    Serial.println(F("  next          - Next effect"));
    Serial.println(F("  prev          - Previous effect"));
    Serial.println(F("  transition <0-2> - Set transition type"));
    Serial.println(F("=================="));
  }
  
  void showPalettesMenu() {
    Serial.println(F("\n=== PALETTES MENU ==="));
    Serial.print(F("Type: "));
    Serial.println(paletteManager.getCurrentTypeName());
    Serial.println(F(""));

    const uint8_t paletteCount = paletteManager.getPaletteCount();
    const char* const* names = paletteManager.getPaletteNames();
    if (paletteCount == 0) {
      Serial.println(F("No palettes available for this type."));
      Serial.println(F(""));
      Serial.println(F("Use ptype <index> to select a different type."));
      Serial.println(F("===================="));
      return;
    }
    for (uint8_t i = 0; i < paletteCount; i++) {
      Serial.print(i);
      Serial.print(F(". "));
      if (i == paletteManager.getCurrentIndex()) {
        Serial.print(F(">>> "));
      }
      if (names && names[i]) {
        Serial.print(names[i]);
      } else {
        Serial.print(F("Palette "));
        Serial.print(i);
      }
      if (i == paletteManager.getCurrentIndex()) {
        Serial.print(F(" <<<"));
      }
      Serial.println();

      if ((i + 1) % 5 == 0 && i + 1 < paletteCount) {
        Serial.println();
      }
    }
    Serial.println(F(""));
    Serial.println(F("Commands:"));
    Serial.print(F("  palette <0-"));
    Serial.print(paletteCount - 1);
    Serial.println(F("> - Select palette"));
    Serial.println(F("  ptype         - Palette type menu"));
    Serial.println(F("===================="));
  }

  void showPaletteTypesMenu() {
    Serial.println(F("\n=== PALETTE TYPES ==="));
    const uint8_t typeCount = PaletteManager::getTypeCount();
    if (typeCount == 0) {
      Serial.println(F("No palette types available."));
      Serial.println(F("======================"));
      return;
    }
    for (uint8_t i = 0; i < typeCount; ++i) {
      Serial.print(i);
      Serial.print(F(". "));
      if (i == paletteManager.getCurrentType()) {
        Serial.print(F(">>> "));
      }
      Serial.print(PaletteManager::getTypeName(static_cast<PaletteManager::PaletteType>(i)));
      if (i == paletteManager.getCurrentType()) {
        Serial.print(F(" <<<"));
      }
      Serial.println();
    }
    Serial.println(F(""));
    Serial.println(F("Commands:"));
    Serial.println(F("  ptype <index> - Select palette type"));
    Serial.println(F("======================"));
  }
  
  void showConfigMenu() {
    Serial.println(F("\n=== CONFIGURATION ==="));
    Serial.print(F("Brightness: "));
    Serial.print(brightnessVal);
    Serial.println(F(" (0-255)"));
    
    Serial.print(F("Fade Amount: "));
    Serial.print(fadeAmount);
    Serial.println(F(" (0-255)"));
    
    Serial.print(F("Palette Speed: "));
    Serial.print(paletteSpeed);
    Serial.println(F(" (1-50)"));
    
    Serial.print(F("Target FPS: "));
    Serial.print(fps);
    Serial.println(F(" (10-120)"));
    
    Serial.println(F(""));
    Serial.println(F("Commands:"));
    Serial.println(F("  brightness <0-255> - Set brightness"));
    Serial.println(F("  fade <0-255>       - Set fade amount"));
    Serial.println(F("  speed <1-50>       - Set palette speed"));
    Serial.println(F("  fps <10-120>       - Set frame rate"));
    Serial.println(F("  reset              - Reset to defaults"));
    Serial.println(F("===================="));
  }
  
  void showWaveMenu() {
    Serial.println(F("\n=== WAVE EFFECTS ==="));
    Serial.println(F("Wave diagnostics are not available in this build."));
    Serial.println(F("Use the effects menu to select wave-style animations."));
    Serial.println(F("=================="));
  }
  
  void showPerformanceInfo() {
    
    Serial.println(F("\n=== PERFORMANCE INFO ==="));
    
    // Get timing percentages
    float effectPct, ledPct, serialPct, idlePct;
    perfMon.getTimingPercentages(effectPct, ledPct, serialPct, idlePct);
    
    Serial.print(F("FPS: "));
    Serial.print(perfMon.getCurrentFPS(), 1);
    Serial.print(F(" / "));
    Serial.print(fps);
    Serial.print(F(" ("));
    Serial.print(perfMon.getCurrentFPS() / fps * 100, 0);
    Serial.println(F("%)"));
    
    Serial.print(F("CPU Usage: "));
    Serial.print(perfMon.getCPUUsage(), 1);
    Serial.println(F("%"));
    
    Serial.println(F("\nTiming Breakdown:"));
    Serial.print(F("  Effect: "));
    Serial.print(effectPct, 1);
    Serial.print(F("% ("));
    Serial.print(perfMon.getEffectTime());
    Serial.println(F("μs)"));
    
    Serial.print(F("  FastLED: "));
    Serial.print(ledPct, 1);
    Serial.print(F("% ("));
    Serial.print(perfMon.getFastLEDTime());
    Serial.println(F("μs)"));
    
    Serial.print(F("  Serial: "));
    Serial.print(serialPct, 1);
    Serial.println(F("%"));
    
    Serial.print(F("  Idle: "));
    Serial.print(idlePct, 1);
    Serial.println(F("%"));
    
    Serial.print(F("\nDropped Frames: "));
    Serial.println(perfMon.getDroppedFrames());
    
    Serial.print(F("Free Heap: "));
    Serial.print(ESP.getFreeHeap());
    Serial.print(F(" / Min: "));
    Serial.println(perfMon.getMinFreeHeap());
    
    Serial.println(F("======================"));
  }
  
  void showDetailedPerformance() {
    perfMon.printDetailedReport();
  }
  
  void showPerformanceGraph() {
    perfMon.drawPerformanceGraph();
  }

  void resetPerformanceMetrics() {
    perfMon.resetPeaks();
    Serial.println(F("Performance metrics reset"));
  }
  
  void showPipelineMenu() {
    Serial.println(F("\n=== PIPELINE EFFECTS ==="));
    Serial.println(F("Modular Visual Pipeline System"));
    Serial.println(F(""));
    Serial.println(F("Pipeline Effects:"));
    Serial.println(F("  11. Pipeline Gradient"));
    Serial.println(F("  12. Pipeline Fibonacci"));
    Serial.println(F("  13. Pipeline Audio"));
    Serial.println(F("  14. Pipeline Matrix"));
    Serial.println(F("  15. Pipeline Reaction"));
    Serial.println(F(""));
    Serial.println(F("Features:"));
    Serial.println(F("  - Modular stage-based rendering"));
    Serial.println(F("  - Composable effects"));
    Serial.println(F("  - Per-stage performance tracking"));
    Serial.println(F(""));
    Serial.println(F("Commands:"));
    Serial.println(F("  effect 11-15 - Select pipeline effect"));
    Serial.println(F("======================"));
  }
  
  void setEffect(String value) {
    int effectNum = value.toInt();
    if (effectNum >= 0 && effectNum < fxEngine.getNumEffects()) {
      fxEngine.setEffect(effectNum, 0, 800);
      Serial.print(F("Set effect to: "));
      Serial.println(effectNum);
    } else {
      Serial.println(F("Invalid effect number"));
    }
  }
  
  void setPalette(String value) {
    int paletteNum = value.toInt();
    uint8_t currentTypeCount = paletteManager.getPaletteCount();
    if (paletteNum >= 0 && paletteNum < currentTypeCount) {
      paletteManager.setPalette(paletteNum);
      Serial.print(F("Set palette to: "));
      Serial.print(paletteNum);
      Serial.print(F(" - "));
      Serial.println(paletteManager.getCurrentName());
    } else {
      Serial.print(F("Invalid palette number (0-"));
      Serial.print(currentTypeCount - 1);
      Serial.println(F(")"));
    }
  }

  void setPaletteType(String value) {
    int typeIndex = value.toInt();
    uint8_t typeCount = PaletteManager::getTypeCount();
    if (typeIndex >= 0 && typeIndex < typeCount) {
      paletteManager.setPaletteType(static_cast<PaletteManager::PaletteType>(typeIndex));
      Serial.print(F("Palette type set to: "));
      Serial.println(paletteManager.getCurrentTypeName());
      Serial.print(F("Current palette: #"));
      Serial.print(paletteManager.getCurrentIndex());
      Serial.print(F(" - "));
      Serial.println(paletteManager.getCurrentName());
    } else {
      Serial.print(F("Invalid palette type (0-"));
      Serial.print(typeCount - 1);
      Serial.println(F(")"));
    }
  }
  
  void setBrightness(String value) {
    int brightness = value.toInt();
    if (brightness >= 0 && brightness <= 255) {
      brightnessVal = brightness;
      FastLED.setBrightness(brightnessVal);
      Serial.print(F("Set brightness to: "));
      Serial.println(brightness);
    } else {
      Serial.println(F("Invalid brightness (0-255)"));
    }
  }
  
  void setFade(String value) {
    int fade = value.toInt();
    if (fade >= 0 && fade <= 255) {
      fadeAmount = fade;
      Serial.print(F("Set fade amount to: "));
      Serial.println(fade);
    } else {
      Serial.println(F("Invalid fade amount (0-255)"));
    }
  }
  
  void setSpeed(String value) {
    int speed = value.toInt();
    if (speed >= 1 && speed <= 50) {
      paletteSpeed = speed;
      Serial.print(F("Set palette speed to: "));
      Serial.println(speed);
    } else {
      Serial.println(F("Invalid speed (1-50)"));
    }
  }
  
  void setFPS(String value) {
    int newFPS = value.toInt();
    if (newFPS >= 10 && newFPS <= 120) {
      fps = newFPS;
      Serial.print(F("Set FPS to: "));
      Serial.println(newFPS);
    } else {
      Serial.println(F("Invalid FPS (10-120)"));
    }
  }
  
  void setTransition(String value) {
    int transType = value.toInt();
    if (transType >= 0 && transType <= 2) {
      const char* transNames[] = {"Fade", "Wipe", "Blend"};
      rt_transition_type = static_cast<uint8_t>(transType);
      Serial.print(F("Transition type set to: "));
      Serial.println(transNames[transType]);
    } else {
      Serial.println(F("Invalid transition type (0=Fade, 1=Wipe, 2=Blend)"));
    }
  }

  void handleGuardrail(String args) {
    args.trim();
    if (args.length() == 0 || args.equalsIgnoreCase("status")) {
      Serial.print(F("Guardrail: "));
      Serial.print(rt_enable_brown_guardrail ? F("ON") : F("OFF"));
      Serial.print(F("  G<=")); Serial.print(rt_max_g_percent_of_r);
      Serial.print(F("%, B<=")); Serial.print(rt_max_b_percent_of_r);
      Serial.println(F("%)"));
      return;
    }

    if (args.equalsIgnoreCase("on")) {
      rt_enable_brown_guardrail = true;
      Serial.println(F("Brown guardrail enabled"));
      return;
    }
    if (args.equalsIgnoreCase("off")) {
      rt_enable_brown_guardrail = false;
      Serial.println(F("Brown guardrail disabled"));
      return;
    }

    int space = args.indexOf(' ');
    if (space <= 0) {
      Serial.println(F("Usage: guardrail [on|off|status|green <0-100>|blue <0-100>]"));
      return;
    }

    String which = args.substring(0, space);
    which.trim();
    which.toLowerCase();
    String value = args.substring(space + 1);
    value.trim();
    int pct = value.toInt();
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    if (which == "green") {
      rt_max_g_percent_of_r = static_cast<uint8_t>(pct);
      Serial.print(F("Max green set to " ));
      Serial.print(pct);
      Serial.println(F("% of red"));
      rt_enable_brown_guardrail = true;
    } else if (which == "blue") {
      rt_max_b_percent_of_r = static_cast<uint8_t>(pct);
      Serial.print(F("Max blue set to " ));
      Serial.print(pct);
      Serial.println(F("% of red"));
      rt_enable_brown_guardrail = true;
    } else {
      Serial.println(F("Usage: guardrail [on|off|status|green <0-100>|blue <0-100>]"));
    }
  }

  void handleSpork(String args) {
    args.trim();
    if (args.length() == 0) args = "status";

    if (args.equalsIgnoreCase("status")) {
      Serial.println(F("\n=== PIXELSPORK ==="));
      Serial.print(F("Offset Rate (ms): ")); Serial.println(rt_spork_offset_rate_ms);
      Serial.print(F("Offset Step:      ")); Serial.println(rt_spork_offset_step);
      Serial.print(F("Grad Len Override:")); Serial.println(rt_spork_grad_len_override);
      Serial.print(F("Fade Duration (ms):")); Serial.println(rt_spork_fade_ms);
      Serial.println(F("=================="));
      return;
    }

    int space = args.indexOf(' ');
    String key = args;
    String value = "";
    if (space > 0) {
      key = args.substring(0, space);
      value = args.substring(space + 1);
      value.trim();
    }

    key.toLowerCase();
    if (key == "offset" && value.length() > 0) {
      uint16_t rate = value.toInt();
      rt_spork_offset_rate_ms = rate;
      Serial.print(F("PixelSpork offset rate set to "));
      Serial.print(rt_spork_offset_rate_ms);
      Serial.println(F(" ms"));
    } else if (key == "len" && value.length() > 0) {
      uint16_t len = value.toInt();
      rt_spork_grad_len_override = len;
      Serial.print(F("PixelSpork gradient length override set to "));
      Serial.println(rt_spork_grad_len_override);
    } else if (key == "step" && value.length() > 0) {
      uint8_t step = value.toInt();
      if (step < 1) step = 1;
      rt_spork_offset_step = step;
      Serial.print(F("PixelSpork offset step set to "));
      Serial.println(rt_spork_offset_step);
    } else if (key == "fade" && value.length() > 0) {
      uint16_t fade = value.toInt();
      if (fade < 1) fade = 1;
      rt_spork_fade_ms = fade;
      Serial.print(F("PixelSpork fade duration set to "));
      Serial.print(rt_spork_fade_ms);
      Serial.println(F(" ms"));
    } else {
      Serial.println(F("Usage: spork [status|offset <ms>|len <pixels>|step <1-8>|fade <ms>]"));
    }
  }
  
  void resetToDefaults() {
    brightnessVal = HardwareConfig::DEFAULT_BRIGHTNESS;
    fadeAmount = 20;
    paletteSpeed = 10;
    fps = 120;
    currentPaletteIndex = 0;
    FastLED.setBrightness(brightnessVal);
    fxEngine.setEffect(0, 0, 800);
    Serial.println(F("Reset to default settings"));
  }
  
  void clearScreen() {
    Serial.print(F("\033[2J\033[H")); // ANSI clear screen
    Serial.println(F("=== Light Crystals Control System ==="));
  }
};

// Global instance
extern SerialMenu serialMenu;

#endif // SERIAL_MENU_H
