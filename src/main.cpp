#include <Arduino.h>
#include <RCSwitch.h>

#define FAKE_VCC_RF 3
#define FAKE_GND_BUZ 8
#define CONTROL_PIN 12
#define LED_PIN 11
#define FAKE_VCC_BUZ 5

#define BTN_REPEAT_VAL 1000
#define BREEZE_MIN_VAL 2000
#define BREEZE_MAX_VAL 5500
#define TIMER_DISCRET_VAL 1800000 // 30 min
#define TIMER_LIMIT_VAL 10800000  // 3 hour

#define BTN_CHANGE_MODE_CODE 0x3300C0
#define BTN_TIMER_MODE_CODE 0x330000

#define BTN_CHANGE_MODE_CODE2 0xdb6a1

RCSwitch rf = RCSwitch();

enum states
{
    on = 1,
    breeze,
    off
};

uint8_t curState = 3, buzPart = 0, buzAdr = 0, buzCnt = 0;
uint16_t breezeRnd = 0, buzDel = 60;
uint16_t buzCounts[7];
uint32_t tmCheck = 0, tmBreeze = 0, tmTimer = 0, timerTime = 0, tmBuz = 0;
bool enBreeze = false, enAvailHeadler = false, enTimer = false;

bool debug = false;

void AvailableHeadler();
void BreezeHeadler();
void SetFanOn();
void SetFanOff();
void ResetTm();
void SetBreezeOn();
void SetRndBreezeVal();
void TimerHeadler();
void IncreseTimer();
void SetTimerOff();
void BuzzHeadler();
void BuzDirect(uint8_t a, uint16_t d);
void BuzPattern(uint16_t one = 0, uint16_t two = 0, uint16_t three = 0, uint16_t fore = 0, uint16_t five = 0, uint16_t six = 0, uint16_t seven = 0);
void SetBuzDelay(uint16_t dl);

void setup()
{
    if (debug)
        Serial.begin(115200);

    pinMode(FAKE_VCC_RF, OUTPUT);
    pinMode(FAKE_VCC_BUZ, OUTPUT);
    pinMode(FAKE_GND_BUZ, OUTPUT);
    pinMode(CONTROL_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);

    digitalWrite(FAKE_VCC_RF, HIGH);
    digitalWrite(FAKE_VCC_BUZ, LOW);
    digitalWrite(FAKE_GND_BUZ, LOW);
    digitalWrite(CONTROL_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    randomSeed(analogRead(A0));

    rf.enableReceive(0); // Receiver on interrupt 0 => that is pin #2

    SetBuzDelay(60);
    BuzPattern(500, 80); // ready to listen commands

    // Code scanner--------------------------------------------------
    // while (1)
    // {
    //     if (rf.available())
    //     {
    //         Serial.println("Code: " + String(rf.getReceivedValue(), HEX));

    //         rf.resetAvailable();
    //     }

    //     delay(100);
    // }
    //---------------------------------------------------------------
}

void loop()
{
    BuzzHeadler();
    AvailableHeadler();
    BreezeHeadler();
    TimerHeadler();

    if (rf.available())
    {
        uint32_t com = rf.getReceivedValue();

        if (debug)
            Serial.println("Command: " + String(com, HEX));

        switch (com)
        {
        case BTN_CHANGE_MODE_CODE: // enable,breeze,off
            curState++;

            if (curState > off)
                curState = on;

            switch (curState)
            {
            case on:
                if (debug)
                    Serial.println("Enable mode");

                SetFanOn();

                break;

            case breeze:
                if (debug)
                    Serial.println("Breeze mode");

                SetBreezeOn();

                break;

            case off:
                if (debug)
                    Serial.println("Disable mode");

                SetFanOff();
            }

            break;

        case BTN_TIMER_MODE_CODE: // timer
            if (curState != off)
                IncreseTimer();
            else if (debug)
                Serial.println("Impossible, current state is disadled");

            break;

        default:
            if (debug)
                Serial.println("Unknown comand");

            break;
        }

        enAvailHeadler = true;

        rf.resetAvailable();
        rf.disableReceive();

        ResetTm();
    }
}

void ResetTm()
{
    tmCheck = millis();
}

void AvailableHeadler()
{
    if (enAvailHeadler && millis() - tmCheck >= BTN_REPEAT_VAL)
    {
        enAvailHeadler = false;
        rf.enableReceive(0);
    }
}

void SetFanOn()
{
    SetBuzDelay(60);
    BuzPattern(40, 40);

    digitalWrite(CONTROL_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
}

void SetFanOff()
{
    BuzDirect(0, 600);

    curState = off;
    enBreeze = false;
    enTimer = false;
    timerTime = 0;
    buzCnt = 0;

    digitalWrite(CONTROL_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
}

void SetBreezeOn()
{
    SetBuzDelay(60);
    BuzPattern(40, 40);

    enBreeze = true;
    SetRndBreezeVal();
}

void BreezeHeadler()
{
    if (millis() - tmBreeze > breezeRnd && enBreeze)
    {
        digitalWrite(CONTROL_PIN, !digitalRead(CONTROL_PIN));
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));

        SetRndBreezeVal();

        tmBreeze = millis();
    }
}

void SetRndBreezeVal()
{
    breezeRnd = random(BREEZE_MIN_VAL, BREEZE_MAX_VAL);
}

void IncreseTimer()
{
    timerTime += TIMER_DISCRET_VAL;
    buzCnt++;

    if (timerTime > TIMER_LIMIT_VAL)
        timerTime = 0;

    if (debug)
        Serial.println("Timer " + String(timerTime / 60000));

    if (buzCnt == 7)
    {
        buzCnt = 0;

        SetBuzDelay(30);
        BuzPattern(30, 30, 30, 30, 30);
    }
    else
    {
        SetBuzDelay(120);

        for (size_t i = 0; i < buzCnt; i++)
            BuzDirect(i, 80);
    }

    enTimer = true;
    tmTimer = millis();
}

void TimerHeadler()
{
    if (timerTime != 0) // if while incresing by cycle, don't disadle every crpssing 0
        if (enTimer && millis() - tmTimer > timerTime)
            SetTimerOff();
}

void SetTimerOff()
{
    if (debug)
        Serial.println("Disable by timer");

    SetFanOff();
}

void BuzzHeadler()
{
    if (buzCounts[buzAdr] != 0)
    {
        switch (buzPart)
        {
        case 0:
            digitalWrite(FAKE_VCC_BUZ, 1); // start buzz

            buzPart++;

            tmBuz = millis();

            break;
        case 1:
            if (millis() - tmBuz >= buzCounts[buzAdr])
            {
                digitalWrite(FAKE_VCC_BUZ, 0); // stop buzz

                buzPart++;

                tmBuz = millis();
            }

            break;
        case 2:
            if (millis() - tmBuz >= buzDel) // delay detween stop
            {
                buzPart = 0;
                buzCounts[buzAdr] = 0;

                buzAdr++;

                if (buzAdr == 7)
                    buzAdr = 0;
            }

            break;
        }
    }
    else
    {
        buzAdr = 0;
        buzPart = 0;
    }
}

void BuzDirect(uint8_t a, uint16_t d)
{
    buzCounts[a] = d;

    // for (size_t i = 0; i < 7; i++)
    // {
    // if (debug)
    // {
    //     Serial.print(buzCounts[i]);
    //     Serial.print(" ");
    // }
    // }

    // if (debug)
    // Serial.println();
}

void BuzPattern(uint16_t one = 0, uint16_t two = 0, uint16_t three = 0, uint16_t fore = 0, uint16_t five = 0, uint16_t six = 0, uint16_t seven = 0)
{
    buzCounts[0] = one;
    buzCounts[1] = two;
    buzCounts[2] = three;
    buzCounts[3] = fore;
    buzCounts[4] = five;
    buzCounts[6] = six;
    buzCounts[7] = seven;
}

void SetBuzDelay(uint16_t dl)
{
    buzDel = dl;
}