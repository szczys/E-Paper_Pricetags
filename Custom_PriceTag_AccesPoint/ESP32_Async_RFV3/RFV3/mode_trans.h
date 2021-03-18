#pragma once
#include "RFV3.h"

class ModeTrans : public mode_class
{
  public:
    virtual void interrupt()
    {
      switch (tx_state) {
        case 1:
          set_fifo_interrupt(6, 7);
          tx_state = 2;
          break;
        case 2:
          cc1101_idle();
          set_fifo_interrupt(6, 7);
          cc1101_rx();
          tx_state = 3;
          rx_start_time = millis();
          break;
        case 3:
          tx_state = 4;
          break;
        case 40:
          tx_state = 1;
          tx_data_main();
          break;
        case 50:
          set_fifo_interrupt(6, 7);
          tx_state = 51;
          break;
        case 51:
          set_is_data_waiting(false);
          set_fifo_interrupt(6, 7);
          tx_state = 52;
          set_buffer_length_answer(length_to_receive);
          copy_buffer_answer(data_to_receive, length_to_receive);

          char str[3 * length_to_receive];
          tohex(data_to_receive, length_to_receive, str, 3 * length_to_receive);
          String data_received = "NetID: ";
          data_received += String(get_network_id());
          data_received += " freq: ";
          data_received += String(get_freq());
          data_received += " display: ";
          data_received += String(get_display_id());
          data_received += " ";
          data_received += String(millis());
          data_received += " = ";
          data_received += str;
          data_received += "\r\n";

          Serial.print("Received data packet:");
          Serial.println(data_received);
          appendFile("/answers.txt", data_received);


          if (get_trans_mode()) {
            set_trans_mode(0);
            restore_current_settings();
            set_mode_wu();
          } else {

            set_mode_idle();
          }
          break;
      }
    }

    virtual void new_interval()
    {
      log_main("to long, back to idle");
      set_mode_idle();
    }

    virtual void pre()
    {

      static int count = 2;
      log_main(mode_name);
      if (get_last_to_short()) {
        set_last_to_short(false);
        log_main("Last one was to short so continue");
      } else {
        if (get_trans_mode()) {
          id_offset = 4;
          max_cur_packet_len = 56;
        } else {
          id_offset = 0;
          max_cur_packet_len = 57;
        }
        packet_counter = 0;
        packet_counter_rx = 0;
        last_packet = false;
        is_first = true;
        multi_tx = false;
        curr_data_position = 0;
        length_to_receive = 0;
        length_to_send = 0;
        still_to_send = 0;
        number_of_con_data = 0;
        tx_state = 0;
        memset (data_to_send, 0x00, 0x4000);
        memset (data_to_receive, 0x00, 0x1000);

        length_to_send = get_buffer_length();
        copy_buffer(data_to_send, length_to_send);

      }

    }

    virtual void main()
    {
      switch (tx_state) {
        case 0:
          time_left = 1100 - (millis() - get_last_slot_time());
          log_main("TIME LEFT: " + String(time_left));
          if (time_left < 150) {
            set_last_to_short(true);
            set_mode_idle();
          } else {
            tx_data_main();
            if (get_rx_enable()) {
              tx_state = 1;
            }
            else {
              if (last_packet)tx_state = 50;
            }
          }
          break;
        case 3:
          if (millis() - rx_start_time >= get_rx_timeout()) {
            log_main("RX_TIMEOUT!!!");
            set_fifo_interrupt(6, 7);
            set_mode_idle();
          }
          break;
        case 4:
          read_data_cc1101();
          tx_state = 0;
          break;
      }
    }

    virtual String get_name()
    {
      return mode_name;
    }

  private:
    String mode_name = "Transmission";

    int time_left;

    uint8_t data_to_send[0x4000];
    uint8_t data_to_receive[0x1000];
    int length_to_send = 0;
    int length_to_receive = 0;

    int curr_data_position = 0;
    bool is_first = true;
    int still_to_send = 0;

    bool display_more_data = false;
    bool display_more_than_one = false;

    int number_of_con_data = 0;

    volatile int tx_state = 0;
    volatile long rx_start_time;

    bool multi_tx = false;
    bool last_packet = false;
    uint8_t packet_counter = 0;
    uint8_t packet_counter_rx = 0;

    uint8_t tx_data_buffer_int[62];
    bool next_rx_enable = false;

    int max_cur_packet_len = 57;// Real data is shorter if new Activation is used
    int id_offset = 0;/*When using the new Activation the ID = Serial so 6 instead of 2 bytes*/

    void tx_data_main() {
      log_normal("TX data main");
      int curr_packet_len = 0;

      memset (tx_data_buffer_int, 0x00, 62);
      tx_data_buffer_int[0] = get_network_id();

      if (id_offset > 0) {
        uint8_t serial[7];
        get_serial(serial);
        memcpy(&tx_data_buffer_int[1], serial, 6);
        /* Serial from 1 - 6 */
      } else {
        tx_data_buffer_int[1] = get_display_id() >> 8;
        tx_data_buffer_int[2] = get_display_id() & 0xff;
      }
      set_rx_enable(true);
      still_to_send = length_to_send - curr_data_position;

      if (still_to_send > 0) {
        if (is_first) {

          tx_data_buffer_int[id_offset + 3] = packet_counter;
          Serial.println("Is first");
          is_first = false;
          number_of_con_data = still_to_send / (max_cur_packet_len - id_offset);
          if (number_of_con_data > 7)number_of_con_data = 7;
          Serial.println("Cont data: " + String(number_of_con_data));
          Serial.println("Still to send: " + String(still_to_send));

        } else {

          tx_data_buffer_int[id_offset + 3] = packet_counter;
          Serial.println("Is continiuos");
          set_rx_enable(false);
          if (number_of_con_data == 0) {
            is_first = true;
            set_rx_enable(true);
            packet_counter = packet_counter + 1;
            packet_counter &= 0x0F;
          }

        }

        /*Copy the actual data into the TX buffer*/
        if (still_to_send > (max_cur_packet_len - id_offset)) {
          curr_packet_len = (max_cur_packet_len - id_offset);
        } else {
          tx_data_buffer_int[id_offset + 3] |= 0x10;
          curr_packet_len = still_to_send;
        }
        memcpy(&tx_data_buffer_int[id_offset + 5], &data_to_send[curr_data_position], curr_packet_len);
        curr_data_position += curr_packet_len;

        /*END Copy the actual data into the TX buffer*/
      } else {
        Serial.println("Is only rx");
        tx_data_buffer_int[id_offset + 3] = 0x80 | packet_counter_rx;
        if (display_more_data) {
          if (display_more_than_one) { //Display want antother part of current data
            display_more_than_one = false;
            number_of_con_data = 0x01;
            tx_data_buffer_int[id_offset + 5] = 0x2B;
            packet_counter_rx = packet_counter_rx + 1;
            packet_counter_rx &= 0x0F;
          } else { //display wants to send more data
            tx_data_buffer_int[id_offset + 3] |= 0x10;
            display_more_data = false;
          }
        } else { //Last message sending now
          tx_data_buffer_int[id_offset + 5] = 0x2B;
          last_packet = true;
          set_rx_enable(false);
        }
      }
      tx_data_buffer_int[id_offset + 4] = number_of_con_data;

      if (!multi_tx)cc1101_tx();
      send_radio_tx_burst(tx_data_buffer_int, (id_offset > 0) ? 61 : 62); //New activation is one byte shorter
      if (!multi_tx)set_fifo_interrupt(2, 15);

      Serial.print("PacketCounter: " + String(packet_counter));

      Serial.print(" rx: " + String(packet_counter_rx));
      if (number_of_con_data) {
        set_rx_enable(false);
        tx_state = 40;
        multi_tx = true;
        number_of_con_data--;
        packet_counter = packet_counter + 1;
        packet_counter &= 0x0F;
      } else {
        multi_tx = false;
      }

      Serial.print(" Data to send: ");
      for (int i = 0; i < (id_offset + 6 + curr_packet_len); i++) {
        Serial.print(" 0x");
        Serial.print(tx_data_buffer_int[i], HEX);
      }
      Serial.println();

    }

    void set_rx_enable(bool state) {
      next_rx_enable = state;
    }

    bool get_rx_enable() {
      bool temp_rx = next_rx_enable;
      next_rx_enable = false;
      return temp_rx;
    }

    void read_data_cc1101() {

      uint8_t read_length = (id_offset > 0) ? 0x0F : 0x0D;
      uint8_t read_first = spi_read_register(0xFB);
      uint8_t read_length_in = spi_read_register(0xFF);//Would be the length as well but could be a wrong reading so better have it fixed

      uint8_t data_array[read_length + 1];
      spi_read_burst(0xFF, data_array, read_length);
      uint8_t data_array1[2];
      spi_read_burst(0xFF, data_array1, 0x02);

      Serial.print("Read_data: 0x");
      Serial.print(read_first, HEX);
      Serial.print(" 0x");
      Serial.print(read_length_in, HEX);

      Serial.print(" Data:");
      for (int i = 0; i < read_length; i++) {
        Serial.print(" 0x");
        Serial.print(data_array[i], HEX);
      }
      Serial.print(" Status:");
      for (int i = 0; i < 0x02; i++) {
        Serial.print(" 0x");
        Serial.print(data_array1[i], HEX);
      }
      Serial.println();

      if (!(data_array[3 + id_offset] & 0x80)) { //New actual data was received, adding it to the answer buffer.
        memcpy(&data_to_receive[length_to_receive], &data_array[8 + ((id_offset > 0) ? 2 : 0)], 5);
        length_to_receive += 5;
      }
      display_more_data = !(data_array[3 + id_offset] & 0x10); //Display wants more communication
      display_more_than_one = !(data_array[3 + id_offset] & 0xf0); //Display wants more than one packet
    }

};

ModeTrans modeTrans;