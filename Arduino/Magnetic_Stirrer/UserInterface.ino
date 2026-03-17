U8G2_SSD1362_256X64_1_4W_HW_SPI u8g2(U8G2_R0,  /*cs=*/ 10, /*dc=*/ 9, /*reset=*/ 8);
static unsigned int cursorPos = 1;
int lineSpacing = 15;

void InitialiseOLED() {
  u8g2.begin(); // Start the display
}

int UI (int MENU, Inputs input) {
  int menuLimit = GetMenuLimit(MENU);
  cursorPos += MoveCursor(menuLimit, input.knobDir, cursorPos);
  u8g2.firstPage();
  do {
    DisplayOptions(MENU);
    DisplayCursor(cursorPos);
  } while (u8g2.nextPage());
  return MENU;
}

int GetMenuLimit(int MENU) {
  if (MENU == MENU1) return 3;
  return 1;
}

int MoveCursor(int limit, int knobDir, int pos) {
  if ((knobDir == 1) && (pos < limit)) {
    return 1; //move down
  }
  else if ((knobDir == -1) && (pos > 1)) {
    return -1; //move up
  }
  return 0;
}

void DisplayCursor(int lineNum) {
  u8g2.drawStr(0, lineNum * 15,">");
}

void DisplayOptions(int MENU) {
  if (MENU == MENU1) {
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(15,15,"Start");
    u8g2.drawStr(15,30,"Callibrate Load Sensor");
    u8g2.drawStr(15,45,"Sensor Readings");
  }
}
