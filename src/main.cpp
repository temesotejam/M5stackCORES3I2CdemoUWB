#include <M5Unified.h>
#include "protocol.h"

namespace {
constexpr uint32_t clocks[2]={100000,400000};
constexpr uint32_t intervalMs=20; // Read rate 50 Hz; unrelated to the SCL clock.
constexpr uint32_t autoCount=500;
constexpr uint32_t bg=0x08121f, panel=0x13243a, white=0xe7f1ff;
constexpr uint32_t cyan=0x45d6d0, amber=0xffc66d, red=0xff778a;
M5Canvas canvas(&M5.Display);
uwbtest::Stats stats[2];
unsigned selected=0;
bool running=true, autoTest=false, autoDone=false, busReady=false;
bool frameSeen=false, lastValid=false;
uint32_t nextRead=0,lastDraw=0,lastLog=0,lastSuccessMs=0;
uint8_t lastFrame[16]={};
const char* probe="not probed";

void resetStats() {
  stats[0]={}; stats[1]={}; autoDone=false; frameSeen=false; lastValid=false;
}
void logStats(unsigned i) {
  auto& s=stats[i];
  if (!Serial) return;
  Serial.printf("STAT,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
    (unsigned long)clocks[i],(unsigned long)s.attempts,(unsigned long)s.ok,
    (unsigned long)s.io,(unsigned long)s.bad,(unsigned long)s.gaps,
    (unsigned long)s.sequence,(unsigned long)s.maxUs);
}
void readFrame() {
  uint8_t b[16]={};
  const uint32_t start=micros();
  bool good=false;
  if (busReady) {
    auto& bus=M5.Ex_I2C;
    if (bus.start(uwbtest::address,true,clocks[selected])) {
      const bool readOK=bus.read(b,sizeof b,true); // Last byte NACK, then STOP.
      const bool stopOK=bus.stop();
      good=readOK && stopOK;
    }
  }
  const uint32_t duration=micros()-start;
  auto& s=stats[selected];
  if (!good) { s.transportError(duration); lastValid=false; }
  else {
    memcpy(lastFrame,b,sizeof b); frameSeen=true;
    uint32_t seq=0;
    lastValid=uwbtest::decode(b,sizeof b,seq)==uwbtest::Result::valid;
    s.received(b,sizeof b,duration);
    if(lastValid) lastSuccessMs=millis();
  }
  if (autoTest && s.attempts>=autoCount) {
    logStats(selected);
    if (selected==0) { selected=1; stats[1].haveSequence=false; }
    else { running=false; autoTest=false; autoDone=true; }
  }
}
void button(int x,int y,int w,const char* label,bool active=false) {
  canvas.fillRoundRect(x,y,w,28,6,active?cyan:panel);
  canvas.setTextColor(active?bg:white);
  canvas.setTextDatum(middle_center);
  canvas.drawString(label,x+w/2,y+14);
  canvas.setTextDatum(top_left);
}
void draw() {
  auto& s=stats[selected];
  canvas.fillScreen(bg);
  canvas.setTextColor(cyan); canvas.setTextSize(1);
  canvas.drawString("TYPE 2DK / I2C CHECK",12,8);
  canvas.setTextColor(white); canvas.setTextSize(2);
  canvas.drawString(selected?"400 kHz":"100 kHz",12,27);
  canvas.setTextSize(1);
  const char* status=!busReady?"BUS INIT ERROR":running?(lastValid?"RECEIVING":"WAIT / ERROR"):"PAUSED";
  if(autoTest) status="AUTO TEST";
  if(autoDone) status=stats[0].passes(autoCount)&&stats[1].passes(autoCount)?"AUTO PASS":"AUTO FAIL";
  canvas.setTextColor(!busReady?red:(lastValid?cyan:amber));
  canvas.drawString(status,163,32);
  canvas.setTextColor(white);
  canvas.setCursor(12,54); canvas.printf("PORT A  SDA 2 / SCL 1   ADDR 0x42");
  canvas.setCursor(12,70); canvas.printf("OK %lu    IO %lu    BAD %lu",(unsigned long)s.ok,(unsigned long)s.io,(unsigned long)s.bad);
  canvas.setCursor(12,86); canvas.printf("SEQ %lu    GAP %lu",(unsigned long)s.sequence,(unsigned long)s.gaps);
  canvas.setCursor(12,102); canvas.printf("READ %lu us / MAX %lu us",(unsigned long)s.lastUs,(unsigned long)s.maxUs);
  canvas.setCursor(12,118);
  if(autoDone) canvas.printf("100k: %s   400k: %s",stats[0].passes(autoCount)?"PASS":"FAIL",stats[1].passes(autoCount)?"PASS":"FAIL");
  else if(autoTest) canvas.printf("AUTO %lu / %lu reads",(unsigned long)s.attempts,(unsigned long)autoCount);
  else if(s.ok) canvas.printf("50 Hz reads / age %lu ms",(unsigned long)(millis()-lastSuccessMs));
  else canvas.printf("0x42 probe: %s",probe);
  canvas.setTextColor(0x8faac6);
  canvas.setCursor(12,137);
  if(frameSeen) for(unsigned i=0;i<8;i++) canvas.printf("%02X ",lastFrame[i]);
  else canvas.print("Waiting for 2DKI test frame...");
  canvas.setCursor(12,151);
  if(frameSeen) for(unsigned i=8;i<16;i++) canvas.printf("%02X ",lastFrame[i]);
  else canvas.print("Test data only / no distance");
  button(8,174,96,"100 kHz",selected==0);
  button(112,174,96,"400 kHz",selected==1);
  button(216,174,96,"PROBE");
  button(8,207,96,running?"PAUSE":"RUN");
  button(112,207,96,"AUTO",autoTest);
  button(216,207,96,"CLEAR");
  canvas.pushSprite(0,0);
}
void action(unsigned b) {
  if(b<2) { selected=b; autoTest=false; autoDone=false; stats[b].haveSequence=false; running=true; lastValid=false; }
  else if(b==2) {
    autoTest=false; autoDone=false;
    probe=busReady&&M5.Ex_I2C.scanID(uwbtest::address,clocks[selected])?"ACK":"NO ACK";
    if(Serial) Serial.printf("PROBE,%lu,%s\n",(unsigned long)clocks[selected],probe);
  } else if(b==3) {
    running=!running; autoTest=false; autoDone=false; stats[selected].haveSequence=false;
  } else if(b==4) {
    resetStats(); selected=0; autoTest=true; running=true;
  } else if(b==5) {
    resetStats(); autoTest=false;
  }
  nextRead=millis()+intervalMs;
  draw();
}
}
void setup() {
  auto cfg=M5.config();
  cfg.serial_baudrate=115200;
  cfg.internal_imu=false; cfg.internal_rtc=false;
  cfg.internal_mic=false; cfg.internal_spk=false;
  cfg.external_imu=false; cfg.external_rtc=false;
  cfg.external_display_value=0;
  cfg.output_power=false; // Both devices powered through their own USB.
  cfg.fallback_board=m5::board_t::board_M5StackCoreS3;
  M5.begin(cfg);
  M5.Display.setRotation(1); M5.Display.setBrightness(160);
  canvas.setColorDepth(16);
  if(!canvas.createSprite(320,240)) {
    M5.Display.fillScreen(TFT_BLACK); M5.Display.println("Display buffer failed");
    while(true) delay(1000);
  }
  // M5Unified manages separate internal and external buses. Do not re-init Wire.
  busReady=M5.Ex_I2C.getSDA()==2 && M5.Ex_I2C.getSCL()==1 && M5.Ex_I2C.begin();
  nextRead=millis()+5000; // Allow the slave's startup delay.
  draw();
}
void loop() {
  M5.update();
  if(M5.Touch.getCount()) {
    auto t=M5.Touch.getDetail();
    if(t.wasPressed() && t.y>=174 && t.y<235 && t.x>=8 && t.x<312) {
      unsigned column=(t.x-8)/104;
      if(column<3 && (t.x-8)%104<96 && !(t.y>=202 && t.y<207))
        action(column+(t.y>=207?3:0));
    }
  }
  uint32_t now=millis();
  if(running && int32_t(now-nextRead)>=0) { readFrame(); nextRead=millis()+intervalMs; }
  if(now-lastDraw>=200) {lastDraw=now;draw();}
  if(now-lastLog>=1000) {lastLog=now;logStats(selected);}
  delay(1);
}
