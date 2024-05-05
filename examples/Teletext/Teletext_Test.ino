/*
  Teletekst generator gebaseerd op de CVBSGenerator
  uit een (gemodificeerde) FabGL library

  Gert van der Knokke 2024
 */


#include <Preferences.h>


#include "fabgl.h"
#include "devdrivers/cvbsgenerator.h"
#include "pages.h"

#define VIDEOOUT_GPIO GPIO_NUM_25


Preferences preferences;


fabgl::CVBS16Controller DisplayController;

#define MYLED 2


// defaults
#define MODES_DEFAULT 4  // P-PAL-B
#define MONO_DEFAULT 0   // color
#define HRATE_DEFAULT 1  // horizontal rate (1 = x1, 2 = x2, 3 = x3)



// modes

static const char* MODES_DESC[] = {
  "Interlaced PAL-B",
  "Progressive PAL-B",
  "Interlaced NTSC-M",
  "Progressive NTSC-M",
  "Interlaced PAL-B Wide",
  "Progressive PAL-B Wide",
  "Interlaced NTSC-M Wide",
  "Progressive NTSC-M Wide",
  "Progressive NTSC-M Ext",
};

static const char* MODES_STD[] = {
  "I-PAL-B",
  "P-PAL-B",
  "I-NTSC-M",
  "P-NTSC-M",
  "I-PAL-B-WIDE",
  "P-PAL-B-WIDE",
  "I-NTSC-M-WIDE",
  "P-NTSC-M-WIDE",
  "P-NTSC-M-EXT",
};

static const uint8_t hamming[16] = {
  0b00010101,  // 0
  0b00000010,  // 1
  0b01001001,  // 2
  0b01011110,  // 3
  0b01100100,  // 4
  0b01110011,  // 5
  0b00111000,  // 6
  0b00101111,  // 7
  0b11010000,  // 8
  0b11000111,  // 9
  0b10001100,  // A
  0b10011011,  // B
  0b10100001,  // C
  0b10110110,  // D
  0b11111101,  // E
  0b11101010   // F
};


#define BACKGROUND_COLOR RGB888(0, 0, 0)


static RGB888 palet[8] = {
  Color::BrightWhite,
  Color::BrightYellow,
  Color::BrightCyan,
  Color::BrightGreen,
  Color::BrightMagenta,
  Color::BrightRed,
  Color::BrightBlue,
  Color::Black,
};

// main app
struct MyApp : public uiApp {


  void init() {

    // setStyle(&dialogStyle);

    // set root window background color to dark green
    rootWindow()->frameStyle().backgroundColor = BACKGROUND_COLOR;
    //rootWindow()->windowStyle().borderColor = RGB888(255, 255, 0);
    //rootWindow()->windowStyle().borderSize = 1;


    // some static text
    rootWindow()->onPaint = [&]() {
      auto cv = canvas();


      // color bars, all 8 full colors
      auto w = cv->getWidth();
      auto h = cv->getHeight();

      for (int t = 0; t < 8; t++) {
        cv->setBrushColor(palet[t]);
        cv->fillRectangle(t * (w / 8), 0, (t + 1) * (w / 8) - 1, h - 1);
      }

      // black bar in center
      cv->setBrushColor(Color::Black);
      cv->fillRectangle(0, (h / 2) - 20, w - 1, (h / 2) + 20);

      // announce ourselves
      cv->selectFont(&fabgl::FONT_std_24);
      cv->setPenColor(RGB888(255, 255, 255));
      cv->drawText(150, h / 2 - 12, "PAL-I testbeeld & Teletekst generator");

      // by...
      cv->selectFont(&fabgl::FONT_std_16);
      cv->setPenColor(RGB888(255, 255, 255));
      cv->drawText(7 * (w / 8) + 5, h - 30, "(C)2024");
      cv->drawText(7 * (w / 8) + 5, h - 15, "  KGE");
    };
  }

} app;


void setup() {
  pinMode(MYLED, OUTPUT);
  Serial.begin(115200);
  delay(500);
  Serial.write("\n\n\n");  // DEBUG ONLY

  preferences.begin("CVBS", false);


  DisplayController.begin(VIDEOOUT_GPIO);

  DisplayController.setHorizontalRate(HRATE_DEFAULT);
  DisplayController.setMonochrome(MONO_DEFAULT);

  DisplayController.setResolution(MODES_STD[MODES_DEFAULT]);
}

static const uint8_t parity[128]={
  0x80,0x01,0x02,0x83,0x04,0x85,0x86,0x07,
  0x08,0x89,0x8a,0x0b,0x8c,0x0d,0x0e,0x8f,
  0x10,0x91,0x92,0x13,0x94,0x15,0x16,0x97,
  0x98,0x19,0x1a,0x9b,0x1c,0x9d,0x9e,0x1f,
  0x20,0xa1,0xa2,0x23,0xa4,0x25,0x26,0xa7,
  0xa8,0x29,0x2a,0xab,0x2c,0xad,0xae,0x2f,
  0xb0,0x31,0x32,0xb3,0x34,0xb5,0xb6,0x37,
  0x38,0xb9,0xba,0x3b,0xbc,0x3d,0x3e,0xbf,
  0x40,0xc1,0xc2,0x43,0xc4,0x45,0x46,0xc7,
  0xc8,0x49,0x4a,0xcb,0x4c,0xcd,0xce,0x4f,
  0xd0,0x51,0x52,0xd3,0x54,0xd5,0xd6,0x57,
  0x58,0xd9,0xda,0x5b,0xdc,0x5d,0x5e,0xdf,
  0xe0,0x61,0x62,0xe3,0x64,0xe5,0xe6,0x67,
  0x68,0xe9,0xea,0x6b,0xec,0x6d,0x6e,0xef,
  0x70,0xf1,0xf2,0x73,0xf4,0x75,0x76,0xf7,
  0xf8,0x79,0x7a,0xfb,0x7c,0xfd,0xfe,0x7f
};


#if FABGLIB_CVBSCONTROLLER_PERFORMANCE_CHECK
namespace fabgl {
extern volatile uint64_t s_cvbsctrlcycles;
}
using fabgl::s_cvbsctrlcycles;
#endif


struct tm timeinfo;
static int currentpage = 0;
static int currentline = 0;


void send_header(int page, unsigned char* pagedata) {
  int t, s;
  int row;
  char tmTekst[40];

  //getLocalTime(&timeinfo);

  // header aanmaken, paginanummer, etc.
  DisplayController.writeTXTbuf(3, hamming[page / 100]);         // bladnummer 1 (paginanummer hondertallen, 0-8) + bit 0 regelnummer
  DisplayController.writeTXTbuf(4, hamming[0]);                  // + regelnummer bits 1-4
  DisplayController.writeTXTbuf(5, hamming[page % 10]);          // paginanummer eenheden
  DisplayController.writeTXTbuf(6, hamming[(page % 100) / 10]);  // paginanummer tientallen
  //DisplayController.writeTXTbuf(7, hamming[timeinfo.tm_min % 10]);      // minuten eenheden
  //DisplayController.writeTXTbuf(8, hamming[8 + timeinfo.tm_min / 10]);  // minuten tientallen + C4 bit
  //DisplayController.writeTXTbuf(9, hamming[timeinfo.tm_hour % 10]);     // uren eenheden
  //DisplayController.writeTXTbuf(10, hamming[timeinfo.tm_hour / 10]);    // uren tientallen

  DisplayController.writeTXTbuf(7, hamming[0]);   // minuten eenheden
  DisplayController.writeTXTbuf(8, hamming[0]);   // minuten tientallen + C4 bit
  DisplayController.writeTXTbuf(9, hamming[0]);   // uren eenheden
  DisplayController.writeTXTbuf(10, hamming[0]);  // uren tientallen

  DisplayController.writeTXTbuf(11, hamming[2]);  // C7 C8 C9 C10  was 2
  DisplayController.writeTXTbuf(12, hamming[0]);  // C11 C12 C13 C14 was 9

  // header tekst invullen uit bladzijde (behalve tijd en datum)
  for (t = 0; t < 24; t++) DisplayController.writeTXTbuf(t + 13, parity[pagedata[t + 8]]);

  // monteer kloktijd in header, deze moet nog vervangen worden door een RTC
 // sprintf(tmTekst, "%02d-%02d-%02d\003%02d:%02d.%02d", timeinfo.tm_mday, timeinfo.tm_mon, timeinfo.tm_year, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
 // for (t = 0; t < 18; t++) DisplayController.writeTXTbuf(t + 28, parity(tmTekst[t]));
  DisplayController.writeTXTbuf(37,parity[(timeinfo.tm_hour/10)+48]);
  DisplayController.writeTXTbuf(38,parity[(timeinfo.tm_hour%10)+48]);
  DisplayController.writeTXTbuf(39,parity[':']);
  DisplayController.writeTXTbuf(40,parity[(timeinfo.tm_min/10)+48]);
  DisplayController.writeTXTbuf(41,parity[(timeinfo.tm_min%10)+48]);
  DisplayController.writeTXTbuf(42,parity['.']); 
  DisplayController.writeTXTbuf(43,parity[(timeinfo.tm_sec/10)+48]);
  DisplayController.writeTXTbuf(44,parity[(timeinfo.tm_sec%10)+48]);
  
  timeinfo.tm_sec++;  // pseudo secondes...

  if (timeinfo.tm_sec > 59) {
    timeinfo.tm_sec = 0;
    timeinfo.tm_min++;

    if (timeinfo.tm_min > 59) {
      timeinfo.tm_min = 0;
      timeinfo.tm_hour++;
    }

    if (timeinfo.tm_hour > 23) {
      timeinfo.tm_hour = 0;
    }
  }
}

void send_line(int page, int row, unsigned char* pagedata) {


  // stuur regel van bladzijde
  DisplayController.writeTXTbuf(3, hamming[(page / 100) + ((row & 1) << 3)]);  // bladnummer 1 (paginanummer hondertallen, 0-8)
  DisplayController.writeTXTbuf(4, hamming[row >> 1]);                         // + regelnummer t
  for (int s = 0; s < 40; s++) {
    DisplayController.writeTXTbuf(5 + s, parity[pagedata[row * 40 + s]]);
  }
}



typedef struct pageInfo {
  int pagenumber;
  unsigned char* pagedata;
};

pageInfo pages[] = {
  100, kabel100_ttx,
  109, kabel109_ttx,
  453, kabel453_ttx,
  0, NULL
};

void loop() {
  app.runAsync(&DisplayController, 2000);
  // vul clock run-in en framecode alvast in
  DisplayController.writeTXTbuf(0, 0b01010101);
  DisplayController.writeTXTbuf(1, 0b01010101);
  DisplayController.writeTXTbuf(2, 0b00100111);

  DisplayController.clearTxtState();
  digitalWrite(MYLED, LOW);

  // eindeloze lus, RTOS handelt dit als het goed is af..
  while (true) {
    // trigger de teletext generator
    DisplayController.clearTxtState();

    // wacht op vrijgave buffer 
    while (DisplayController.txtState() == true);

    //digitalWrite(MYLED, HIGH);

    if (currentline == 0) {
      send_header(pages[currentpage].pagenumber, pages[currentpage].pagedata);
      currentline++;
    } else {
      send_line(pages[currentpage].pagenumber, currentline, pages[currentpage].pagedata);
      currentline++;
      //digitalWrite(MYLED, LOW);

      // einde bladzijde?
      if (currentline == 25) {
        currentline = 0;
        currentpage++;
        // niet meer pagina's?
        if (pages[currentpage].pagenumber == 0) currentpage = 0;
      }
    }
 
  }
  // hier komt hij nooit als het goed is...
  vTaskDelete(NULL);
}
