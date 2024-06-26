#include "ferraris.h"

// ugly stuff to get interrupt callable
static void IRAM_ATTR staticInterruptHandler0() { Ferraris::getInstance(0).IRQhandler(); }
#if FERRARIS_NUM > 1
static void IRAM_ATTR staticInterruptHandler1() { Ferraris::getInstance(1).IRQhandler(); }
#endif
#if FERRARIS_NUM > 2
static void IRAM_ATTR staticInterruptHandler2() { Ferraris::getInstance(2).IRQhandler(); }
#endif
#if FERRARIS_NUM > 3
static void IRAM_ATTR staticInterruptHandler3() { Ferraris::getInstance(3).IRQhandler(); }
#endif
#if FERRARIS_NUM > 4
static void IRAM_ATTR staticInterruptHandler4() { Ferraris::getInstance(4).IRQhandler(); }
#endif
#if FERRARIS_NUM > 5
static void IRAM_ATTR staticInterruptHandler5() { Ferraris::getInstance(5).IRQhandler(); }
#endif
#if FERRARIS_NUM > 6
static void IRAM_ATTR staticInterruptHandler6() { Ferraris::getInstance(6).IRQhandler(); }
#endif

void (*staticInterruptHandlers[FERRARIS_NUM])() =
{
  &staticInterruptHandler0,
#if FERRARIS_NUM > 1
  &staticInterruptHandler1,
#endif
#if FERRARIS_NUM > 2
  &staticInterruptHandler2,
#endif
#if FERRARIS_NUM > 3
  &staticInterruptHandler3,
#endif
#if FERRARIS_NUM > 4
  &staticInterruptHandler4,
#endif
#if FERRARIS_NUM > 5
  &staticInterruptHandler5,
#endif
#if FERRARIS_NUM > 6
  &staticInterruptHandler6
#endif
};


// initialize private instance counter
uint8_t Ferraris::FINSTANCE = 0;

// private constructor
Ferraris::Ferraris()
  : m_state(startup)
  , m_config_rev_kWh(75)
  , m_config_debounce(80)
  , m_config_twoway(false)
  , m_timestamp(millis())
  , m_timestampLast1(0)
  , m_timestampLast2(0)
  , m_revolutions(0)
  , m_revolutionsRaw(0)   // revolutions unfiltered
  , m_revolutions5(0)
  , m_revolutions10(0)
  , m_interval1(0)        // suspicious interval detection
  , m_interval2(0)
  , m_interval3(0)
  , m_interval4(0)
  , m_interval5(0)
  , m_intervalTraining(true)
  , m_suspicious(false)
  , m_changed(false)
  , m_direction(+1)
  , m_average_timestamp(0)
  , m_average_revolutions(0)
{
  uint8_t F = Ferraris::FINSTANCE++;
  m_PIN  = Ferraris::PINS[F];
  m_DPIN = Ferraris::DPINS[F];
  pinMode(m_PIN, INPUT_PULLUP);
  switch (F) {
    case 0: m_handler = staticInterruptHandler0; break;
#if FERRARIS_NUM > 1
    case 1: m_handler = staticInterruptHandler1; break;
#endif
#if FERRARIS_NUM > 2
    case 2: m_handler = staticInterruptHandler2; break;
#endif
#if FERRARIS_NUM > 3
    case 3: m_handler = staticInterruptHandler3; break;
#endif
#if FERRARIS_NUM > 4
    case 4: m_handler = staticInterruptHandler4; break;
#endif
#if FERRARIS_NUM > 5
    case 5: m_handler = staticInterruptHandler5; break;
#endif
#if FERRARIS_NUM > 6
    case 6: m_handler = staticInterruptHandler6; break;
#endif
    default: abort();
  }
}

Ferraris& Ferraris::getInstance(unsigned char F)
{
  static Ferraris singleton[FERRARIS_NUM];
  assert(F < FERRARIS_NUM);
  return singleton[F];
}


// ----------------------------------------------------------------------------
// main interface
// ----------------------------------------------------------------------------

void Ferraris::begin()
{
  uint8_t value = digitalRead(m_PIN);

  if (value == FERRARIS_SILVER) {
    m_state = Ferraris::states::silver_debounce;
  }

  if (value == FERRARIS_RED) {
    m_state = Ferraris::states::red_debounce;
    m_timestampLast1 = m_timestamp;
    m_timestampLast2 = m_timestamp;
  }
  m_changed = true;
}

// define IRQ mode to get to PIN state
#if (FERRARIS_RED == HIGH)
  #define FERRARIS_IRQMODE_RED     RISING
  #define FERRARIS_IRQMODE_SILVER  FALLING
#else
  #define FERRARIS_IRQMODE_RED     FALLING
  #define FERRARIS_IRQMODE_SILVER  RISING
#endif

bool Ferraris::loop()
{
  // wait for debounce time
  int timestamp = millis();
  if (m_state == Ferraris::states::silver_debounce)
    if ((timestamp - m_timestamp) >= 4*m_config_debounce) {
      m_state = Ferraris::states::silver;
      detachInterrupt(digitalPinToInterrupt(m_PIN));
      attachInterrupt(digitalPinToInterrupt(m_PIN), m_handler, FERRARIS_IRQMODE_RED);
    }

  if (m_state == Ferraris::states::red_debounce)
    if ((timestamp - m_timestamp) >= m_config_debounce) {
      if (digitalRead(m_PIN) == FERRARIS_RED) {
        m_state = Ferraris::states::red;
        detachInterrupt(digitalPinToInterrupt(m_PIN));
        attachInterrupt(digitalPinToInterrupt(m_PIN), m_handler, FERRARIS_IRQMODE_SILVER);
      } else {
        // we missed RED -> SILVER !
        m_state = Ferraris::states::silver_debounce;
        m_timestamp = timestamp;
        Serial.println("Missed RED->SILVER !");
      }
    }

  // return "something has changed" state
  bool retval = m_changed;
  m_changed = false;
  return retval;
}

void Ferraris::IRQhandler()
{
  // disable interrupt during debounce time
  //detachInterrupt(digitalPinToInterrupt(m_PIN));
  m_timestamp = millis();

  // silver -> red
  if (m_state == Ferraris::states::silver) {
    m_state = Ferraris::states::red_debounce;
    if ((!m_config_twoway) ||
        (m_config_twoway && (digitalRead(m_DPIN) == FERRARIS_SILVER))) {

      if (!m_config_twoway)
        setNewInterval(m_timestamp - m_timestampLast1);
      // suspicious values do not count
      if (!m_suspicious)
        m_revolutions++;
      m_revolutionsRaw++;
      if(m_timestamp - m_timestampLast1 < 5000)
        m_revolutions5++;
      if(m_timestamp - m_timestampLast1 < 10000)
        m_revolutions10++;

      m_direction = std::min(1, m_direction+1);
      m_timestampLast2 = m_timestampLast1;
      m_timestampLast1 = m_timestamp;
      m_changed = true;
    }
  }

  // red -> silver
  if (m_state == Ferraris::states::red) {
    m_state = Ferraris::states::silver_debounce;
    if (m_config_twoway && (digitalRead(m_DPIN) == FERRARIS_SILVER)) {
      m_revolutions--;
      m_direction = std::max(-1, m_direction-1);
      m_timestampLast2 = m_timestampLast1;
      m_timestampLast1 = m_timestamp;
      m_changed = true;
    }
  }
}


// ----------------------------------------------------------------------------
// calculation functions
// ----------------------------------------------------------------------------

// new Interval time [ms]
bool Ferraris::setNewInterval(unsigned long value)
{
  // returns true when the previous interval or patterns were suspicious

  if ((!m_intervalTraining) &&
      (value < 100)) {
        // interval value too short
        m_suspicious = true;
        return true;
  }

  if (m_interval5 > 0)
    m_intervalTraining = false;

  m_interval5 = m_interval4;
  m_interval4 = m_interval3;
  m_interval3 = m_interval2;
  m_interval2 = m_interval1;
  m_interval1 = value;
  m_suspicious = false;

  if (m_intervalTraining)
    return false;

  // LONG SHORT LONG XXXX XXXX
  if (((m_interval2 < 10000)         &&
       (m_interval2 < m_interval1/3) &&
       (m_interval2 < m_interval3/3)   ) ||
  // LONG SHORT SHORT LONG XXXX
      ((m_interval2 < 9000)          &&
       (m_interval3 < 9000)          &&
       (m_interval2 < m_interval1/4) &&
       (m_interval3 < m_interval1/4) &&
       (m_interval2 < m_interval4/4) &&
       (m_interval3 < m_interval4/4)   ) ||
  // LONG SHORT SHORT SHORT LONG
      ((m_interval2 < 8000)          &&
       (m_interval3 < 8000)          &&
       (m_interval4 < 8000)          &&
       (m_interval2 < m_interval1/4) &&
       (m_interval3 < m_interval1/4) &&
       (m_interval4 < m_interval1/4) &&
       (m_interval2 < m_interval5/4) &&
       (m_interval3 < m_interval5/4)   ) ||
  // LONG LONG SHORT LONG LONG
      ((m_interval3 < 20000)         &&
       (m_interval3 < m_interval2/3) &&
       (m_interval3 < m_interval4/3) &&
       (m_interval3 < m_interval1/4) &&
       (m_interval3 < m_interval5/4)   )    ) {
    m_suspicious = true;
    return true;
  }

  return false;
}



bool Ferraris::get_state() const
{
  switch (m_state) {
    case Ferraris::states::silver:
    case Ferraris::states::silver_debounce:
      return FERRARIS_SILVER;
      break;
    default:
      return FERRARIS_RED;
  }
}

// get current consumption based on duration of last revolution
int Ferraris::get_W() const
{
  unsigned long elapsedtime = m_timestampLast1 - m_timestampLast2;  // last full cycle
  if (elapsedtime == 0) return 0;
  unsigned long runningtime = millis() - m_timestampLast1;          // current open cycle
  // [60 min * 60 sec * 1000 ms] * [1 kW -> 1000 W] / [time for 1kWh]
  return m_direction * 3600000000 / (std::max(elapsedtime, runningtime) * m_config_rev_kWh);
}

// get current consumption average since last call
int Ferraris::get_W_average()
{
  // check for very low consumption (less than 2 revolutions per call)
  if (m_average_timestamp == m_timestampLast1) {
    return get_W();
  }
  if (m_average_timestamp == m_timestampLast2) {
    m_average_timestamp   = m_timestampLast1;
    m_average_revolutions = m_revolutions;
    return get_W();
  }

  // changes during last average cycle
  unsigned long elapsedT = m_timestampLast1 - m_average_timestamp;
    signed long elapsedR = m_revolutions    - m_average_revolutions;
  if ((elapsedT == 0) || (elapsedR == 0)) return 0;

  // update average state with last capture
  m_average_timestamp   = m_timestampLast1;
  m_average_revolutions = m_revolutions;

  // [60 min * 60 sec * 1000 ms] * [1 kW -> 1000 W] / [time for 1kWh]
  return 3600000000 * elapsedR / (elapsedT * m_config_rev_kWh);
}

float Ferraris::get_kW() const
{
  unsigned long elapsedtime = m_timestampLast1 - m_timestampLast2;  // last full cycle
  if (elapsedtime == 0) return 0.0f;
  unsigned long runningtime = millis() - m_timestampLast1;          // current open cycle
  // [60 min * 60 sec * 1000 ms] * [1 kW] / [time for 1kWh]
  return m_direction * 3600000.0f / (std::max(elapsedtime, runningtime) * m_config_rev_kWh);
}

float Ferraris::get_kWh() const
{
  return float(m_revolutions) / float(m_config_rev_kWh);
}


// ----------------------------------------------------------------------------
// simple getter and setter
// ----------------------------------------------------------------------------

unsigned long Ferraris::get_revolutions() const
{
  return m_revolutions;
}

void Ferraris::set_revolutions(unsigned long value)
{
  m_revolutions = value;
  m_changed = true;
}

unsigned long Ferraris::get_revolutionsRaw() const
{
  return m_revolutionsRaw;
}

void Ferraris::set_revolutionsRaw(unsigned long value)
{
  m_revolutionsRaw = value;
  m_changed = true;
}

unsigned long Ferraris::get_revolutions5() const
{
  return m_revolutions5;
}

void Ferraris::set_revolutions5(unsigned long value)
{
  m_revolutions5 = value;
  m_changed = true;
}

unsigned long Ferraris::get_revolutions10() const
{
  return m_revolutions10;
}

void Ferraris::set_revolutions10(unsigned long value)
{
  m_revolutions10 = value;
  m_changed = true;
}

unsigned int Ferraris::get_U_kWh() const
{
  return m_config_rev_kWh;
}

void Ferraris::set_U_kWh(unsigned int value)
{
  m_config_rev_kWh = value;
  m_changed = true;
}

unsigned int Ferraris::get_debounce() const
{
  return m_config_debounce;
}

void Ferraris::set_debounce(unsigned int value)
{
  m_config_debounce = value;
}

bool Ferraris::get_twoway() const
{
  return m_config_twoway;
}

void Ferraris::set_twoway(bool flag)
{
  m_config_twoway = flag;
}
