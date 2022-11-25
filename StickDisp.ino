/**
 * @file StickDisp.ino
 * @author a.amg
 * @brief M5stickC plus 用簡易表示端末化プログラム
 * @version 0.4
 * @date 2022-11-25
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <M5StickCPlus.h>
#include <time.h> 

TFT_eSprite sprite = TFT_eSprite(&M5.Lcd);

hw_timer_t *wdtimer = NULL;

#define TIMEOUT 1000
#define CMDSTR_MAX 32

#define LCDCONTENT_MAXSCENE 3
#define LCDCONTENT_VALLINES 4

#define VERSION_STRING "0.4.0"

#define LEDMODE_OFF 0
#define LEDMODE_ON 1
#define LEDMODE_ONESHOT 2

typedef struct tLCDCONTENT {
    bool modified;
    int scene;
    struct tm dttime;
    uint8_t led;
    int debug;
    uint16_t clk_color;
    String valuestr[LCDCONTENT_VALLINES][2];
    String toppgstr[2];
} LCDCONTENT;

static LCDCONTENT lcdinfo;

typedef struct tBEEPCTRL {
    int tone;
    int count;
} BEEPCTRL;

static BEEPCTRL beepctrl;

void LCDC_init() 
{
    lcdinfo.modified = true;
    lcdinfo.scene = 0;
    lcdinfo.toppgstr[0] = "";
    lcdinfo.toppgstr[1] = "";
    lcdinfo.dttime.tm_sec = 0;
    lcdinfo.led = 0;
    lcdinfo.debug = 0;
    lcdinfo.clk_color = WHITE;

    M5.Lcd.setRotation(1);
    sprite.setColorDepth(16);
    sprite.createSprite(M5.Lcd.width(), M5.Lcd.height());

    for(int i = 0; i < LCDCONTENT_VALLINES; i++)
    {
        lcdinfo.valuestr[i][0] = "Label";
        lcdinfo.valuestr[i][1] = "Value";
    }
}


/**
 * @brief 時計画面の描画
 */
void LCDC_show_scene0()
{
    sprite.fillRect(0,0,M5.Lcd.width(), M5.Lcd.height(), BLACK);

    // Current time
    sprite.setTextColor(WHITE, BLACK);
    sprite.setTextFont(4);  // 26pixel font
    sprite.setCursor(0,0);
    sprite.printf("%04d/%02d/%02d", lcdinfo.dttime.tm_year + 1900, lcdinfo.dttime.tm_mon + 1, lcdinfo.dttime.tm_mday);

    sprite.setTextColor(lcdinfo.clk_color, BLACK);
    sprite.setTextFont(7);  // 48pixel font
    sprite.setCursor(0,30);
    sprite.printf("%02d:%02d:%02d", lcdinfo.dttime.tm_hour, lcdinfo.dttime.tm_min, lcdinfo.dttime.tm_sec);

    sprite.setTextColor(WHITE, BLACK);
    sprite.setTextFont(4);  // 26pixel font
    sprite.setTextSize(1);
    sprite.setCursor(0, 84);
    sprite.print(lcdinfo.toppgstr[0]);
    sprite.setCursor(0, 110);
    sprite.print(lcdinfo.toppgstr[1]);

    sprite.pushSprite(0, 0);
}


/**
 * @brief テーブル画面の描画
 */
void LCDC_show_scene1()
{
    const int LBLWIDTH = 80;
    int i, x, y;

    sprite.fillRect(0,0,M5.Lcd.width(), M5.Lcd.height(), NAVY);
    sprite.setTextFont(4); // 26pix font
    sprite.setTextSize(1);
    sprite.setTextColor(WHITE, NAVY);

    x = 4;
    for(i = 0; i < LCDCONTENT_VALLINES; i++)
    {
        y = i * 30 + 4;
        sprite.setCursor(x,y);
        sprite.print(lcdinfo.valuestr[i][0]);
    }

    x = LBLWIDTH +4;
    for(i = 0; i < LCDCONTENT_VALLINES; i++)
    {
        y = i * 30 + 4;
        sprite.setCursor(x,y);
        sprite.print(lcdinfo.valuestr[i][1]);
    }

    // 枠線
    for(i = 0; i <= LCDCONTENT_VALLINES; i++)
    {
        y = i * 30;
        sprite.drawLine(0, y, M5.Lcd.width(), y, DARKGREY);
    }
    sprite.drawLine(LBLWIDTH, 0, LBLWIDTH, y, DARKGREY);

    sprite.pushSprite(0, 0);
}


/**
 * @brief シャットダウン画面の描画
 * 
 * シャットダウン画面にはバージョン情報も出す
 */
void LCDC_show_scene2()
{
    sprite.fillRect(0,0,M5.Lcd.width(), M5.Lcd.height(), MAROON);
    sprite.setTextFont(4); // 26pix font
    sprite.setTextSize(1);
    sprite.setTextColor(WHITE, MAROON);
    sprite.setCursor(0,M5.Lcd.height()/2 - 13);
    sprite.printf("Shutdown?");

    sprite.setTextFont(2); // 14pix font
    sprite.setCursor(0,M5.Lcd.height() - 14);
    sprite.printf("Ver. " VERSION_STRING);
    sprite.pushSprite(0, 0);
}


/**
 * @brief LCDの表示更新． 100ms毎に呼ばれる．
 */
void LCDC_show()
{
    // 表示内容に変更があった場合のみ再描画
    if( lcdinfo.modified == true )
    {
        switch(lcdinfo.scene)
        {
            case 0:
                LCDC_show_scene0();
                break;
            case 1:
                LCDC_show_scene1();
                break;
            case 2:
                LCDC_show_scene2();
                break;
            default:
                LCDC_show_scene0();
                break;
        }
    }

    // LED点灯状態の更新
    if( lcdinfo.led != LEDMODE_OFF )
    {
        digitalWrite(GPIO_NUM_10, LOW);     // LOWで点灯
    }
    else
    {
        digitalWrite(GPIO_NUM_10, HIGH);
    }

    // oneshot の場合は次のタイミング(100ms後)では消灯
    if( lcdinfo.led == LEDMODE_ONESHOT)     
    {
        lcdinfo.led = LEDMODE_OFF;
    }

    lcdinfo.modified = false;
}


/**
 * @brief RTC読み出し
 * @param tm 時刻情報格納先
 */
void rtc_read(struct tm *tm)
{
    RTC_DateTypeDef DateStruct;
    RTC_DateTypeDef DateStruct2;
    RTC_TimeTypeDef TimeStruct;

    do{
        M5.Rtc.GetData(&DateStruct);
        M5.Rtc.GetTime(&TimeStruct);
        M5.Rtc.GetData(&DateStruct2);
    }while( DateStruct.Date != DateStruct2.Date );

    tm->tm_year = DateStruct.Year - 1900;
    tm->tm_mon = DateStruct.Month - 1;
    tm->tm_mday = DateStruct.Date;
    tm->tm_hour = TimeStruct.Hours;
    tm->tm_min = TimeStruct.Minutes;
    tm->tm_sec = TimeStruct.Seconds;
}


/**
 * @brief RTC読み出し．時刻が進むまで待つ．
 * @param tm 時刻情報格納先
 */
void rtc_read_step(struct tm *tm)
{
    struct tm told;

    rtc_read(&told);
    do{
        rtc_read(tm);
        if( told.tm_sec == tm->tm_sec )
        {
            delay(10);
        }
    }while(told.tm_sec == tm->tm_sec);
}



/**
 * @brief RTC書き込み
 * @param tm 時刻情報
 */
void rtc_write(struct tm *tm)
{
    RTC_DateTypeDef DateStruct;
    RTC_TimeTypeDef TimeStruct;

    DateStruct.Year = tm->tm_year + 1900;
    DateStruct.Month = tm->tm_mon + 1;
    DateStruct.Date = tm->tm_mday;
    TimeStruct.Hours = tm->tm_hour;
    TimeStruct.Minutes = tm->tm_min;
    TimeStruct.Seconds = tm->tm_sec;

    M5.Rtc.SetData(&DateStruct);
    M5.Rtc.SetTime(&TimeStruct);
}


void IRAM_ATTR resetModule() 
{
    ets_printf("Reboot\n");
    esp_restart();
}


/**
 * @brief セットアップ
 */
void setup()
{
    // Initialize the M5Stack object
    M5.begin();

    beepctrl.tone = 1000;
    beepctrl.count = 0;
    M5.Beep.tone(1000);
    M5.Beep.mute();

    LCDC_init();

    // LEDの制御 LOWを出力すると点灯する
    pinMode(GPIO_NUM_10, OUTPUT);
    digitalWrite(GPIO_NUM_10, HIGH);

    // シリアルポートのレート
    Serial.begin(115200);

    // RTCから初期値の読み出し
    rtc_read(&lcdinfo.dttime);
    LCDC_show();

    wdtimer = timerBegin(0, 80, true);
    timerAttachInterrupt(wdtimer, &resetModule, true);
    timerAlarmWrite(wdtimer, TIMEOUT * 1000, false);
    timerAlarmEnable(wdtimer);
}


/**
 * @brief コマンド文字列の確認．
 * @param buf コマンド文字列
 * @return 1:正常，0:異常
 */
int serial_isvalid(const uint8_t *buf)
{
    int i;
    int rtn;
    
    rtn = 1;
    for( i = 0; i < CMDSTR_MAX; i++)
    {
        if( buf[i] == 0 )
        {
            rtn = 1;
            break;
        }
        if(buf[i] < ' ' || buf[i] > '~')
        {
            rtn = 0;
            break;
        }
    }
    return rtn;
}


/**
 * @brief hex文字から整数へ変換．
 * 
 * @param c 
 * @return 変換した整数(0-15)．失敗した場合は-1
 */
inline int hexchar2dec(char c)
{
    if( c >= '0' && c <= '9' )
    {
        return c - '0';
    }
    if( c >= 'A' && c <= 'F' )
    {
        return c - 'A' + 10;
    }
    if( c >= 'a' && c <= 'f' )
    {
        return c - 'a' + 10;
    }
    return -1;
}


/**
 * @brief コマンドを1つ処理する
 * @param buf コマンド文字列
 * @return 0:正常
 */
int serial_cmd(const uint8_t *buf)
{
    struct tm tm;
    int i, scn;
    int l,c,n;
    int cr, cb, cg;

    if( serial_isvalid(buf) == 0 )
    {
        return 1;
    }

    switch( buf[0] )
    {
        // Read RTC
        case 'r':
            rtc_read(&tm);
            Serial.printf("$r%04d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
            Serial.printf("%02d%02d%02d\r\n", tm.tm_hour, tm.tm_min, tm.tm_sec);
            break;

        // 秒が更新される瞬間まで待って返答する read
        case 'R':
            rtc_read_step(&tm);
            Serial.printf("$R%04d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
            Serial.printf("%02d%02d%02d\r\n", tm.tm_hour, tm.tm_min, tm.tm_sec);
            break;

        // Write RTC
        case 'w':
            for(i = 1; i<= 14; i++)
            {
                if( buf[i] < '0' || buf[i] > '9' )
                {
                    return 1;
                }
            }
            tm.tm_year = (buf[1] - '0') * 1000 
                + (buf[2] - '0') * 100 
                + (buf[3] - '0') * 10 
                + (buf[4] - '0') - 1900;
            tm.tm_mon = (buf[5] - '0') * 10 + (buf[6] - '0') - 1;
            tm.tm_mday = (buf[7] - '0') * 10 + (buf[8] - '0');
            tm.tm_hour = (buf[9] - '0') * 10 + (buf[10] - '0');
            tm.tm_min = (buf[11] - '0') * 10 + (buf[12] - '0');
            tm.tm_sec = (buf[13] - '0') * 10 + (buf[14] - '0');
            rtc_write(&tm);
            break;

        // 時刻画面に表示する文字列
        case 't':
            n = buf[1] - '0';
            if( n < 0 || n > 1 )
            {
                return 1;
            }
            lcdinfo.toppgstr[n] = (const char*)&buf[2];
            lcdinfo.modified = true;
            break;

        // 時刻の文字の色
        // cRGB で，R,G,Bはそれぞれ0-Fの値．M5StickのRGBはビット数が565なので，変換が必要．
        case 'c':
            cr = hexchar2dec(buf[1]) * 15;
            cg = hexchar2dec(buf[2]) * 15;
            cb = hexchar2dec(buf[3]) * 15;
            if( cr >= 0 && cb >= 0 && cg >= 0 )
            {
                lcdinfo.clk_color = M5.lcd.color565(cr, cg, cb);
            }
            lcdinfo.modified = true;
            break;

        // Power off
        case 'p':
            M5.Axp.PowerOff();
            break;

        // LED
        case 'l':
            if( buf[1] == '1' )
            {
                lcdinfo.led = LEDMODE_ON;
            }
            else if (buf[1] == '2')
            {
                lcdinfo.led = LEDMODE_ONESHOT;
            }
            else
            {
                lcdinfo.led = LEDMODE_OFF;
            }
            break;

        // Scene選択
        case 's':
            scn = buf[1] - '0';
            if( scn < 0 || scn >= LCDCONTENT_MAXSCENE )
            {
                return 1;
            }
            lcdinfo.scene = scn;
            lcdinfo.modified = true;
            break;

        // 表画面の汎用文字列
        // v10abcdefg という形式で，1が行，0が列を表す．0列目がラベルに相当．
        case 'v':
            l = buf[1] - '0';
            c = buf[2] - '0';
            if( l < 0 || l >= LCDCONTENT_VALLINES )
            {
                return 1;
            }
            if( c < 0 || c > 1 )
            {
                return 1;
            }
            lcdinfo.valuestr[l][c] = (const char*)&buf[3];
            lcdinfo.modified = true;
            break;

        // バージョン取得
        case 'V':
            Serial.printf("$Vstickdisp " VERSION_STRING "\r\n");
            break;

        default:
            // Error
            return 1;
    }

    return 0;
}


/**
 * @brief シリアルから入ってきた文字列の処理．
 * 
 * @param c 入力された文字
 * @return 0:正常
 */
int serial_parse(uint8_t c)
{
    static int rxstate = 0;
    static int rxptr = 0;
    static uint8_t rxbuf[CMDSTR_MAX];
    int rtn;

    if(c == 0 )
    {
        rxstate = 0;
        rxptr = 0;
        return 0;
    }

    rtn = 0;
    switch(rxstate)
    {
        case 0:
            if(c == '$')
            {
                rxptr = 0;
                rxstate = 1;
            }
            break;
        case 1:
            if(c == '\r' || c == '\n' )
            {
                rxbuf[rxptr] = 0;
                rtn = serial_cmd(rxbuf);
                rxstate = 0;
            }
            else
            {
                rxbuf[rxptr++] = c;
                if( rxptr >= sizeof(rxbuf) )   // overflow
                {
                    rxstate = 0;
                    rxptr = 0;
                    rtn = 1;
                }
            }
            break;
        default:
            rxstate = 0;
            break;
    }

    return rtn;
}


/**
 * @brief ビープ音制御のためのポーリング用関数
 * 
 */
void beep_poll()
{
    if( beepctrl.count > 0 )
    {
        beepctrl.count--;
    }
    else
    {
        M5.Beep.mute();
    }
}


/**
 * @brief  ピっという音を鳴らす
 * 
 * @param tone 音の高さ
 * @param count 鳴らす時間 (10ms単位)
 */
void beep_trigger(int tone, int count)
{
    beepctrl.tone = tone;
    beepctrl.count = count;
    M5.Beep.tone(beepctrl.tone);
}


/**
 * @brief 10ms周期で回るループ
 * 
 */
void loop() 
{
    uint8_t rxc;
    static uint8_t updatecount = 0;
    int tsec;

    timerWrite(wdtimer, 0);

    M5.update();
    while( Serial.available() > 0 )
    {
        rxc = Serial.read();
        serial_parse(rxc);
    }

    if( M5.BtnA.wasPressed() )
    {
        Serial.printf("$b%dA\r\n", lcdinfo.scene);
        beep_trigger(1000, 10);
    }

    if( M5.BtnB.wasPressed() )
    {
        Serial.printf("$b%dB\r\n", lcdinfo.scene);
        lcdinfo.scene++;
        if( lcdinfo.scene >= LCDCONTENT_MAXSCENE )
        {
            lcdinfo.scene = 0;
        }
        lcdinfo.modified = true;
    }

    beep_poll();

    // 画面は100ms毎に更新
    if( updatecount == 0)
    {
        tsec = lcdinfo.dttime.tm_sec;
        rtc_read(&lcdinfo.dttime);
        if( tsec != lcdinfo.dttime.tm_sec )
        {
            lcdinfo.modified = true;
        }
        LCDC_show();
        updatecount = 10;
    }
    else
    {
        updatecount--;
    }

    delay(10);
}
