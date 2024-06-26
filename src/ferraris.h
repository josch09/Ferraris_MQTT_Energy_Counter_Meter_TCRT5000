#ifndef FERRARIS_H
#define FERRARIS_H

#include <Arduino.h>


// how many Ferraris meters do we want
#define FERRARIS_NUM 2

// define logic level of pin input
#define FERRARIS_RED     HIGH
#define FERRARIS_SILVER  LOW


class Ferraris {
  public:
  /*
  * Used pins          NodeMCU V2+V3   Wemos D1    Wemos D1 mini
  * Internal LED       <D0> GPIO 16    (IO16)      (D0)
  * IR Pin Messure 1   (D1) GPIO 05    (IO5)       (D1)
  * IR Pin Messure 2   (D2) GPIO 04    (IO4)       (D2)
  * IR Pin Messure 3   (D3) GPIO 00    (IO0)       (D3)
  * ESP8266 LED        <D4> GPIO 02    <IO2>       <D4>
  * IR Pin Messure 4   (D5) GPIO 14    (IO14)      (D5)
  * IR Pin Messure 5   (D6) GPIO 12    (IO12)      (D6)
  * IR Pin Messure 6   (D7) GPIO 13    (IO13)      (D7)
  * IR Pin Messure 7   (D8) GPIO 15                (D8)
  */
    const uint8_t PINS[7]  = {D1, D2, D3, D5, D6, D7, D8};
    const uint8_t DPINS[7] = {D2, D1, D5, D3, D7, D8, D7};

  private: // singleton pattern: prevent access to constructors
    Ferraris();
    Ferraris(const Ferraris &);
    Ferraris &operator=(const Ferraris &);

  public:
    static Ferraris& getInstance(unsigned char F);

    void            begin();
    bool            loop();

    void            IRQhandler();

    bool            get_state() const;  // actual PIN state
    int             get_W() const;      // actual consumption in [W]
    float           get_kW() const;     // actual consumption in [kW]
    float           get_kWh() const;    // total reading of meter
    int             get_W_average();    // average consumption since last call in [W]

    // total revolution count
    unsigned long   get_revolutions() const;
    void            set_revolutions(unsigned long value);

    // total revolution count (raw)
    unsigned long   get_revolutionsRaw() const;
    void            set_revolutionsRaw(unsigned long value);
    // short revolution count (<5s interval)
    unsigned long   get_revolutions5() const;
    void            set_revolutions5(unsigned long value);
    // short revolution count (<10s interval)
    unsigned long   get_revolutions10() const;
    void            set_revolutions10(unsigned long value);

    // config: revolutions per kWh
    unsigned int    get_U_kWh() const;
    void            set_U_kWh(unsigned int value);

    // config: debounce time [ms]
    unsigned int    get_debounce() const;
    void            set_debounce(unsigned int value);

    // config: count mode, single / two way
    bool            get_twoway() const;
    void            set_twoway(bool flag);

    enum states {startup, silver_debounce, silver, red_debounce, red};

    // new Interval time [ms] returns true when suspicious revolution detected
    bool             setNewInterval(unsigned long value);

  private:
    uint8_t           m_PIN;
    uint8_t           m_DPIN;
    void              (*m_handler)();
    Ferraris::states  m_state;
    unsigned int      m_config_rev_kWh;   // revolutions per kWh
    unsigned int      m_config_debounce;  // debounce time [ms]
    bool              m_config_twoway;    // single or dual way counting

    unsigned long     m_timestamp;
    unsigned long     m_timestampLast1;
    unsigned long     m_timestampLast2;
    unsigned long     m_revolutions;      // total amount of revolutions (corrected filtered)
    unsigned long     m_revolutionsRaw;   // revolutions unfiltered
    unsigned long     m_revolutions5;     // revolutions count (less than 5sec per revolution)
    unsigned long     m_revolutions10;    // revolutions count (less than 10sec per revolution)

    unsigned long     m_interval1;        // suspicious interval in ms detection
    unsigned long     m_interval2;
    unsigned long     m_interval3;
    unsigned long     m_interval4;
    unsigned long     m_interval5;
    unsigned long     m_intervalTraining;
    bool              m_suspicious;       // suspicious interval detected

    bool              m_changed;          // something has changed -> give info in loop()
    short int         m_direction;        // direction of last count

    unsigned long     m_average_timestamp;      // last average call: timestampLast2
    unsigned long     m_average_revolutions;    // last average call: amount of revolutions

    static uint8_t    FINSTANCE; // used to identify instance
};

#if (FERRARIS_NUM < 1) || (FERRARIS_NUM > 7)
  #error "Maximum of 7 Farraris meters allowed"
#endif

#endif