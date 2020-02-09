#include <CapacitiveSensor.h>

CapacitiveSensor   capsensor = CapacitiveSensor(4, 2);       // 10M resistor between pins 4 & 2, pin 2 is sensor pin, add a wire and or foil if desired

int BUTTON_DOWN  = 7; // any time the button is pressed, this is HI
int DOUBLE_CLICK = 8; // if we detect a double-click, HI for 1s
int LONG_PRESS   = 9; // if we detect a long press (before release), HI for 1s

// 750 is the magic number, adjust based on the material used for the touch
int TOUCH_THRESHOLD = 750;
int READ_DELAY      = 30;      // ms
int LONG_DELAY      = 3000;    // ms
int DBL_UPCANCEL    = 1000;     // ms
int DBL_DOWNCANCEL  = 750;     // ms
int STATE_DURATION  = 1000;   // ms, time to make the LONG_PRESS or DOUBLE_CLICK high

void setup()
{
  // not sure if this is required
  capsensor.set_CS_AutocaL_Millis(0xFFFFFFFF);
  // don't need this unless debugging
  // Serial.begin(9600);

  // setup output pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_DOWN, OUTPUT);
  pinMode(DOUBLE_CLICK, OUTPUT);
  pinMode(LONG_PRESS, OUTPUT);
}

enum 
{
  stateDefault,
  stateSingleOrLong,
  stateSingleOrDouble,
  stateLong,
  stateDoubleOrLong
};

// detect button down
bool isButtonDown() 
{
  long total =  capsensor.capacitiveSensor(READ_DELAY);
  return total > TOUCH_THRESHOLD;
}

// state monitors
unsigned long down0 = 0;
unsigned long up0 = 0;
int state = stateDefault;
int oldState = stateDefault;

// S0
int handleDefault(bool down) 
{
  if (down) 
  {
    down0 = millis();
    up0 = down0;
    return stateSingleOrLong;
  }
  // no change
  return state;
}

// S1
int singleOrLong(bool down)
{
  if (!down)
  {
    up0 = millis();
    return stateSingleOrDouble;
  }
  if (millis() - up0 > LONG_DELAY) 
  {
    Serial.print("Long delay: ");
    Serial.println(millis() - up0);
    digitalWrite(LONG_PRESS, HIGH);
    delay(STATE_DURATION);
    digitalWrite(LONG_PRESS, LOW);
    Serial.println("Long delay over");
    up0 = millis();
    return stateLong;
  }
  // no change
  return state;
}

// S2
int singleOrDouble(bool down)
{
  if (down)
  {
    down0 = millis();
    return stateDoubleOrLong;
  }
  if (up0 - down0 > DBL_UPCANCEL) 
  {
    return stateDefault;
  }
  if (millis() - up0 > DBL_UPCANCEL)
  {
    return stateDefault;
  }
  return state;
}

// S3
int longPress(bool down) 
{
  if (!down)
  {
    return stateDefault;
  }
  return state;
}

// S4
int doubleOrLong(bool down)
{
  if (!down)
  {
    Serial.println("Double");
    digitalWrite(DOUBLE_CLICK, HIGH);
    delay(STATE_DURATION);
    digitalWrite(DOUBLE_CLICK, LOW);
    return stateDefault;
  }
  if (millis() - down0 > DBL_DOWNCANCEL)
  {
    return stateSingleOrLong;
  }
  return state;
}

int lastPrint = 0;

// main loop
void loop()
{
  bool down = isButtonDown();
  // this is common to all states
  digitalWrite(LED_BUILTIN, down);
  digitalWrite(BUTTON_DOWN, down);

  if (false) // oldState != state) 
  {
    Serial.print("State: ");
    Serial.print(state);
    Serial.print("/");
    Serial.print(down0);
    Serial.print("/");
    Serial.println(up0);
    oldState = state;
  }

  switch (state)
  {
    case stateDefault:
      state = handleDefault(down);
      break;
    case stateSingleOrLong:
      state = singleOrLong(down);
      break;
    case stateSingleOrDouble:
      state = singleOrDouble(down);
      break;
    case stateLong:
      state = longPress(down);
      break;
    case stateDoubleOrLong:
      state = doubleOrLong(down);
      break;
    default:
      state = 0;
      break;
  } 
}
