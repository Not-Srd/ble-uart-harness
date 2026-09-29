#include "uart_proto.h"
#include "crc8.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#define FRAME_SOF 0xA5
#define MAX_PAYLOAD_LEN 32

/*CMDS*/
#define CMD_PING 0x01
#define CMD_START_ADV 0x02
#define CMD_STOP_ADV 0x03
#define CMD_DISCONNECT 0x04
#define CMD_START_STREAM 0x05
#define CMD_STOP_STREAM 0x06
#define CMD_GET_STATS 0x07

/*REPLIES*/
#define REP_ACK 0x80
#define REP_NACK 0x81
#define REP_EVT 0x82
#define CMD_UNKNOWN 0xFF


/*ERROR CODES*/
#define ERR_BAD_CRC 0x01
#define ERR_BAD_LEN 0x02
#define ERR_UNKNOWN_CMD 0x03
#define ERR_BAD_STATE 0x04
#define ERR_BAD_PARAM 0x05
#define ERR_TIMEOUT 0x06

/*Parsing States*/
typedef enum {
  HUNT_SOF,
  GET_LEN,
  GET_CMD,
  GET_PAYLOAD,
  GET_CRC
} Parsing_States;



static void send_frame(uint8_t cmd, const uint8_t payload[], uint8_t len) {

  if (len > MAX_PAYLOAD_LEN) { 
    return; 
  }

  uint8_t frame[MAX_PAYLOAD_LEN + 4];
  
  frame[0] = FRAME_SOF;
  frame[1] = len;
  frame[2] = cmd;
  memcpy(&frame[3], payload, len);
  frame[3+len] = crc8(&frame[1], len+2);

  uart_write_bytes(UART_NUM_0, frame, len + 4);

}


static void handle_frame(uint8_t cmd, const uint8_t *payload, uint8_t len){
  switch (cmd) {
    case CMD_PING: 
    {
      if (len != 0) {
        uint8_t temp[] = {cmd, ERR_BAD_PARAM};
        send_frame(REP_NACK, temp, sizeof(temp));
      } else {
        uint8_t temp[] = {cmd};
        send_frame(REP_ACK, temp, sizeof(temp));
      }
      break;
    }
    default: 
    {
      uint8_t temp[] = {cmd, ERR_UNKNOWN_CMD};
      send_frame(REP_NACK, temp, sizeof(temp));
      break;
    }
  }
}


static void uart_rx_task(void *arg) {
  
  Parsing_States current_state = HUNT_SOF;
  uint8_t frame[2+MAX_PAYLOAD_LEN];
  size_t countof_payload_bytes = 0;
  
  for (;;) {

    uint8_t b;
    int n = uart_read_bytes(UART_NUM_0, &b, 1, pdMS_TO_TICKS(50));

    if (n == 1) {
      switch (current_state) {

        case HUNT_SOF: 
          current_state = (b == FRAME_SOF)? GET_LEN:HUNT_SOF;
          countof_payload_bytes = 0;
          break;

        case GET_LEN: {
          if (b > MAX_PAYLOAD_LEN) {
            uint8_t temp[] = {CMD_UNKNOWN, ERR_BAD_LEN};
            send_frame(REP_NACK, temp, sizeof(temp));
            current_state = (b == FRAME_SOF)? GET_LEN:HUNT_SOF;
            break;
          } else {
            frame[0] = b;
            current_state = GET_CMD;
            break;
          }
        }

        case GET_CMD: {
          frame[1] = b;
          current_state = (frame[0] == 0)? GET_CRC:GET_PAYLOAD;
          break;
        }

        case GET_PAYLOAD: {          
          frame[2+countof_payload_bytes++] = b;
          current_state = (countof_payload_bytes < frame[0])? GET_PAYLOAD:GET_CRC;
          break;
        }

        case GET_CRC: {
          if (b == crc8(frame, 2+countof_payload_bytes)) {
            handle_frame(frame[1], &frame[2], countof_payload_bytes);
          } else {
            uint8_t temp[] = {CMD_UNKNOWN, ERR_BAD_CRC};
            send_frame(REP_NACK, temp, sizeof(temp));
          }
          current_state = HUNT_SOF;
          break;
        }
      }
    } else {

      if (current_state != HUNT_SOF) {
        uint8_t temp[] = {CMD_UNKNOWN, ERR_TIMEOUT};
        send_frame(REP_NACK, temp, sizeof(temp));
        current_state = HUNT_SOF;
      }

    }
  }
}

void uart_proto_init() {
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0));
  xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 5, NULL);
}