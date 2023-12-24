#include <avr/sleep.h>

#define INTERVAL_MS (86400000) // 24h
#define ENDPOINT "ds1.myu.ac.jp"


/* for LTE-M Shield for Arduino */
#define RX 10
#define TX 11
#define BAUDRATE 9600
#define BG96_RESET 15

#define TINY_GSM_MODEM_BG96
#include <TinyGsmClient.h>

#include <SoftwareSerial.h>
SoftwareSerial LTE_M_shieldUART(RX, TX);
TinyGsm modem(LTE_M_shieldUART);
TinyGsmClient ctx(modem);

/* for debug */
#define RX1 6
#define TX1 7

SoftwareSerial DEBUG(RX1, TX1);

/* for TWELITE */
// pin 0, 1
#define TWELITE_UART Serial

// #define RESET_DURATION 86400000UL // 1 day
void software_reset() {
  asm volatile ("  jmp 0");
}

void setup() {
  for( int i = 0; i < 20; i++ )
  {
    pinMode(i, INPUT_PULLUP);
  }
  pinMode(RX, INPUT);
  pinMode(TX, OUTPUT);
  pinMode(RX1, INPUT);
  pinMode(TX1, OUTPUT);
  
  DEBUG.begin(9600);
  
  DEBUG.print(F("module setup start"));
  pinMode(BG96_RESET,OUTPUT);
  digitalWrite(BG96_RESET,LOW);
  delay(300);
  digitalWrite(BG96_RESET,HIGH);
  delay(300);
  digitalWrite(BG96_RESET,LOW);

  DEBUG.print(F("1"));
  LTE_M_shieldUART.begin(BAUDRATE);

  DEBUG.print(F("2"));
  modem.restart();

  DEBUG.print(F("3"));
  String modemInfo = modem.getModemInfo();
  // DEBUG.print(F(modemInfo));
  // DEBUG.print(modemInfo.length());

  DEBUG.print(F("4"));
  for(int i = 0; i < 3; i ++){
    if(modem.waitForNetwork()){
      break;
    }
    if(i==2){
      DEBUG.println(F(" send AT command "));
      LTE_M_shieldUART.write("ATE1");
      LTE_M_shieldUART.write(0x0d);
      delay(100);
      LTE_M_shieldUART.write("AT+CGDCONT=1,\"IP\",\"soracom.io\",\"0.0.0.0\",0,0,0,0");
      delay(100);
      LTE_M_shieldUART.write("AT+QCFG=\"nwscanmode\",0,0");
      delay(100);
      LTE_M_shieldUART.write("AT+QCFG=\"iotopmode\",0,0");
      delay(100);
      LTE_M_shieldUART.write("AT+QCFG=\"nwscanseq\",00,1");
      delay(100);
      software_reset();
    }
    DEBUG.println(F(" again "));
  }

  DEBUG.print(F("5"));
  modem.gprsConnect("soracom.io", "sora", "sora");
  while (!modem.isNetworkConnected()) DEBUG.print(".");
  DEBUG.print(F("6"));
  IPAddress ipaddr = modem.localIP();

  DEBUG.print(F("7"));
  TWELITE_UART.begin(115200);
  DEBUG.listen();  
  DEBUG.println(F("loop start"));
}

void loop() {
    // DEBUG.println(F("waiting...."));
    // delay(1000);

    /* sleep */
    // スリープモード移行
    Sleep();

    // シリアル待つ
    delay(2000);
    
    while(TWELITE_UART.available()){ //TWELITEからの受信待ち
        // データ受け取り
        String str_data = TWELITE_UART.readString();
        str_data.trim();
        
        DEBUG.println(str_data);

        int start = -1;
        int goal = -1;
        // msgs 送られてきたmsg（重複なし）
        String msgs[5];
        // 重複数
        int msgs_n[5];
        // msgsの要素数
        int m_i = 0;
        String msg;
        int max_n = 0;
        String majority_data;
        int i;
        int n;
        int j;
        
        for(i = 0;i < str_data.length();i++){
          // 文字の種類によって場合分け
          switch(str_data.charAt(i)){
            
            // msg一回分、;が来た時
            case ';':
              // ;の前の文字列を切り出す
              msg = str_data.substring(start+1, i);
              DEBUG.print("msg is ");
              DEBUG.println(msg);
              
              // msgsに既に無いか調べる
              for(n = 0; n < 5; n++){
                
                // もしあったら同じインデックスのmsgs_nの要素を+1する
                if(msgs[n] == msg){
                  msgs_n[n]++;
                  break;
                }
                
                // もし5回回してなければmsgsのm_iに入れ、m_iを＋１する
                if(n==4){
                  msgs[m_i] = msg;
                  msgs_n[m_i] = 1;
                  m_i++;
                }
                
              }
              // 次のstart
              start = i;
              DEBUG.print("next start is ");
              DEBUG.println(start);
              break;
              
            // 送信のスタート位置
            case '.':
              DEBUG.println("period");
              start = i;
              break;
              
            // 送信のゴールの位置
            case ',':
              DEBUG.println("camma");
              // if通るとstartが初期化されるため一度しか引っかからない
              if(start != -1){
                // 送信までの処理
                
                // 多数決を取る
                // 一番msgs_nが大きいインデックスのmsgsをmajority_dataに
                // 2:2:1になった場合どうするか
                max_n = 0;
                majority_data;
                
                for(j = 0; j < m_i; j++){
                  if(max_n < msgs_n[j]){
                    max_n = msgs_n[j];
                    majority_data = msgs[j];
                    
                    DEBUG.println(max_n);
                  }
                }
                DEBUG.print("majority data is ");
                DEBUG.println(majority_data);
  
                //キャストして送る
                postData(majority_data.c_str());
                
                // msgs, msgs_n. m_i, start, goalの初期化
                start = -1;
                goal = -1;
                m_i = 0;
                msgs[5];
                msgs_n[5];
              }
              break;
              
            // 普通の文字の時
            default:
              // DEBUG.print(str_data[i]);
              
              break;
          }
        }
    }
        

        
    #ifdef RESET_DURATION
      if(millis() > RESET_DURATION ){
        delay(1000);
        software_reset();
      }
    #endif
}

void postData(const char *data){
  // majority_data.c_str()
  char payload[120];
  sprintf_P(payload, PSTR("{\"post_data\":\"%s\"}"), data);

  DEBUG.listen();
  DEBUG.println(payload);

  LTE_M_shieldUART.listen();
  /* connect */
  if (!ctx.connect(ENDPOINT, 80)) {
    DEBUG.listen();
    DEBUG.println(F("failed."));
    delay(3000);
    return;
  }
  /* send request */
  char hdr_buf[40];
  ctx.println(F("POST /api/post HTTP/1.1"));
  sprintf_P(hdr_buf, PSTR("Host: %s"), ENDPOINT);
  ctx.println(hdr_buf);
  ctx.println(F("Content-Type: application/json"));
  sprintf_P(hdr_buf, PSTR("Content-Length: %d"), strlen(payload));
  ctx.println(hdr_buf);
  ctx.println();
  ctx.println(payload);
  
  /* receive response */
  while (ctx.connected()) {
    String line = ctx.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  // NOTE: response body is ignore
  ctx.stop();
}


// 外部割り込みの結果呼び出される関数
void wakeUpNow()
{
  DEBUG.println("begin wakeUpNow()");
  DEBUG.println("done wakeUpNow()");
}

void Sleep()
{
  DEBUG.println("begin Sleap()");
  delay(100);
  // アナログ・デジタル・コンバータ機能を無効にする（少しでも電力消費を抑えるため）
  byte adcsra_old = ADCSRA; // ADC Control と Register Aの設定値の以前の値を保持。
  ADCSRA = 0;   // アナログ・デジタル・コンバータ機能の無効にする。

  // スリープモードを設定する
  set_sleep_mode(SLEEP_MODE_STANDBY);

  // 割り込み機能を無効にする
  noInterrupts(); 

  // 電圧降下検出機能を無効にする
  sleep_bod_disable();

  // スリープ機能を有効にする
  sleep_enable();

  // 割り込み機能を有効にする
  interrupts();

  // 外部割り込み条件を設定する（Wakeするための外部割り込み条件の割込設定）
  attachInterrupt(0,wakeUpNow, LOW);  // INT0端子(２番ピン)の立ち下がりで割り込み発生。wakeUpNow関数が呼ばれる。

  // スリープ実行
  sleep_cpu();

  // スリープ機能を無効にする（設定した外部割り込みでWakeしたので、スリープを無効にする）
  sleep_disable();

  // 外部割り込み条件の設定を解除する（Wakeしたので）
  detachInterrupt(0);

  // アナログ・デジタル・コンバータ機能を有効にする（Wakeしたので）
  ADCSRA = adcsra_old;

  DEBUG.println("done Sleap()");
}
