#include <M5Unified.h>
#include "protocol.h"
#include "i2c_transport.h"

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
const char* probe="NOT_RUN";
ExternalI2C externalBus;
esp_err_t lastReadError=ESP_OK;
const char* lastReadOutcome="WAIT";
int beforeSda=1,beforeScl=1,afterSda=1,afterScl=1;
uint32_t noAck[2]={},timeoutCount[2]={},otherErrors[2]={};
bool startupProbeDone=false;

void runProbe() {
  const int sda=ExternalI2C::sda(),scl=ExternalI2C::scl();
  const esp_err_t error=externalBus.probe(uwbtest::address,clocks[selected]);
  probe=ExternalI2C::outcome(error);
  if(Serial) Serial.printf("PROBE,%lu,%s,error=%s,before=%d/%d,after=%d/%d\n",
    (unsigned long)clocks[selected],probe,esp_err_to_name(error),sda,scl,
    ExternalI2C::sda(),ExternalI2C::scl());
}

void resetStats() {
  stats[0]={}; stats[1]={}; autoDone=false; frameSeen=false; lastValid=false;
  for(unsigned i=0;i<2;i++) noAck[i]=timeoutCount[i]=otherErrors[i]=0;
}
void logStats(unsigned i) {
  auto& s=stats[i];
  if (!Serial) return;
  Serial.printf("STAT,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
    (unsigned long)clocks[i],(unsigned long)s.attempts,(unsigned long)s.ok,
    (unsigned long)s.io,(unsigned long)s.bad,(unsigned long)s.gaps,
    (unsigned long)s.sequence,(unsigned long)s.maxUs);
  Serial.printf("DIAG,fw=%s,bus=%s,init=%s,port=%d,internal_port=%d,read=%s,error=%s,before=%d/%d,after=%d/%d,idle=%d/%d,nack=%lu,timeout=%lu,other=%lu,probe=%s\n",
    FW_VERSION,busReady?"READY":"INIT_FAIL",esp_err_to_name(externalBus.initError),
    (int)externalBus.port,(int)M5.In_I2C.getPort(),lastReadOutcome,
    esp_err_to_name(lastReadError),beforeSda,beforeScl,afterSda,afterScl,
    ExternalI2C::sda(),ExternalI2C::scl(),(unsigned long)noAck[i],
    (unsigned long)timeoutCount[i],(unsigned long)otherErrors[i],probe);
}
void readFrame() {
  uint8_t b[16]={};
  const uint32_t start=micros();
  beforeSda=ExternalI2C::sda(); beforeScl=ExternalI2C::scl();
  lastReadError=externalBus.read(uwbtest::address,b,sizeof b,clocks[selected]);
  afterSda=ExternalI2C::sda(); afterScl=ExternalI2C::scl();
  const bool good=lastReadError==ESP_OK;
  lastReadOutcome=good?"RECEIVED":ExternalI2C::outcome(lastReadError);
  if(lastReadError==ESP_FAIL) ++noAck[selected];
  else if(lastReadError==ESP_ERR_TIMEOUT) ++timeoutCount[selected];
  else if(!good) ++otherErrors[selected];
  const uint32_t duration=micros()-start;
  auto& s=stats[selected];
  if (!good) { s.transportError(duration); lastValid=false; }
  else {
    memcpy(lastFrame,b,sizeof b); frameSeen=true;
    uint32_t seq=0;
    lastValid=uwbtest::decode(b,sizeof b,seq)==uwbtest::Result::valid;
    s.received(b,sizeof b,duration);
    lastReadOutcome=lastValid?"VALID":"BAD_FRAME";
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
  canvas.drawString("2DK / I2C DIAG " FW_VERSION,12,8);
  canvas.setTextColor(white); canvas.setTextSize(2);
  canvas.drawString(selected?"400 kHz":"100 kHz",12,27);
  canvas.setTextSize(1);
  const char* status=!busReady?"BUS INIT ERROR":running?(lastValid?"RECEIVING":lastReadOutcome):"PAUSED";
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
  else canvas.printf("SDA %d SCL %d / PROBE %s",ExternalI2C::sda(),ExternalI2C::scl(),probe);
  canvas.setTextColor(0x8faac6);
  canvas.setCursor(12,137);
  if(frameSeen) for(unsigned i=0;i<8;i++) canvas.printf("%02X ",lastFrame[i]);
  else canvas.printf("NACK %lu / TIMEOUT %lu",(unsigned long)noAck[selected],(unsigned long)timeoutCount[selected]);
  canvas.setCursor(12,151);
  if(frameSeen) for(unsigned i=8;i<16;i++) canvas.printf("%02X ",lastFrame[i]);
  else canvas.printf("Read: %s",lastReadOutcome);
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
    runProbe();
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
  // Verify bus separation before releasing ONLY the external controller.
  const auto port=M5.Ex_I2C.getPort();
  if(M5.Ex_I2C.getSDA()==2 && M5.Ex_I2C.getSCL()==1
      && port!=M5.In_I2C.getPort()) {
    M5.Ex_I2C.release();
    busReady=externalBus.begin(port,clocks[0])==ESP_OK;
  }
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
  // USB command p repeats the probe without needing the touch screen.
  if(Serial && Serial.available()) { const int c=Serial.read(); if(c=='p'||c=='P') runProbe(); }
  uint32_t now=millis();
  if(!startupProbeDone && now>=5000) { startupProbeDone=true;runProbe(); }
  if(running && int32_t(now-nextRead)>=0) { readFrame(); nextRead=millis()+intervalMs; }
  if(now-lastDraw>=200) {lastDraw=now;draw();}
  if(now-lastLog>=1000) {lastLog=now;logStats(selected);}
  delay(1);
}
