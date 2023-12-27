#include "ESP32_S3_Box_3_TFT.h"

static const uint8_t ILI9341_INIT_CMD[] = {
  0xEF, 3, 0x03, 0x80, 0x02,
  0xCF, 3, 0x00, 0xC1, 0x30,
  0xED, 4, 0x64, 0x03, 0x12, 0x81,
  0xE8, 3, 0x85, 0x00, 0x78,
  0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
  0xF7, 1, 0x20,
  0xEA, 2, 0x00, 0x00,
  ILI9341_PWCTR1  , 1, 0x23,             // Power control VRH[5:0]
  ILI9341_PWCTR2  , 1, 0x10,             // Power control SAP[2:0];BT[3:0]
  ILI9341_VMCTR1  , 2, 0x3e, 0x28,       // VCM control
  ILI9341_VMCTR2  , 1, 0x86,             // VCM control2
  ILI9341_MADCTL  , 1, 0xC8,             // Memory Access Control - BGR | Y + X Mirroring...
  ILI9341_VSCRSADD, 1, 0x00,             // Vertical scroll zero
  ILI9341_PIXFMT  , 1, 0x55,
  ILI9341_FRMCTR1 , 2, 0x00, 0x18,
  ILI9341_DFUNCTR , 3, 0x08, 0x82, 0x27, // Display Function Control
  0xF2, 1, 0x00,                         // 3Gamma Function Disable
  ILI9341_GAMMASET , 1, 0x01,             // Gamma curve selected
  ILI9341_GMCTRP1 , 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, // Set Gamma
  0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
  ILI9341_GMCTRN1 , 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, // Set Gamma
  0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
  ILI9341_SLPOUT  , 0x80,                // Exit Sleep
  ILI9341_DISPON  , 0x80,                // Display on
  0x00                                   // End of list
};

ESP32S3BOX3_TFT::ESP32S3BOX3_TFT()
  : Adafruit_SPITFT(ESP32S3BOX3_TFTWIDTH, ESP32S3BOX3_TFTHEIGHT, -1, TFT_DC, -1) {

  // Set SPI to work with the TFT
  SPI.begin(TFT_CLK, TFT_MISO, TFT_MOSI, TFT_CS);
  SPI.setHwCs(true);
}

void ESP32S3BOX3_TFT::begin(uint32_t freq) {
  // Turn LCD Backlight ON
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  if (!freq) freq = BOX3_SPI_DEFAULT_FREQ; // can't use zero as frequency
  if (freq > 80000000) freq = 80000000;    // ILI9341 goes up to 80MHz
  
  initSPI(20000000, SPI_MODE0);  // configure ILI9341 using 20MHz
  displayInit();
  // change to the desired SPI Frequency
  initSPI(freq, SPI_MODE0);  
}


void ESP32S3BOX3_TFT::displayInit() {

  uint8_t x, cmd, numArgs;
  const uint8_t *addr = ILI9341_INIT_CMD;

  if (_rst < 0) {                 // If no hardware reset pin...
    sendCommand(ILI9341_SWRESET); // Engage software reset
    delay(150);
  }

  while ((cmd = *addr++) > 0) {
    x = *addr++;
    numArgs = x & 0x7F;
    sendCommand(cmd, addr, numArgs);
    addr += numArgs;
    if (x & 0x80)
      delay(150);
  }
  _width = ESP32S3BOX3_TFTWIDTH;
  _height = ESP32S3BOX3_TFTHEIGHT;
}

void ESP32S3BOX3_TFT::setAddrWindow(uint16_t x1, uint16_t y1, uint16_t w, uint16_t h) {
  static uint16_t old_x1 = 0xffff, old_x2 = 0xffff;
  static uint16_t old_y1 = 0xffff, old_y2 = 0xffff;

  uint16_t x2 = (x1 + w - 1), y2 = (y1 + h - 1);
  if (x1 != old_x1 || x2 != old_x2) {
    writeCommand(ILI9341_CASET); // Column address set
    SPI_WRITE16(x1);
    SPI_WRITE16(x2);
    old_x1 = x1;
    old_x2 = x2;
  }
  if (y1 != old_y1 || y2 != old_y2) {
    writeCommand(ILI9341_PASET); // Row address set
    SPI_WRITE16(y1);
    SPI_WRITE16(y2);
    old_y1 = y1;
    old_y2 = y2;
  }
  writeCommand(ILI9341_RAMWR); // Write to RAM
}

void ESP32S3BOX3_TFT::enableDisplay(boolean enable) {
  sendCommand(enable ? ILI9341_DISPON : ILI9341_DISPOFF);
}

void ESP32S3BOX3_TFT::enableSleep(boolean enable) {
  sendCommand(enable ? ILI9341_SLPIN : ILI9341_SLPOUT);
}

void ESP32S3BOX3_TFT::invertDisplay(bool invert) {
  sendCommand(invert ? ILI9341_INVON : ILI9341_INVOFF);
}

void ESP32S3BOX3_TFT::scrollTo(uint16_t y) {
  uint8_t data[2];
  data[0] = y >> 8;
  data[1] = y & 0xff;
  sendCommand(ILI9341_VSCRSADD, (uint8_t *)data, 2);
}

void ESP32S3BOX3_TFT::setScrollMargins(uint16_t top, uint16_t bottom) {
  // TFA+VSA+BFA must equal 320
  if (top + bottom <= ILI9341_TFTHEIGHT) {
    uint16_t middle = ILI9341_TFTHEIGHT - (top + bottom);
    uint8_t data[6];
    data[0] = top >> 8;
    data[1] = top & 0xff;
    data[2] = middle >> 8;
    data[3] = middle & 0xff;
    data[4] = bottom >> 8;
    data[5] = bottom & 0xff;
    sendCommand(ILI9341_VSCRDEF, (uint8_t *)data, 6);
  }
}


void ESP32S3BOX3_TFT::setRotation(uint8_t m) {
  rotation = m % 4; // can't be higher than 3
  switch (rotation) {
    case 0:
      m = (MADCTL_MY | MADCTL_MX | MADCTL_BGR);
      _width = ESP32S3BOX3_TFTWIDTH;
      _height = ESP32S3BOX3_TFTHEIGHT;
      break;
    case 1:
      m = (MADCTL_MY | MADCTL_MV | MADCTL_BGR);
      _width = ESP32S3BOX3_TFTHEIGHT;
      _height = ESP32S3BOX3_TFTWIDTH;
      break;
    case 2:
      m = (MADCTL_BGR);
      _width = ESP32S3BOX3_TFTWIDTH;
      _height = ESP32S3BOX3_TFTHEIGHT;
      break;
    case 3:
      m = (MADCTL_MX | MADCTL_MV | MADCTL_BGR);
      _width = ESP32S3BOX3_TFTHEIGHT;
      _height = ESP32S3BOX3_TFTWIDTH;
      break;
  }

  sendCommand(ILI9341_MADCTL, &m, 1);
}

