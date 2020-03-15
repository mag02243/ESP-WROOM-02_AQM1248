//
// ST7565R を使用したグラフィック液晶表示クラス
//
// SPI 対応で入力ポートを使用しない前提で動作する
//  cs  :out チップセレクト
//  rs  :out レジスタセレクト
//  sck :out SPIクロック
//  mosi:out SPIデータ出力
//

#ifndef __GLCD_H
#define __GLCD_H

#include <Arduino.h>

#define LCD_WIDTH  128
#define LCD_HEIGHT  64

#define GLCD_BLACK 1
#define GLCD_WHITE 0
#define GLCD_CLEAR 255


class GLCD
{
  public:
    GLCD();
    void begin(uint8_t cs = 5, uint8_t rs = 12, uint8_t sck = 14, uint8_t mosi = 13);
    void sendCmd(uint8_t cmd);
    void sendData(uint8_t data);
    void FontPos(uint8_t x, uint8_t y);
    void put(char c, bool reverse = false);
    void print(char *str, bool reverse = false);
    void clear();
    bool SetPixel(uint8_t x, uint8_t y, uint8_t color = 1);
    int8_t Line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color = 1);
    bool setFont(uint8_t no, uint8_t *font);      // no: 0-15
    uint8_t kprint(uint8_t x, uint8_t y, char *str, uint8_t one = GLCD_BLACK, uint8_t zero = GLCD_WHITE);
    uint8_t kprint32(uint8_t x, uint8_t y, char *str, uint8_t one = GLCD_BLACK, uint8_t zero = GLCD_WHITE);
    void Rectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2,
      uint8_t border = GLCD_BLACK, int8_t fill = GLCD_CLEAR);
    void beginBackGroundDraw();
    void endBackGroundDraw();
  private:
    void write(uint8_t direction, uint8_t data);
    uint8_t mosi;     // データ送信
    uint8_t sck;      // クロック
    uint8_t cs;       // チップセレクトピン
    uint8_t rs;       // レジスタセレクトピン
    uint8_t x0, y0;
    uint8_t fx0, fy0;
    uint8_t background;
};

#endif
