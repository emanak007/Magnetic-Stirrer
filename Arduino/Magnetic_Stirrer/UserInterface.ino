U8G2_SSD1362_256X64_1_4W_HW_SPI u8g2(U8G2_R0, /*cs=*/ 10, /*dc=*/ 9, /*reset=*/ 8);

void InitialiseOLED() {
    u8g2.begin(); 
}


void Display_setting(int idx, int row) { 
    int y_pixel = (row + 1) * 20; 
    
    u8g2.setCursor(10, y_pixel); 
    
    if (row == 1) {
        if (selected) {
            u8g2.print("* "); 
        } else {
            u8g2.print("> "); 
        }
    } else {
        u8g2.print("  "); 
    }

    u8g2.print(setting_names[idx]);

    if (idx != CONFIRM) {
        u8g2.print(": ");
    }

    if (idx == SET_SPEED) u8g2.print(settings.target_rpm);
    else if (idx == SET_TEMP) u8g2.print(settings.target_temp);
    else if (idx == SET_RAMP) u8g2.print(settings.ramp_time_ms);
    else if (idx == SET_DURATION) u8g2.print(settings.stir_duration_ms);
    else if (idx == SET_TEMP_STOP) u8g2.print(settings.stir_till_temp);
    else if (idx == SET_MASS_STOP) u8g2.print(settings.stir_till_mass);
    else if (idx == SET_CLAMP) u8g2.print(settings.clamp_enabled ? "ON" : "OFF");
    else if (idx == SET_MODE) u8g2.print(settings.implicit_mode ? "IMPLICIT" : "EXPLICIT");
}


void Display_settings() {

    u8g2.firstPage();
    do {
        u8g2.setFont(u8g2_font_ncenB10_tr); 
        
        // Top (-1), Middle (0), Bottom (1)
        for (int i = -1; i <= 1; i++) {
            int idx = highlighted + i;
          

            if (idx < 0 || idx >= NUM_SETTINGS) continue;
            
            int displayRow = i + 1; 
            
            Display_setting(idx, displayRow); 
        }
    } while (u8g2.nextPage());
}