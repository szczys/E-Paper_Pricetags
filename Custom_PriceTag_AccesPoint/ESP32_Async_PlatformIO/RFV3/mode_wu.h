#pragma once
#include "RFV3.h"

class ModeWu : public mode_class
{
public:
  virtual void interrupt()
  {
    wakeup();
  }

  virtual void new_interval()
  {

    if (millis() - wakeup_start_time > 17000)
    {
      log_main("Something wrong, back to idle");
      set_mode_idle();
    }
  }

  virtual void pre()
  {
    log_main(mode_name);
    wakeup_start_time = millis();

    memset(tx_wu_buffer, 0xff, 10);
    tx_wu_buffer[0] = 0x00;
    tx_wu_buffer[7] = get_freq() + 1;
    tx_wu_buffer[8] = 0x01;
    tx_wu_buffer[9] = get_network_id();

    printf("Wu:");
    for (int i = 0; i < 9; i++)
    {
      printf(" 0x%02x", tx_wu_buffer[i]);
    }
    printf("\r\n");

    cc1101_prepaire_tx(get_wu_channel(), 0);
    wakeup();
    cc1101_tx();
  }

  virtual void main()
  {
  }

  virtual void post()
  {
  }

  virtual String get_name()
  {
    return mode_name;
  }

private:
  String mode_name = "Wakeup";

  long wakeup_start_time;
  uint8_t tx_wu_buffer[10];

  void wakeup()
  {
    if (millis() - wakeup_start_time > 16000)
    {
      log_main("WAKEUP done");
      set_mode_full_sync();
    }
    else
    {
      cc1101_tx_fill(tx_wu_buffer, 10);
    }
  }
};

ModeWu modeWu;
