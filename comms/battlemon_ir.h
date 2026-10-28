#pragma once
#include "../game/battlemon_creature.h"
#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// IR battle protocol
//
// Uses the Flipper infrared library (infrared_signal_transmit / InfraredWorker)
// rather than the low-level furi_hal_infrared HAL callbacks, which are internal
// to the infrared library and not intended for direct app use.
//
// Encoding: NEC protocol with a reserved address (0xD1 = BattleMon).
// Challenge = command 0xAC, Packet bytes 0..7 = commands 0x00..0x07.
//
// Handshake:
//   1. Both devices broadcast challenge signal (NEC addr=0xD1 cmd=0xAC)
//   2. First device to receive challenge ACKs and enters packet exchange
//   3. 8 packet bytes sent sequentially as NEC commands (addr=0xD1, cmd=byte)
//   4. Both run battle_resolve() locally — no further comms needed
// ---------------------------------------------------------------------------

#define IR_DIGIFLIP_ADDR  0xD1u   // Reserved address for BattleMon
#define IR_CMD_CHALLENGE  0xACu   // Challenge magic
#define IR_CMD_ACK        0xCBu   // ACK magic
#define IR_CMD_PKT_BASE   0x00u   // Packet byte N sent as cmd = (PKT_BASE + N)

typedef enum {
    IR_STATE_IDLE,
    IR_STATE_SENDING_CHALLENGE,
    IR_STATE_WAITING_ACK,
    IR_STATE_SENDING_PACKET,
    IR_STATE_WAITING_PACKET,
    IR_STATE_DONE,
    IR_STATE_ERROR_TIMEOUT,
    IR_STATE_ERROR_CHECKSUM,
} IrState;

typedef struct {
    IrState     state;
    BMPacket  received_packet;
    bool        packet_ready;
    uint32_t    _deadline_tick;   // furi_get_tick() deadline for current state
    uint8_t     _bytes_received;  // how many packet bytes arrived so far
    uint8_t     _rx_buf[8];       // raw received packet bytes
} IrContext;

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
void        ir_init(IrContext* ctx);
void        ir_deinit(IrContext* ctx);
void        ir_start_challenge(IrContext* ctx);
IrState     ir_tick(IrContext* ctx);
void        ir_send_packet(IrContext* ctx, const BMPacket* pkt);
const char* ir_state_label(IrState state);
