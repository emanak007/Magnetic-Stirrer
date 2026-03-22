/*
  ======== User Interface (OLED Display) ========
  SSD1362 256x64, 4-wire HW SPI
  CS=10, DC=9, RES=8, MOSI=11(D1), SCK=13(D0)
*/

U8G2_SSD1362_256X64_1_4W_HW_SPI u8g2(U8G2_R0, PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RES);

// For flashing effect when editing a numeric value
unsigned long flash_timer  = 0;
bool          flash_visible = true;
const unsigned long FLASH_INTERVAL = 300; // ms

void InitialiseOLED() {
  //u8g2.setBusClock(400000); // increase communication speeeeedd!!!
  u8g2.begin();
}

// ── Idle screen ──
void DisplayIdleScreen() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.setCursor(40, 20);
    u8g2.print("MAGNETIC STIRRER");
    u8g2.setFont(u8g2_font_ncenR08_tr);
    if (settings.implicit_mode) {
      // Show what will run so user knows before placing beaker
      u8g2.setCursor(10, 40);
      u8g2.print("AUTO ");
      u8g2.print(settings.target_rpm);
      u8g2.print(" RPM");
      if (settings.stop_temp > 0) {
        u8g2.print("  Stop:");
        u8g2.print(settings.stop_temp);
        u8g2.print("C");
      }
      u8g2.setCursor(10, 55);
      u8g2.print("Place beaker to start");
    } else {
      u8g2.setCursor(50, 45);
      u8g2.print("Press any key to start");
    }
  } while (u8g2.nextPage());
}

// ── Settings screen (3-line scrolling menu) ──
void DisplaySettingsScreen() {
  // Update flash timer
  unsigned long now = millis();
  if (now - flash_timer >= FLASH_INTERVAL) {
    flash_timer = now;
    flash_visible = !flash_visible;
  }

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB10_tr);

    // Show 3 rows: highlighted-1, highlighted, highlighted+1
    for (int i = -1; i <= 1; i++) {
      int idx = highlighted + i;
      if (idx < 0 || idx >= NUM_MENU_ITEMS) continue;

      int row = i + 1; // 0=top, 1=middle, 2=bottom
      DisplayMenuItem(idx, row);
    }
  } while (u8g2.nextPage());
}

void DisplayMenuItem(int idx, int row) {
  int y_pixel = (row + 1) * 20;
  bool is_highlighted = (row == 1);

  // Check if this action item is currently flashing feedback
  bool is_action_flashing = (IsActionItem(idx) && action_flash_item == idx
                             && (millis() - action_flash_start < ACTION_FLASH_DURATION));

  // Prefix: arrow for highlighted, star for editing
  u8g2.setCursor(2, y_pixel);
  if (is_highlighted) {
    if (selected && IsNumericItem(idx)) {
      u8g2.print("* ");
    } else {
      u8g2.print("> ");
    }
  } else {
    u8g2.print("  ");
  }

  // For action items doing flash feedback, briefly invert/hide text
  if (is_action_flashing && !flash_visible) {
    return; // skip drawing this frame for flash effect
  }

  // For numeric items being edited, flash the value
  bool hide_value = (selected && is_highlighted && IsNumericItem(idx) && !flash_visible);

  // Draw name
  u8g2.print(menu_names[idx]);

  // Draw value (if applicable)
  if (IsNumericItem(idx) || IsToggleItem(idx)) {
    u8g2.print(": ");
    if (!hide_value) {
      char val_buf[16];
      GetSettingValueStr(idx, val_buf, sizeof(val_buf));
      u8g2.print(val_buf);
    }
  }
  // Action items show no value — just the name
}

// ── Stirring screen (live temp + RPM) ──
// ── Stirring screen (Live Data & Status) ──
void DisplayStirringScreen() {
  unsigned long elapsed_s = 0;
  
  if (overall_state == STIRRING) {
    elapsed_s = (millis() - stir_start_time) / 1000;
  }
  
  u8g2.firstPage();
  do {
    // --- 1. TOP STATUS BAR ---
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setCursor(0, 10);
    if (overall_state == CLAMPING) {
      u8g2.print(F("STATUS: CLAMPING BEAKER..."));
    } else {
      u8g2.print(F("STATUS: STIRRING ACTIVE"));
    }
    
    // Draw a nice horizontal line under the status
    u8g2.drawLine(0, 14, 256, 14); 

    // --- 2. MAIN RPM DISPLAY (Big Font) ---
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.setCursor(0, 34);
    u8g2.print(settings.target_rpm);
    u8g2.print(F(" RPM"));

    // --- 3. SENSOR TELEMETRY (Bottom Section) ---
    u8g2.setFont(u8g2_font_ncenB08_tr);
    
    // Column 1: Mass
    u8g2.setCursor(0, 52);
    u8g2.print(F("Mass: "));
    u8g2.print(inputs.mass_g, 1);
    u8g2.print(F(" g"));

    // Column 2: Temperature (Shifted to the right)
    u8g2.setCursor(80, 52);
    u8g2.print(F("Temp: "));
    u8g2.print(inputs.temperature_c, 1);
    // Print the little degree symbol manually
    u8g2.print(F("\xb0")); 
    u8g2.print(F("C"));

    // --- 4. TIMER ---
    u8g2.setCursor(0, 64);
    u8g2.print(F("Elapsed Time: "));
    u8g2.print(elapsed_s);
    u8g2.print(F(" s"));

  } while (u8g2.nextPage()); 
}
// ── Stopping screen (Priority 5: show reason + "STOPPING...") ──
void DisplayStoppingScreen() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB10_tr);

    u8g2.setCursor(60, 25);
    u8g2.print("STOPPING...");

    u8g2.setFont(u8g2_font_ncenR08_tr);
    u8g2.setCursor(30, 50);
    u8g2.print(StopReasonStr(stop_reason));
  } while (u8g2.nextPage());
}
