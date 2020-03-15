#include "GLCD.h"
#include "GLCD_font.h"
#include "GLCD_font32.h"

//フォント部分等
//https://synapse.kyoto/hard/MGLCD_AQM1248A/page010.html

extern const uint8_t FontBmp1[][5] PROGMEM;
extern const uint8_t FontBmp2[][5] PROGMEM;

// ST7565Rからデータが取得できない前提の裏グラフィックRAM
static uint8_t VRAM[LCD_WIDTH][LCD_HEIGHT / 8];
static uint8_t *userFont[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int kfind(uint8_t c1, uint8_t c2, uint8_t c3)
{
  int pmin, pmax, pcur;
  uint32_t code, _code;
  code = ((uint32_t)c1 << 16) + ((uint32_t)c2 << 8) + (uint32_t)c3;
  pmin = 0;
  pmax = KF_SIZE - 1;
  while(pmin < pmax) {
    pcur = (pmin + pmax) / 2;
    const uint8_t *p = kfont[pcur];
    uint8_t _c1 = pgm_read_byte(p + 0);
    uint8_t _c2 = pgm_read_byte(p + 1);
    uint8_t _c3 = pgm_read_byte(p + 2);
    _code = ((uint32_t)_c1 << 16) + ((uint32_t)_c2 << 8) + (uint32_t)_c3;
    if (code == _code)
      return pcur;
    if (code < _code)
      pmax = pcur;
    else
      pmin = pcur + 1;
  }
  return -1;
}

uint8_t GLCD::kprint(uint8_t x, uint8_t y, char *str, uint8_t one, uint8_t zero)
{
  uint8_t c1, c2, c3;
  int f;
  c1 = c2 = c3 = 0;
  while(*str) {
    c1 = c2;
    c2 = c3;
    c3 = *str++;
    if (0xce <= c3 && c3 <= 0xd1) {
      c2 = c3;
      c3 = *str++;
    } else
    if (0xe2 <= c3 && c3 <= 0xef) {
      c1 = c3;
      c2 = *str++;
      c3 = *str++;
    }
    f = kfind(c1, c2, c3);
    if (f < 0) {
      c1 = c2 = c3 = 0;
      continue;
    }
    const uint8_t *p = kfont[f];
    for(int i = 0; i < 12; i++) {
      uint8_t c = pgm_read_byte(p + i + 4);
      for(int k = 0; k < 8; k++)
        SetPixel(x + 7 - k, y + i, (c & (1 << k)) != 0 ? one : zero);
    }
    x += pgm_read_byte(p + 3);
    c1 = c2 = c3 = 0;
  }
  return x;
}

static int kfind32(uint8_t c1, uint8_t c2, uint8_t c3)
{
  int pmin, pmax, pcur;
  uint32_t code, _code;
  code = ((uint32_t)c1 << 16) + ((uint32_t)c2 << 8) + (uint32_t)c3;
  pmin = 0;
  pmax = KF_SIZE32 - 1;
  while(pmin < pmax) {
    pcur = (pmin + pmax) / 2;
    const uint32_t *p = kfont32[pcur];
    _code = pgm_read_dword(p + 0);
    if (code == _code)
      return pcur;
    if (code < _code)
      pmax = pcur;
    else
      pmin = pcur + 1;
  }
  return -1;
}

uint8_t GLCD::kprint32(uint8_t x, uint8_t y, char *str, uint8_t one, uint8_t zero)
{
  uint8_t c1, c2, c3;
  int f;
  c1 = c2 = c3 = 0;
  while(*str) {
    c1 = c2;
    c2 = c3;
    c3 = *str++;
    if (0xce <= c3 && c3 <= 0xd1) {
      c2 = c3;
      c3 = *str++;
    } else
    if (0xe2 <= c3 && c3 <= 0xef) {
      c1 = c3;
      c2 = *str++;
      c3 = *str++;
    }
    f = kfind32(c1, c2, c3);
    if (f < 0) {
      c1 = c2 = c3 = 0;
      continue;
    }
    const uint32_t *p = kfont32[f];
    uint8_t w = (uint8_t)pgm_read_dword(p + 1);
    for(int i = 0; i < 32; i++) {
      uint32_t c = pgm_read_dword(p + i + 2);
      for(int k = 0; k < w; k++)
        SetPixel(x + w - 1 - k, y + i, (c & (1 << k)) != 0 ? one : zero);
    }
    x += w;
    c1 = c2 = c3 = 0;
  }
  return x;
}

GLCD::GLCD()
{
  x0 = y0 = 0;
  cs = 5;
  rs = 12;
  sck = 14;
  mosi = 13;
}

void GLCD::begin(uint8_t _cs, uint8_t _rs, uint8_t _sck, uint8_t _mosi)
{
  cs = _cs;
  rs = _rs;
  sck = _sck;
  mosi = _mosi;
  pinMode(cs,   OUTPUT);      
  pinMode(rs,   OUTPUT);      
  pinMode(mosi, OUTPUT);    
  pinMode(sck,  OUTPUT);

  sendCmd(0xe2);     // reset

  sendCmd(0xae);     // Display: OFF
  sendCmd(0xa0);     // ADC select: normal
  sendCmd(0xc8);     // Common output mode: reverse
  sendCmd(0xa2);     // LCD bias: 1/9
  // 内部レギュレータを順番にON
  sendCmd(0x2c);     // Power control 1 booster: ON
  delay(2);
  sendCmd(0x2e);     // Power control 2 Voltage regulator circuit: ON
  delay(2);
  sendCmd(0x2f);     // Power control 3 Voltage follower circuit: ON
  // V0 Voltage Regulator Internal Resistor Ratio Set
  // 0x20 - 0x27
  // AQM1248A-FLW-FBW の場合0x25 または 0x26
  sendCmd(0x26);

  sendCmd(0x81);     // The Electronic Volume Mode Set
  // Electronic Volume Register Set 0x01-0x3f
  // AQM1248A-FLW-FBW の場合0x01 または 0x08
  sendCmd(0x01);

  // 表示設定
  sendCmd(0xa4);     // Display all points: OFF
  sendCmd(0x40);     // Display start address: 0x00
  sendCmd(0xa6);     // Display: normal
  sendCmd(0xaf);     // Display: ON
/*
  sendCmd(0xA2);     // 1/9 BIAS
  sendCmd(0xA0);     // ADC SELECT , NORMAL
  sendCmd(0xC8);     // COM OUTPUT REVERSE
  sendCmd(0xA4);     // DISPLAY ALL POINTS NORMAL
  sendCmd(0x40);     // DISPLAY START LINE SET
  sendCmd(0x25);     // INTERNAL RESISTOR RATIO
  sendCmd(0x81);     // ELECTRONIC VOLUME MODE SET
  sendCmd(0x10);     // ELECTRONIC VOLUME
  sendCmd(0x2F);     // POWER CONTROLLER SET
  sendCmd(0xAF);     // DISPLAY ON
*/

  clear();
}

void GLCD::sendCmd(uint8_t cmd)
{
  write(LOW, cmd);
}

void GLCD::sendData(uint8_t data)
{
  write(HIGH, data);
  VRAM[x0++][y0] = data;
  if (x0 >= LCD_WIDTH)
    x0 = LCD_WIDTH - 1;
}

void GLCD::write(uint8_t direction, uint8_t data)
{
  digitalWrite(cs, LOW);
  digitalWrite(rs, direction);
  shiftOut(MOSI, SCK, MSBFIRST, data);  
  digitalWrite(cs, HIGH);
}

void GLCD::FontPos(uint8_t x, uint8_t y)
{
  fx0 = x;
  fy0 = y;
}

void GLCD::put(char _c, bool reverse)
{
  const uint8_t *p;
  uint8_t c = (uint8_t)_c;
  int n;
  sendCmd(0xb0 + fy0);
  n = fx0 * 6;
  x0 = n;
  y0 = fy0;
  sendCmd(0x10 + ((n >> 4) & 0x0f));
  sendCmd(0x00 + (n & 0x0f));

  if (c < 0x08) {
    if (userFont[c] == 0)
      p = FontBmp1[0];
    else
      p = 0;
  } else
  if (c < 0x20)
    p = FontBmp1[0];
  else if (c < 0x80)
    p = FontBmp1[c - 0x20];
  else if (c < 0xa0)
    p = FontBmp1[0];
  else if (c < 0xe0)
    p = FontBmp2[c - 0xa0];
  else
    p = FontBmp1[0];

  if (p != 0) {
    if (!reverse) {
      for(int i = 0; i < 5; i++)
        sendData(pgm_read_byte(p + i));
      sendData(0);
    } else {
      for(int i = 0; i < 5; i++)
        sendData(~pgm_read_byte(p + i));
      sendData(0xff);
    }
  } else {
    p = userFont[c];
    if (!reverse) {
      for(int i = 0; i < 6; i++)
        sendData(p[i]);
    } else {
      for(int i = 0; i < 6; i++)
        sendData(~p[i]);
    }
  }

  if (fx0 * 6 < LCD_WIDTH)
    fx0++;
}

void GLCD::print(char *str, bool reverse)
{
  const uint8_t *p;
  uint8_t c, b;
  while(c = *str++) {
    if (c < 0x08) {
      if (userFont[c] == 0)
        p = FontBmp1[0];
      else
        p = 0;
    } else
    if (c < 0x20)
      p = FontBmp1[0];
    else if (c < 0x80)
      p = FontBmp1[c - 0x20];
    else if (c < 0xa0)
      p = FontBmp1[0];
    else if (c < 0xe0){
      p = FontBmp2[c - 0xa0];
    } else if (c == 0xef) {
      uint16_t w = (((uint8_t)str[0]) << 8) + (uint8_t)str[1];
      if (0xbda1 <= w && w <= 0xbdbf) { // 半角カナの範囲
        c = (uint8_t)(w - 0xbda1 + 0xa1);
        p = FontBmp2[c - 0xa0];
      } else if (0xbe80 <= w && w <= 0xbe9f) { // 半角カナの範囲
        c = (uint8_t)(w - 0xbe80 + 0xc0);
        p = FontBmp2[c - 0xa0];
      } else {
        p = FontBmp1[0];
      }
      str += 2;
    }
    else
      p = FontBmp1[0];

    for(uint8_t x = 0; x < 5; x++) {
      if (p != 0)
        b = pgm_read_byte(p + x);
      else
        b = userFont[c][x];
      for(uint8_t y = 0; y < 8; y++) {
        if (!reverse)
          SetPixel(fx0 + x, fy0 + y, (b & (1 << y)) ? GLCD_BLACK : GLCD_WHITE);
        else
          SetPixel(fx0 + x, fy0 + y, (b & (1 << y)) ? GLCD_WHITE : GLCD_BLACK);
      }
    }

    if (fx0 < LCD_WIDTH)
      fx0 += 6;
  }
}

bool GLCD::SetPixel(uint8_t x, uint8_t y, uint8_t color)
{
  uint8_t page;
  uint8_t data;
  
  if (x >= LCD_WIDTH)
    return false;
  if (y >= LCD_HEIGHT)
    return false;
  if (color == GLCD_CLEAR)
    return false;
  page = (y >> 3); // calculate page address
  if (color)
    data = VRAM[x][page] | (1 << (y & 0x7));
  else
    data = VRAM[x][page] & (~(1 << (y & 0x7)));
  VRAM[x][page] = data;
  x0 = x;
  y0 = page;
  if (background == 0) {
    sendCmd(0xb0 + page);  // Page address set
    sendCmd(0x10 + (x >> 4));  // Column address set upper bit
    sendCmd(0x00 + (x & 0xf)); // Column address set lower bit
    sendData(data);    // write bitmap data
  }
  return true;
}

int8_t GLCD::Line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color)
{
  uint8_t ybig, temp, x, y;
  
  if (x1 >= LCD_WIDTH || y1 >= LCD_HEIGHT ||
      x2 >= LCD_WIDTH || y2 >= LCD_HEIGHT ||
      color > 1) return -1;
  ybig = abs(y2 - y1) > abs(x2 - x1);
  if (!ybig) {
    if (x1 > x2) {
      temp = x1; x1 = x2; x2 = temp;
      temp = y1; y1 = y2; y2 = temp;
    }
    for(x = x1; x < x2; x++) {
      y = ((int)(x - x1)) * (y2 - y1) / (x2 - x1) + y1;
      SetPixel(x, y, color);
    }
  } else {
    if (y1 > y2) {
      temp = x1; x1 = x2; x2 = temp;
      temp = y1; y1 = y2; y2 = temp;
    }
    for(y = y1; y < y2; y++) {
      x = ((int)(y - y1)) * (x2 - x1) / (y2 - y1) + x1;
      SetPixel(x, y, color);
    }
  }
  SetPixel(x1, y2, color);
  return 0;
}

bool GLCD::setFont(uint8_t no, uint8_t *font)
{
  if (no >= sizeof(userFont) / sizeof(userFont[0]))
    return false;
  userFont[no] = font;
  return true;
}

void GLCD::Rectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t border, int8_t fill)
{
  uint8_t x, y, pat1, pat2, pat3, pat4, p, p1, p2;
  if (x1 > x2) { uint8_t t = x1; x1 = x2; x2 = t; }
  if (y1 > y2) { uint8_t t = y1; y1 = y2; y2 = t; }
  pat2 = 0x80 >> (7 - (y1 % 8));
  pat1 = 0 - pat2;
  pat4 = 0x80 >> (7 - (y2 % 8));
  pat3 = (uint8_t)(((uint16_t)pat4 << 1) - 1);

  p1 = y1 >> 3;
  p2 = y2 >> 3;
  if (p1 == p2) {
    pat1 = pat1 & pat3;
    pat3 = pat1;
    pat2 = pat2 & pat4;
    pat4 = pat2;
  }
  sendCmd(0xb0 + p1);         // Page address set
  // 左上
  sendCmd(0x10 + (x1 >> 4));  // Column address set upper bit
  sendCmd(0x00 + (x1 & 0xf)); // Column address set lower bit
  if (border == GLCD_BLACK) {
    VRAM[x1][p1] |= pat1;
    sendData(VRAM[x1][p1]);
  } else {
    VRAM[x1][p1] &= ~pat1;
    sendData(VRAM[x1][p1]);
  }
  for(x = x1 + 1; x < x2; x++) {
    // 上
    if (border == GLCD_BLACK) {
      VRAM[x][p1] |= pat2;
      sendData(VRAM[x][p1]);
    } else {
      VRAM[x][p1] &= ~pat2;
      sendData(VRAM[x][p1]);
    }
  }
  if (border == GLCD_BLACK) {
    VRAM[x2][p1] |= pat1;
    sendData(VRAM[x1][p1]);
  } else {
    VRAM[x2][p1] &= ~pat1;
    sendData(VRAM[x1][p1]);
  }

  sendCmd(0xb0 + p2);         // Page address set
  // 左下
  sendCmd(0x10 + (x1 >> 4));  // Column address set upper bit
  sendCmd(0x00 + (x1 & 0xf)); // Column address set lower bit
  if (border == GLCD_BLACK) {
    VRAM[x1][p2] |= pat3;
    sendData(VRAM[x1][p2]);
  } else {
    VRAM[x1][p2] &= ~pat3;
    sendData(VRAM[x1][p2]);
  }
  for(x = x1 + 1; x < x2; x++) {
    // 下
    if (border == GLCD_BLACK) {
      VRAM[x][p2] |= pat4;
      sendData(VRAM[x][p2]);
    } else {
      VRAM[x][p2] &= ~pat4;
      sendData(VRAM[x][p2]);
    }
  }
  if (border == GLCD_BLACK) {
    VRAM[x2][p2] |= pat3;
    sendData(VRAM[x1][p2]);
  } else {
    VRAM[x2][p1] &= ~pat3;
    sendData(VRAM[x2][p2]);
  }

  for(p = p1 + 1; p < p2; p++) {
    sendCmd(0xb0 + p);          // Page address set
    sendCmd(0x10 + (x1 >> 4));  // Column address set upper bit
    sendCmd(0x00 + (x1 & 0xf)); // Column address set lower bit
    if (border == GLCD_BLACK) { // 左
      VRAM[x1][p] = 0xff;
      sendData(VRAM[x1][p]);
    } else {
      VRAM[x1][p] = 0x00;
      sendData(VRAM[x1][p]);
    }
    sendCmd(0x10 + (x2 >> 4));  // Column address set upper bit
    sendCmd(0x00 + (x2 & 0xf)); // Column address set lower bit
    if (border == GLCD_BLACK) { // 右
      VRAM[x2][p] = 0xff;
      sendData(VRAM[x2][p]);
    } else {
      VRAM[x2][p] = 0x00;
      sendData(VRAM[x2][p]);
    }
  }

  if (fill == GLCD_CLEAR)
    return;
  x1++;
  x2--;
  if (x1 > x2)
    return;
  y1++;
//  y2--;
  if (y1 > y2)
    return;
  pat2 = 0x80 >> (7 - (y1 % 8));
  pat1 = 0 - pat2;
  pat4 = 0x80 >> (7 - (y2 % 8));
  pat3 = (uint8_t)((uint16_t)pat4 - 1);
  p1 = y1 >> 3;
  p2 = y2 >> 3;
  if (p1 == p2) {
    pat1 = pat1 & pat3;
    pat3 = pat1;
    pat2 = pat2 & pat4;
    pat4 = pat2;
  }

  sendCmd(0xb0 + p1);         // Page address set
  // 左上
  sendCmd(0x10 + (x1 >> 4));  // Column address set upper bit
  sendCmd(0x00 + (x1 & 0xf)); // Column address set lower bit
  for(x = x1; x <= x2; x++) {
    if (fill == GLCD_BLACK) {
      VRAM[x][p1] |= pat1;
      sendData(VRAM[x][p1]);
    } else {
      VRAM[x][p1] &= ~pat1;
      sendData(VRAM[x][p1]);
    }
  }
  sendCmd(0xb0 + p2);         // Page address set
  sendCmd(0x10 + (x1 >> 4));  // Column address set upper bit
  sendCmd(0x00 + (x1 & 0xf)); // Column address set lower bit
  for(x = x1; x <= x2; x++) {
    if (fill == GLCD_BLACK) {
      VRAM[x][p2] |= pat3;
      sendData(VRAM[x][p2]);
    } else {
      VRAM[x][p2] &= ~pat3;
      sendData(VRAM[x][p2]);
    }
  }

  for(p = p1 + 1; p < p2; p++) {
    sendCmd(0xb0 + p);          // Page address set
    sendCmd(0x10 + (x1 >> 4));  // Column address set upper bit
    sendCmd(0x00 + (x1 & 0xf)); // Column address set lower bit
    for(x = x1; x <= x2; x++) {
      if (fill == GLCD_BLACK) { // 左
        VRAM[x][p] = 0xff;
        sendData(VRAM[x][p]);
      } else {
        VRAM[x][p] = 0x00;
        sendData(VRAM[x][p]);
      }
    }
  }
}

void GLCD::clear()
{
  for(int y = 0; y < (LCD_HEIGHT + 7) / 8; y++) {
    for(int x = 0; x < LCD_WIDTH; x++) {
      VRAM[x][y] = 0;
    }
  }
  if (background == 0)
    endBackGroundDraw();
}

void GLCD::beginBackGroundDraw()
{
  background = 1;
}

void GLCD::endBackGroundDraw()
{
  background = 0;
  for(int y = 0; y < (LCD_HEIGHT + 7) / 8; y++) {
    sendCmd(0xb0 + y);
    sendCmd(0x10);
    sendCmd(0x00);
    for(int x = 0; x < LCD_WIDTH; x++)
      sendData(VRAM[x][y]);
  }
}

#define s ((((((((((((((((0
#define M <<1)+1)
#define _ <<1))

const uint8_t FontBmp1[][5] PROGMEM =
{
  { // 20
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 21
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ M _ _ M M M M  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 22
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ M M M  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ M M M  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 23
    s  _ _ _ M _ M _ _  ,
    s  _ M M M M M M M  ,
    s  _ _ _ M _ M _ _  ,
    s  _ M M M M M M M  ,
    s  _ _ _ M _ M _ _  
  },
  { // 24
    s  _ _ M _ _ M _ _  ,
    s  _ _ M _ M _ M _  ,
    s  _ M M M M M M M  ,
    s  _ _ M _ M _ M _  ,
    s  _ _ _ M _ _ M _  
  },
  { // 25
    s  _ _ M _ _ _ M M  ,
    s  _ _ _ M _ _ M M  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ M M _ _ M _ _  ,
    s  _ M M _ _ _ M _  
  },
  { // 26
    s  _ _ M M _ M M _  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ M _ M _ M  ,
    s  _ _ M _ _ _ M _  ,
    s  _ M _ M _ _ _ _  
  },
  { // 27
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ M _ M  ,
    s  _ _ _ _ _ _ M M  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 28
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ M M M _ _  ,
    s  _ _ M _ _ _ M _  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 29
    s  _ _ _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ _ M _ _ _ M _  ,
    s  _ _ _ M M M _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 2A
    s  _ _ _ M _ M _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ M M M M M _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ M _ M _ _  
  },
  { // 2B
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ M M M M M _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ M _ _ _  
  },
  { // 2C
    s  _ _ _ _ _ _ _ _  ,
    s  _ M _ M _ _ _ _  ,
    s  _ _ M M _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 2D
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ M _ _ _  
  },
  { // 2E
    s  _ _ _ _ _ _ _ _  ,
    s  _ M M _ _ _ _ _  ,
    s  _ M M _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 2F
    s  _ _ M _ _ _ _ _  ,
    s  _ _ _ M _ _ _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ _ _ M _  
  },
  { // 30
    s  _ _ M M M M M _  ,
    s  _ M _ M _ _ _ M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ _ _ M _ M  ,
    s  _ _ M M M M M _  
  },
  { // 31
    s  _ _ _ _ _ _ _ _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M M M M M M M  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 32
    s  _ M _ _ _ _ M _  ,
    s  _ M M _ _ _ _ M  ,
    s  _ M _ M _ _ _ M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ _ _ M M _  
  },
  { // 33
    s  _ _ M _ _ _ _ M  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ M _ _ _ M _ M  ,
    s  _ M _ _ M _ M M  ,
    s  _ _ M M _ _ _ M  
  },
  { // 34
    s  _ _ _ M M _ _ _  ,
    s  _ _ _ M _ M _ _  ,
    s  _ _ _ M _ _ M _  ,
    s  _ M M M M M M M  ,
    s  _ _ _ M _ _ _ _  
  },
  { // 35
    s  _ _ M _ _ M M M  ,
    s  _ M _ _ _ M _ M  ,
    s  _ M _ _ _ M _ M  ,
    s  _ M _ _ _ M _ M  ,
    s  _ _ M M M _ _ M  
  },
  { // 36
    s  _ _ M M M M _ _  ,
    s  _ M _ _ M _ M _  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ _ M M _ _ _ _  
  },
  { // 37
    s  _ _ _ _ _ _ _ M  ,
    s  _ M M M _ _ _ M  ,
    s  _ _ _ _ M _ _ M  ,
    s  _ _ _ _ _ M _ M  ,
    s  _ _ _ _ _ _ M M  
  },
  { // 38
    s  _ _ M M _ M M _  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ _ M M _ M M _  
  },
  { // 39
    s  _ _ _ _ _ M M _  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ _ M _ M _ _ M  ,
    s  _ _ _ M M M M _  
  },
  { // 3A
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ M M _ M M _  ,
    s  _ _ M M _ M M _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 3B
    s  _ _ _ _ _ _ _ _  ,
    s  _ M _ M _ M M _  ,
    s  _ _ M M _ M M _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 3C
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ M _ M _ _  ,
    s  _ _ M _ _ _ M _  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 3D
    s  _ _ _ M _ M _ _  ,
    s  _ _ _ M _ M _ _  ,
    s  _ _ _ M _ M _ _  ,
    s  _ _ _ M _ M _ _  ,
    s  _ _ _ M _ M _ _  
  },
  { // 3E
    s  _ _ _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ _ M _ _ _ M _  ,
    s  _ _ _ M _ M _ _  ,
    s  _ _ _ _ M _ _ _  
  },
  { // 3F
    s  _ _ _ _ _ _ M _  ,
    s  _ _ _ _ _ _ _ M  ,
    s  _ M _ M _ _ _ M  ,
    s  _ _ _ _ M _ _ M  ,
    s  _ _ _ _ _ M M _  
  },
  { // 40
    s  _ _ M M _ _ M _  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M M M M _ _ M  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ _ M M M M M _  
  },
  { // 41
    s  _ M M M M M M _  ,
    s  _ _ _ M _ _ _ M  ,
    s  _ _ _ M _ _ _ M  ,
    s  _ _ _ M _ _ _ M  ,
    s  _ M M M M M M _  
  },
  { // 42
    s  _ M M M M M M M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ _ M M _ M M _  
  },
  { // 43
    s  _ _ M M M M M _  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ _ M _ _ _ M _  
  },
  { // 44
    s  _ M M M M M M M  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ _ M _ _ _ M _  ,
    s  _ _ _ M M M _ _  
  },
  { // 45
    s  _ M M M M M M M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ _ _ _ _ M  
  },
  { // 46
    s  _ M M M M M M M  ,
    s  _ _ _ _ M _ _ M  ,
    s  _ _ _ _ M _ _ M  ,
    s  _ _ _ _ M _ _ M  ,
    s  _ _ _ _ _ _ _ M  
  },
  { // 47
    s  _ _ M M M M M _  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M M M M _ M _  
  },
  { // 48
    s  _ M M M M M M M  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ M M M M M M M  
  },
  { // 49
    s  _ _ _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ M M M M M M M  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 4A
    s  _ _ M _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ _ M M M M M M  ,
    s  _ _ _ _ _ _ _ M  
  },
  { // 4B
    s  _ M M M M M M M  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ M _ M _ _  ,
    s  _ _ M _ _ _ M _  ,
    s  _ M _ _ _ _ _ M  
  },
  { // 4C
    s  _ M M M M M M M  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  
  },
  { // 4D
    s  _ M M M M M M M  ,
    s  _ _ _ _ _ _ M _  ,
    s  _ _ _ _ M M _ _  ,
    s  _ _ _ _ _ _ M _  ,
    s  _ M M M M M M M  
  },
  { // 4E
    s  _ M M M M M M M  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ M _ _ _ _  ,
    s  _ M M M M M M M  
  },
  { // 4F
    s  _ _ M M M M M _  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ _ M M M M M _  
  },
  { // 50
    s  _ M M M M M M M  ,
    s  _ _ _ _ M _ _ M  ,
    s  _ _ _ _ M _ _ M  ,
    s  _ _ _ _ M _ _ M  ,
    s  _ _ _ _ _ M M _  
  },
  { // 51
    s  _ _ M M M M M _  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ M _ M _ _ _ M  ,
    s  _ _ M _ _ _ _ M  ,
    s  _ M _ M M M M _  
  },
  { // 52
    s  _ M M M M M M M  ,
    s  _ _ _ _ M _ _ M  ,
    s  _ _ _ M M _ _ M  ,
    s  _ _ M _ M _ _ M  ,
    s  _ M _ _ _ M M _  
  },
  { // 53
    s  _ M _ _ _ M M _  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ _ M M _ _ _ M  
  },
  { // 54
    s  _ _ _ _ _ _ _ M  ,
    s  _ _ _ _ _ _ _ M  ,
    s  _ M M M M M M M  ,
    s  _ _ _ _ _ _ _ M  ,
    s  _ _ _ _ _ _ _ M  
  },
  { // 55
    s  _ _ M M M M M M  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M M M M M M  
  },
  { // 56
    s  _ _ _ M M M M M  ,
    s  _ _ M _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M _ _ _ _ _  ,
    s  _ _ _ M M M M M  
  },
  { // 57
    s  _ _ M M M M M M  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M M M _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M M M M M M  
  },
  { // 58
    s  _ M M _ _ _ M M  ,
    s  _ _ _ M _ M _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ M _ M _ _  ,
    s  _ M M _ _ _ M M  
  },
  { // 59
    s  _ _ _ _ _ M M M  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ M M M _ _ _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ _ M M M  
  },
  { // 5A
    s  _ M M _ _ _ _ M  ,
    s  _ M _ M _ _ _ M  ,
    s  _ M _ _ M _ _ M  ,
    s  _ M _ _ _ M _ M  ,
    s  _ M _ _ _ _ M M  
  },
  { // 5B
    s  _ _ _ _ _ _ _ _  ,
    s  _ M M M M M M M  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 5C
    s  _ _ _ _ _ _ M _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ M _ _ _ _  ,
    s  _ _ M _ _ _ _ _  
  },
  { // 5D
    s  _ _ _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ M M M M M M M  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 5E
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ _ _ M _  ,
    s  _ _ _ _ _ _ _ M  ,
    s  _ _ _ _ _ _ M _  ,
    s  _ _ _ _ _ M _ _  
  },
  { // 5F
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  
  },
  { // 60
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ M  ,
    s  _ _ _ _ _ _ M _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 61
    s  _ _ M _ _ _ _ _  ,
    s  _ M _ M _ M _ _  ,
    s  _ M _ M _ M _ _  ,
    s  _ M _ M _ M _ _  ,
    s  _ M M M M _ _ _  
  },
  { // 62
    s  _ M M M M M M M  ,
    s  _ M _ _ M _ _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ _ M M M _ _ _  
  },
  { // 63
    s  _ _ M M M _ _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ _ M _ _ _ _ _  
  },
  { // 64
    s  _ _ M M M _ _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ M _ _ M _ _ _  ,
    s  _ M M M M M M M  
  },
  { // 65
    s  _ _ M M M _ _ _  ,
    s  _ M _ M _ M _ _  ,
    s  _ M _ M _ M _ _  ,
    s  _ M _ M _ M _ _  ,
    s  _ _ _ M M _ _ _  
  },
  { // 66
    s  _ _ _ _ M _ _ _  ,
    s  _ M M M M M M _  ,
    s  _ _ _ _ M _ _ M  ,
    s  _ _ _ _ _ _ _ M  ,
    s  _ _ _ _ _ _ M _  
  },
  { // 67
    s  _ _ _ _ M M _ _  ,
    s  _ M _ M _ _ M _  ,
    s  _ M _ M _ _ M _  ,
    s  _ M _ M _ _ M _  ,
    s  _ _ M M M M M _  
  },
  { // 68
    s  _ M M M M M M M  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ M M M M _ _ _  
  },
  { // 69
    s  _ _ _ _ _ _ _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ M M M M M _ M  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 6A
    s  _ _ M _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ _ M M M M _ M  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 6B
    s  _ M M M M M M M  ,
    s  _ _ _ M _ _ _ _  ,
    s  _ _ M _ M _ _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 6C
    s  _ _ _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ M M M M M M M  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 6D
    s  _ M M M M M _ _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ M M _ _ _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ M M M M _ _ _  
  },
  { // 6E
    s  _ M M M M M _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ M M M M _ _ _  
  },
  { // 6F
    s  _ _ M M M _ _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ _ M M M _ _ _  
  },
  { // 70
    s  _ M M M M M _ _  ,
    s  _ _ _ M _ M _ _  ,
    s  _ _ _ M _ M _ _  ,
    s  _ _ _ M _ M _ _  ,
    s  _ _ _ _ M _ _ _  
  },
  { // 71
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ M _ M _ _  ,
    s  _ _ _ M _ M _ _  ,
    s  _ _ _ M M _ _ _  ,
    s  _ M M M M M _ _  
  },
  { // 72
    s  _ M M M M M _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ M _ _ _  
  },
  { // 73
    s  _ M _ _ M _ _ _  ,
    s  _ M _ M _ M _ _  ,
    s  _ M _ M _ M _ _  ,
    s  _ M _ M _ M _ _  ,
    s  _ _ M _ _ _ _ _  
  },
  { // 74
    s  _ _ _ _ _ M _ _  ,
    s  _ _ M M M M M M  ,
    s  _ M _ _ _ M _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M _ _ _ _ _  
  },
  { // 75
    s  _ _ M M M M _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M _ _ _ _ _  ,
    s  _ M M M M M _ _  
  },
  { // 76
    s  _ _ _ M M M _ _  ,
    s  _ _ M _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M _ _ _ _ _  ,
    s  _ _ _ M M M _ _  
  },
  { // 77
    s  _ _ M M M M _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M M _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M M M M _ _  
  },
  { // 78
    s  _ M _ _ _ M _ _  ,
    s  _ _ M _ M _ _ _  ,
    s  _ _ _ M _ _ _ _  ,
    s  _ _ M _ M _ _ _  ,
    s  _ M _ _ _ M _ _  
  },
  { // 79
    s  _ _ _ _ M M _ _  ,
    s  _ M _ M _ _ _ _  ,
    s  _ M _ M _ _ _ _  ,
    s  _ M _ M _ _ _ _  ,
    s  _ _ M M M M _ _  
  },
  { // 7A
    s  _ M _ _ _ M _ _  ,
    s  _ M M _ _ M _ _  ,
    s  _ M _ M _ M _ _  ,
    s  _ M _ _ M M _ _  ,
    s  _ M _ _ _ M _ _  
  },
  { // 7B
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ M M _ M M _  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 7C
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ M M M M M M M  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 7D
    s  _ _ _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ _ M M _ M M _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // 7E
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ M _ _ _ _  ,
    s  _ _ _ _ M _ _ _  
  },
  { // 7F
    s  _ _ _ _ _ _ _ M  ,
    s  _ _ M _ M _ M _  ,
    s  _ M M M M M _ _  ,
    s  _ _ M _ M _ M _  ,
    s  _ _ _ _ _ _ _ M  
  }
};

const uint8_t FontBmp2[][5] PROGMEM =
{
  { // A0
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // A1
    s  _ M M M _ _ _ _  ,
    s  _ M _ M _ _ _ _  ,
    s  _ M M M _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // A2
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ M M M M  ,
    s  _ _ _ _ _ _ _ M  ,
    s  _ _ _ _ _ _ _ M  
  },
  { // A3
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ M M M M _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // A4
    s  _ _ _ M _ _ _ _  ,
    s  _ _ M _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // A5
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ M M _ _ _  ,
    s  _ _ _ M M _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // A6
    s  _ _ _ _ M _ M _  ,
    s  _ _ _ _ M _ M _  ,
    s  _ M _ _ M _ M _  ,
    s  _ _ M _ M _ M _  ,
    s  _ _ _ M M M M _  
  },
  { // A7
    s  _ _ _ _ _ M _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ _ M M _ M _ _  ,
    s  _ _ _ M _ M _ _  ,
    s  _ _ _ _ M M _ _  
  },
  { // A8
    s  _ _ M _ _ _ _ _  ,
    s  _ _ _ M _ _ _ _  ,
    s  _ M M M M _ _ _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // A9
    s  _ _ _ M M _ _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ M _ _ M M _ _  ,
    s  _ M _ _ M _ _ _  ,
    s  _ _ M M M _ _ _  
  },
  { // AA
    s  _ M _ _ M _ _ _  ,
    s  _ M _ _ M _ _ _  ,
    s  _ M M M M _ _ _  ,
    s  _ M _ _ M _ _ _  ,
    s  _ M _ _ M _ _ _  
  },
  { // AB
    s  _ M _ _ M _ _ _  ,
    s  _ _ M _ M _ _ _  ,
    s  _ _ _ M M _ _ _  ,
    s  _ M M M M M _ _  ,
    s  _ _ _ _ M _ _ _  
  },
  { // AC
    s  _ _ _ _ M _ _ _  ,
    s  _ M M M M M _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ M _ M _ _ _  ,
    s  _ _ _ M M _ _ _  
  },
  { // AD
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ M _ _ _  ,
    s  _ M _ _ M _ _ _  ,
    s  _ M M M M _ _ _  ,
    s  _ M _ _ _ _ _ _  
  },
  { // AE
    s  _ M _ M _ M _ _  ,
    s  _ M _ M _ M _ _  ,
    s  _ M _ M _ M _ _  ,
    s  _ M M M M M _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // AF
    s  _ _ _ M M _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ M _ M M _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M M M _ _ _  
  },
  { // B0
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ _ M _ _ _  
  },
  { // B1
    s  _ _ _ _ _ _ _ M  ,
    s  _ M _ _ _ _ _ M  ,
    s  _ _ M M M M _ M  ,
    s  _ _ _ _ M _ _ M  ,
    s  _ _ _ _ _ M M M  
  },
  { // B2
    s  _ _ _ M _ _ _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ M M M M M _ _  ,
    s  _ _ _ _ _ _ M _  ,
    s  _ _ _ _ _ _ _ M  
  },
  { // B3
    s  _ _ _ _ M M M _  ,
    s  _ _ _ _ _ _ M _  ,
    s  _ M _ _ _ _ M M  ,
    s  _ _ M _ _ _ M _  ,
    s  _ _ _ M M M M _  
  },
  { // B4
    s  _ M _ _ _ _ M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M M M M M M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M _ _ _ _ M _  
  },
  { // B5
    s  _ _ M _ _ _ M _  ,
    s  _ _ _ M _ _ M _  ,
    s  _ _ _ _ M _ M _  ,
    s  _ M M M M M M M  ,
    s  _ _ _ _ _ _ M _  
  },
  { // B6
    s  _ M _ _ _ _ M _  ,
    s  _ _ M M M M M M  ,
    s  _ _ _ _ _ _ M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ _ M M M M M _  
  },
  { // B7
    s  _ _ _ _ M _ M _  ,
    s  _ _ _ _ M _ M _  ,
    s  _ M M M M M M M  ,
    s  _ _ _ _ M _ M _  ,
    s  _ _ _ _ M _ M _  
  },
  { // B8
    s  _ _ _ _ M _ _ _  ,
    s  _ M _ _ _ M M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ _ M _ _ _ M _  ,
    s  _ _ _ M M M M _  
  },
  { // B9
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ _ _ M M  ,
    s  _ M _ _ _ _ M _  ,
    s  _ _ M M M M M _  ,
    s  _ _ _ _ _ _ M _  
  },
  { // BA
    s  _ M _ _ _ _ M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M M M M M M _  
  },
  { // BB
    s  _ _ _ _ _ _ M _  ,
    s  _ M _ _ M M M M  ,
    s  _ _ M _ _ _ M _  ,
    s  _ _ _ M M M M M  ,
    s  _ _ _ _ _ _ M _  
  },
  { // BC
    s  _ M _ _ M _ M _  ,
    s  _ M _ _ M _ M _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M _ _ _ _ _  ,
    s  _ _ _ M M M _ _  
  },
  { // BD
    s  _ M _ _ _ _ M _  ,
    s  _ _ M _ _ _ M _  ,
    s  _ _ _ M _ _ M _  ,
    s  _ _ M _ M _ M _  ,
    s  _ M _ _ _ M M _  
  },
  { // BE
    s  _ _ _ _ _ _ M _  ,
    s  _ _ M M M M M M  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M _ _ M _ M _  ,
    s  _ M _ _ _ M M _  
  },
  { // BF
    s  _ _ _ _ _ M M _  ,
    s  _ M _ _ M _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M _ _ _ _ _  ,
    s  _ _ _ M M M M _  
  },
  { // C0
    s  _ _ _ _ M _ _ _  ,
    s  _ M _ _ _ M M _  ,
    s  _ M _ _ M _ M _  ,
    s  _ _ M M _ _ M _  ,
    s  _ _ _ M M M M _  
  },
  { // C1
    s  _ _ _ _ M _ M _  ,
    s  _ M _ _ M _ M _  ,
    s  _ _ M M M M M _  ,
    s  _ _ _ _ M _ _ M  ,
    s  _ _ _ _ M _ _ _  
  },
  { // C2
    s  _ _ _ _ M M M _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ M _ _ M M M _  ,
    s  _ _ M _ _ _ _ _  ,
    s  _ _ _ M M M M _  
  },
  { // C3
    s  _ _ _ _ _ M _ _  ,
    s  _ M _ _ _ M _ M  ,
    s  _ _ M M M M _ M  ,
    s  _ _ _ _ _ M _ M  ,
    s  _ _ _ _ _ M _ _  
  },
  { // C4
    s  _ _ _ _ _ _ _ _  ,
    s  _ M M M M M M M  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ _ M _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // C5
    s  _ M _ _ _ M _ _  ,
    s  _ _ M _ _ M _ _  ,
    s  _ _ _ M M M M M  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ _ M _ _  
  },
  { // C6
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M _ _ _ _ _ _  
  },
  { // C7
    s  _ M _ _ _ _ M _  ,
    s  _ _ M _ M _ M _  ,
    s  _ _ _ M _ _ M _  ,
    s  _ _ M _ M _ M _  ,
    s  _ _ _ _ _ M M _  
  },
  { // C8
    s  _ _ M _ _ _ M _  ,
    s  _ _ _ M _ _ M _  ,
    s  _ M M M M _ M M  ,
    s  _ _ _ M _ M M _  ,
    s  _ _ M _ _ _ M _  
  },
  { // C9
    s  _ _ _ _ _ _ _ _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M _ _ _ _ _  ,
    s  _ _ _ M M M M M  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // CA
    s  _ M M M M _ _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ M _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ M M M M _ _ _  
  },
  { // CB
    s  _ _ M M M M M M  ,
    s  _ M _ _ _ M _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ M _ _ _ M _ _  ,
    s  _ M _ _ _ M _ _  
  },
  { // CC
    s  _ _ _ _ _ _ M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ _ M _ _ _ M _  ,
    s  _ _ _ M M M M _  
  },
  { // CD
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ _ _ M _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ M _ _ _  ,
    s  _ _ M M _ _ _ _  
  },
  { // CE
    s  _ _ M M _ _ M _  ,
    s  _ _ _ _ _ _ M _  ,
    s  _ M M M M M M M  ,
    s  _ _ _ _ _ _ M _  ,
    s  _ _ M M _ _ M _  
  },
  { // CF
    s  _ _ _ _ _ _ M _  ,
    s  _ _ _ M _ _ M _  ,
    s  _ _ M _ _ _ M _  ,
    s  _ M _ M _ _ M _  ,
    s  _ _ _ _ M M M _  
  },
  { // D0
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ M _ M _ M _  ,
    s  _ _ M _ M _ M _  ,
    s  _ _ M _ M _ M _  ,
    s  _ M _ _ _ _ _ _  
  },
  { // D1
    s  _ _ M M M _ _ _  ,
    s  _ _ M _ _ M _ _  ,
    s  _ _ M _ _ _ M _  ,
    s  _ _ M _ _ _ _ _  ,
    s  _ M M M _ _ _ _  
  },
  { // D2
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M _ M _ _ _  ,
    s  _ _ _ M _ _ _ _  ,
    s  _ _ M _ M _ _ _  ,
    s  _ _ _ _ _ M M _  
  },
  { // D3
    s  _ _ _ _ M _ M _  ,
    s  _ _ M M M M M _  ,
    s  _ M _ _ M _ M _  ,
    s  _ M _ _ M _ M _  ,
    s  _ M _ _ M _ M _  
  },
  { // D4
    s  _ _ _ _ _ M _ _  ,
    s  _ M M M M M M M  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ M _ M _ _  ,
    s  _ _ _ _ M M _ _  
  },
  { // D5
    s  _ M _ _ _ _ _ _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M M M M M M _  ,
    s  _ M _ _ _ _ _ _  
  },
  { // D6
    s  _ M _ _ M _ M _  ,
    s  _ M _ _ M _ M _  ,
    s  _ M _ _ M _ M _  ,
    s  _ M _ _ M _ M _  ,
    s  _ M M M M M M _  
  },
  { // D7
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ _ M _ M  ,
    s  _ M _ _ _ M _ M  ,
    s  _ _ M _ _ M _ M  ,
    s  _ _ _ M M M _ _  
  },
  { // D8
    s  _ _ _ _ M M M M  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M _ _ _ _ _  ,
    s  _ _ _ M M M M M  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // D9
    s  _ M M M M M _ _  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ M M M M M M _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M M _ _ _ _  
  },
  { // DA
    s  _ M M M M M M _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M _ _ _ _ _  ,
    s  _ _ _ M _ _ _ _  ,
    s  _ _ _ _ M _ _ _  
  },
  { // DB
    s  _ M M M M M M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M M M M M M _  
  },
  { // DC
    s  _ _ _ _ M M M _  ,
    s  _ _ _ _ _ _ M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ _ M _ _ _ M _  ,
    s  _ _ _ M M M M _  
  },
  { // DD
    s  _ M _ _ _ _ M _  ,
    s  _ M _ _ _ _ M _  ,
    s  _ M _ _ _ _ _ _  ,
    s  _ _ M _ _ _ _ _  ,
    s  _ _ _ M M _ _ _  
  },
  { // DE
    s  _ _ _ _ _ _ M _  ,
    s  _ _ _ _ _ M _ _  ,
    s  _ _ _ _ _ _ _ M  ,
    s  _ _ _ _ _ _ M _  ,
    s  _ _ _ _ _ _ _ _  
  },
  { // DF
    s  _ _ _ _ _ M M M  ,
    s  _ _ _ _ _ M _ M  ,
    s  _ _ _ _ _ M M M  ,
    s  _ _ _ _ _ _ _ _  ,
    s  _ _ _ _ _ _ _ _  
  }
};

#undef s
#undef M
#undef _
