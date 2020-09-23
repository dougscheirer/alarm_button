#include <CapacitiveSensor.h>

CapacitiveSensor   capsensor = CapacitiveSensor(4, 2);       // 10M resistor between pins 4 & 2, pin 2 is sensor pin, add a wire and or foil if desired

int BUTTON_DOWN  = 7; // any time the button is pressed, this is HI
int DOUBLE_CLICK = 8; // if we detect a double-click, HI for 1s
int LONG_PRESS   = 9; // if we detect a long press (before release), HI for 1s

int TOUCH_THRESHOLD = 750;   // 750 is the magic number, adjust based on the material used for the touch
int READ_DELAY      = 30;    // ms
int LONG_DELAY      = 3000;  // ms
int UP_DEBOUNCE     = 100;   // ms
int DBL_DEBOUNCE    = 100;   // ms
int DBL_UPCANCEL    = 1000;  // ms
int DBL_DOWNCANCEL  = 750;   // ms
int STATE_QUIET     = 750;   // ms, time required in state of no press activity before next state transisiton
int STATE_DURATION  = 500;   // ms, time to make the LONG_PRESS or DOUBLE_CLICK high

bool debugging = false;

typedef struct ledstate
{
  bool isOn;
  bool wasOn;
  unsigned long wantOn;
  int led;
} LEDState;

ledstate ledState[2];

// we run an event loop to control LED states
// set to true when we set the LED high or low
enum
{
  Double,
  Long
};

void setup()
{
  // not sure if this is required
  capsensor.set_CS_AutocaL_Millis(0xFFFFFFFF);
  // don't need this unless debugging
  if (!debugging) {
    Serial.begin(9600);
  }

  ledState[Double].led = DOUBLE_CLICK;
  ledState[Long].led = LONG_PRESS;

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
  stateDoubleOrLong,
  stateWaitForQuiet
};

// detect button down with debounce
long lastUpSignal = -1; // -1 is reset, 0 is saw a down, other is time of last up transition
bool isButtonDown() 
{
  long total =  capsensor.capacitiveSensor(READ_DELAY);
  if (total > TOUCH_THRESHOLD) {
    lastUpSignal = 0;
    return true;
  }
  if (lastUpSignal == -1) {
    return false; // never saw a down
  }
  long now = millis();
  if (lastUpSignal == 0) {
    // still down until the debounce goes through
    lastUpSignal = now;
    return true;
  }
  if (now - lastUpSignal > UP_DEBOUNCE) {
    lastUpSignal = -1;
    return false;
  }
  return true;
}

// state monitors
unsigned long down0 = 0;
unsigned long up0 = 0;
int state = stateDefault;
int oldState = stateDefault;


// to exit the double or long position, how long since no button press
unsigned long wantQuiet = 0;

int setWantOn(int dorl)
{
  ledState[dorl].isOn = false;
  ledState[dorl].wasOn = false;
  ledState[dorl].wantOn = millis();
  return stateWaitForQuiet;
}

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
  unsigned long now = millis();
  if (!down)
  {
    up0 = now;
    return stateSingleOrDouble;
  }
  if (now - up0 > LONG_DELAY) 
  {
    return setWantOn(Long);
  }
  // no change
  return state;
}

// S2
int singleOrDouble(bool down)
{
  if (down)
  {
    // if it's less than the debounce time, ignore it
    if (millis() - up0 < DBL_DEBOUNCE) 
    {
      return state;
    }
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

// S4
int doubleOrLong(bool down)
{
  unsigned long now = millis();
  if (!down)
  {
    return setWantOn(Double);
  }
  if (now - down0 > DBL_DOWNCANCEL)
  {
    return stateSingleOrLong;
  }
  return state;
}

int handleLEDState(bool down, int dorl)
{
  unsigned long now = millis();
  if (!ledState[dorl].isOn && !ledState[dorl].wasOn) {
    digitalWrite(BUTTON_DOWN, LOW); 
    digitalWrite(ledState[dorl].led, HIGH);
    ledState[dorl].isOn = true;
    wantQuiet = now;
    return state;
  }
  // time to turn the LED off?
  if (now - ledState[dorl].wantOn > STATE_DURATION) {
    digitalWrite(ledState[dorl].led, LOW);
    ledState[dorl].isOn = false;
    ledState[dorl].wasOn = true;
  }
  if (now - wantQuiet > STATE_QUIET) {
    // can exit the state now
    ledState[dorl].isOn = false;
    ledState[dorl].wasOn = false;
    ledState[dorl].wantOn = 0;
    return stateDefault;
  }
  if (down) {
     wantQuiet = now;
  }
  return state;
}

int writeWantedPositions(bool down) 
{
  digitalWrite(LED_BUILTIN, down);
  unsigned long now = millis();
  if (ledState[Double].wantOn > 0) {
    return handleLEDState(down, Double);
  }
  // same for long press
  if (ledState[Long].wantOn > 0) {
    return handleLEDState(down, Long);
  }
  
  digitalWrite(BUTTON_DOWN, down); 
  return state;
}

int lastPrint = 0;

// main loop
void loop()
{
  bool down = isButtonDown();
  state = writeWantedPositions(down);

  if (debugging && oldState != state) 
  {
    Serial.print("State: ");
    Serial.print(state);
    Serial.print("/");
    Serial.print(ledState[Long].wantOn);
    Serial.print("/");
    Serial.print(ledState[Long].isOn);
    Serial.print("/");
    Serial.print(ledState[Long].wasOn);
    Serial.print("/");
    Serial.println(wantQuiet);
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
    case stateWaitForQuiet:
      break;
    case stateDoubleOrLong:
      state = doubleOrLong(down);
      break;
    default:
      state = stateDefault;
      break;
  } 
}
