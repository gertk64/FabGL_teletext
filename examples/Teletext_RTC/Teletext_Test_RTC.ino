
/*
  Teletekst generator gebaseerd op de CVBSGenerator
  uit een (gemodificeerde) FabGL library

   ZS-042 RTC module toegevoegd met DS3231  
   LET OP! verwijder de diode en/of weerstand naast de RTC chip
           om opladen/opblazen van de CR2032 te voorkomen!

  Gert van der Knokke 2024
 */


#include <Preferences.h>
#include <uRTCLib.h>
#include "fabgl.h"
#include "devdrivers/cvbsgenerator.h"
#include "teletext.h"
#include "pages.h"

#define VIDEOOUT_GPIO GPIO_NUM_25


Preferences preferences;

uRTCLib rtc;

byte rtcModel = URTCLIB_MODEL_DS3231;


fabgl::CVBS16Controller DisplayController;

#define MYLED 2


// defaults
#define MODES_DEFAULT 4  // P-PAL-B
#define MONO_DEFAULT 0   // color
#define HRATE_DEFAULT 1  // horizontal rate (1 = x1, 2 = x2, 3 = x3)






#define BACKGROUND_COLOR RGB888(0, 0, 0)



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

  URTCLIB_WIRE.begin();
  rtc.set_rtc_address(0x68);


  // set RTC Model
  rtc.set_model(rtcModel);

  // refresh data from RTC HW in RTC class object so flags like rtc.lostPower(), rtc.getEOSCFlag(), etc, can get populated
  rtc.refresh();

  // gebruik dit eenmalig om de kloktijd in te stellen op de RTC module
  // rtc.set(0, 42, 16, 6, 2, 5, 15);

  preferences.begin("CVBS", false);


  DisplayController.begin(VIDEOOUT_GPIO);

  DisplayController.setHorizontalRate(HRATE_DEFAULT);
  DisplayController.setMonochrome(MONO_DEFAULT);

  DisplayController.setResolution(MODES_STD[MODES_DEFAULT]);
}




static int currentpage = 0;
static int currentline = 0;

void send_header(int page, unsigned char* pagedata) {
  int t, s;
  int row;
  char tmTekst[40];

  // lees RTC uit
  rtc.refresh();

  // header aanmaken, paginanummer, etc.
  DisplayController.writeTXTbuf(3, hamming[page / 100]);         // bladnummer 1 (paginanummer hondertallen, 0-8) + bit 0 regelnummer
  DisplayController.writeTXTbuf(4, hamming[0]);                  // + regelnummer bits 1-4
  DisplayController.writeTXTbuf(5, hamming[page % 10]);          // paginanummer eenheden
  DisplayController.writeTXTbuf(6, hamming[(page % 100) / 10]);  // paginanummer tientallen

  // vul de 'pagina op bepaalde tijd info in
  // met alles op nul verschijnt de pagina direct
  DisplayController.writeTXTbuf(7, hamming[0]);   // minuten eenheden
  DisplayController.writeTXTbuf(8, hamming[0]);   // minuten tientallen + C4 bit
  DisplayController.writeTXTbuf(9, hamming[0]);   // uren eenheden
  DisplayController.writeTXTbuf(10, hamming[0]);  // uren tientallen

  // vul de extra bits in
  DisplayController.writeTXTbuf(11, hamming[2]);  // C7 C8 C9 C10
  DisplayController.writeTXTbuf(12, hamming[0]);  // C11 C12 C13 C14

  // header tekst invullen uit bladzijde (tijd en datum worden later overschreven)
  for (t = 0; t < 24; t++) DisplayController.writeTXTbuf(t + 13, parity[pagedata[t + 8]]);

  // monteer kloktijd uit RTC in header
  sprintf(tmTekst, "%02d-%02d-%02d\003%02d:%02d.%02d", rtc.day(), rtc.month(), rtc.year(), rtc.hour(), rtc.minute(), rtc.second());
  for (t = 0; t < 18; t++) DisplayController.writeTXTbuf(t + 28, parity[tmTekst[t]]);
}

// verstuur een pagina regel
void send_line(int page, int row, unsigned char* pagedata) {

  DisplayController.writeTXTbuf(3, hamming[(page / 100) + ((row & 1) << 3)]);  // bladnummer 1 (paginanummer hondertallen, 0-8)
  DisplayController.writeTXTbuf(4, hamming[row >> 1]);                         // + regelnummer t
  for (int s = 0; s < 40; s++) {
    DisplayController.writeTXTbuf(5 + s, parity[pagedata[row * 40 + s]]);
  }
}

// hoofdlus
void loop() {
  app.runAsync(&DisplayController, 2000);
  // vul clock run-in en framecode alvast in
  DisplayController.writeTXTbuf(0, 0b01010101);
  DisplayController.writeTXTbuf(1, 0b01010101);
  DisplayController.writeTXTbuf(2, 0b00100111);

  DisplayController.clearTxtState();
  // digitalWrite(MYLED, LOW);

  // eindeloze lus, RTOS handelt dit als het goed is af..
  while (true) {
    // trigger de teletext generator
    DisplayController.clearTxtState();

    // wacht op vrijgave buffer
    while (DisplayController.txtState() == true)
      ;

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
