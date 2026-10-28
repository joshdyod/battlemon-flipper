#include "battlemon_ir.h"
#include <furi.h>
#include <infrared.h>
#include <infrared/infrared_signal.h>
#include <infrared_worker.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Timing constants
// ---------------------------------------------------------------------------
#define CHALLENGE_TIMEOUT_MS  10000u
#define ACK_TIMEOUT_MS         2000u
#define PACKET_TIMEOUT_MS      5000u
#define PACKET_BYTE_GAP_MS       80u  // gap between sequential NEC transmissions

// ---------------------------------------------------------------------------
// TX: send one NEC command with our reserved address
// infrared_signal_transmit() blocks until done — safe to call from GUI thread.
// ---------------------------------------------------------------------------
static void ir_tx_cmd(uint32_t command) {
    InfraredMessage msg = {
        .protocol = InfraredProtocolNEC,
        .address  = IR_DIGIFLIP_ADDR,
        .command  = command,
        .repeat   = false,
    };
    InfraredSignal* sig = infrared_signal_alloc();
    infrared_signal_set_message(sig, &msg);
    if(infrared_signal_is_valid(sig)) {
        infrared_signal_transmit(sig);
    }
    infrared_signal_free(sig);
}

// ---------------------------------------------------------------------------
// RX: InfraredWorker callback — called from the InfraredWorker thread.
// Stores incoming messages into the context; ir_tick() reads them.
// ---------------------------------------------------------------------------
typedef struct {
    InfraredMessage buf[16];
    uint8_t         head;
    uint8_t         tail;
    FuriMutex*      mutex;
} RxQueue;

static RxQueue s_rxq;

static void rx_callback(void* ctx, InfraredWorkerSignal* signal) {
    UNUSED(ctx);
    if(!infrared_worker_signal_is_decoded(signal)) return;
    const InfraredMessage* msg = infrared_worker_get_decoded_signal(signal);
    if(!msg) return;
    // Accept challenge/ack (addr=0xD1) and packet bytes (addr=0xD1..0xD8)
    if(msg->address < IR_DIGIFLIP_ADDR || msg->address > IR_DIGIFLIP_ADDR + 8u) return;

    furi_mutex_acquire(s_rxq.mutex, FuriWaitForever);
    uint8_t next = (s_rxq.head + 1) & 15u;
    if(next != s_rxq.tail) {
        s_rxq.buf[s_rxq.head] = *msg;
        s_rxq.head = next;
    }
    furi_mutex_release(s_rxq.mutex);
}

// Drain one message from the queue. Returns false if empty.
static bool rx_pop(InfraredMessage* out) {
    furi_mutex_acquire(s_rxq.mutex, FuriWaitForever);
    bool got = (s_rxq.head != s_rxq.tail);
    if(got) {
        *out = s_rxq.buf[s_rxq.tail];
        s_rxq.tail = (s_rxq.tail + 1) & 15u;
    }
    furi_mutex_release(s_rxq.mutex);
    return got;
}

static InfraredWorker* s_worker = NULL;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ir_init(IrContext* ctx) {
    memset(ctx, 0, sizeof(IrContext));
    ctx->state = IR_STATE_IDLE;

    memset(&s_rxq, 0, sizeof(s_rxq));
    s_rxq.mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    s_worker = infrared_worker_alloc();
    infrared_worker_rx_set_received_signal_callback(s_worker, rx_callback, NULL);
    infrared_worker_rx_start(s_worker);
}

void ir_deinit(IrContext* ctx) {
    if(s_worker) {
        infrared_worker_rx_stop(s_worker);
        infrared_worker_free(s_worker);
        s_worker = NULL;
    }
    if(s_rxq.mutex) {
        furi_mutex_free(s_rxq.mutex);
        s_rxq.mutex = NULL;
    }
    ctx->state = IR_STATE_IDLE;
}

void ir_start_challenge(IrContext* ctx) {
    ctx->state         = IR_STATE_SENDING_CHALLENGE;
    ctx->packet_ready  = false;
    ctx->_bytes_received = 0;
    ctx->_deadline_tick = furi_get_tick() + furi_ms_to_ticks(CHALLENGE_TIMEOUT_MS);
}

IrState ir_tick(IrContext* ctx) {
    uint32_t now = furi_get_tick();
    InfraredMessage msg;

    switch(ctx->state) {

        case IR_STATE_SENDING_CHALLENGE:
            // Broadcast challenge, then check for incoming messages
            ir_tx_cmd(IR_CMD_CHALLENGE);

            while(rx_pop(&msg)) {
                if(msg.command == IR_CMD_CHALLENGE) {
                    // Peer also broadcasting — we reply with ACK and start packet send
                    ir_tx_cmd(IR_CMD_ACK);
                    ctx->state        = IR_STATE_SENDING_PACKET;
                    ctx->_deadline_tick = now + furi_ms_to_ticks(PACKET_TIMEOUT_MS);
                    return ctx->state;
                }
                if(msg.command == IR_CMD_ACK) {
                    // Peer already sent ACK to our earlier challenge
                    ctx->state        = IR_STATE_SENDING_PACKET;
                    ctx->_deadline_tick = now + furi_ms_to_ticks(PACKET_TIMEOUT_MS);
                    return ctx->state;
                }
            }

            if(now > ctx->_deadline_tick) ctx->state = IR_STATE_ERROR_TIMEOUT;
            break;

        case IR_STATE_WAITING_ACK:
            while(rx_pop(&msg)) {
                if(msg.command == IR_CMD_ACK) {
                    ctx->state        = IR_STATE_SENDING_PACKET;
                    ctx->_deadline_tick = now + furi_ms_to_ticks(PACKET_TIMEOUT_MS);
                    return ctx->state;
                }
            }
            if(now > ctx->_deadline_tick) ctx->state = IR_STATE_ERROR_TIMEOUT;
            break;

        case IR_STATE_WAITING_PACKET:
            // Drain all received messages and assemble the packet.
            // TX sends address = IR_DIGIFLIP_ADDR + byte_index, command = byte_value
            while(rx_pop(&msg)) {
                if(msg.address < IR_DIGIFLIP_ADDR || msg.address > IR_DIGIFLIP_ADDR + 7u) continue;
                uint8_t idx = (uint8_t)(msg.address - IR_DIGIFLIP_ADDR);
                ctx->_rx_buf[idx] = (uint8_t)msg.command;
                ctx->_bytes_received |= (uint8_t)(1u << idx);
            }
            // All 8 bytes received?
            if(ctx->_bytes_received == 0xFFu) {
                memcpy(&ctx->received_packet, ctx->_rx_buf, sizeof(BMPacket));
                if(packet_is_valid(&ctx->received_packet)) {
                    ctx->packet_ready = true;
                    ctx->state        = IR_STATE_DONE;
                } else {
                    ctx->state = IR_STATE_ERROR_CHECKSUM;
                }
            } else if(now > ctx->_deadline_tick) {
                ctx->state = IR_STATE_ERROR_TIMEOUT;
            }
            break;

        default:
            break;
    }

    return ctx->state;
}

void ir_send_packet(IrContext* ctx, const BMPacket* pkt) {
    // Send each byte as a sequential NEC command
    // command = IR_CMD_PKT_BASE + byte_index (so 0x00..0x07)
    // The receiver uses the command value as both the index and the data.
    // NOTE: This encodes the *index* as the command, not the actual byte value.
    // Both devices know which byte is which by position, not by content —
    // the actual payload is the received_packet assembled by index.
    // To transmit the actual byte values we use the command field to carry
    // the byte: command = (byte_index << 8) | byte_value — but NEC only gives
    // us 8 bits of command. So we send 8 separate NEC messages, one per byte,
    // using a unique command per message that encodes both index and value.
    // Simple approach: use address field for index, command field for value.
    // InfraredMessage.address = IR_DIGIFLIP_ADDR + byte_index (0xD1..0xD8)
    const uint8_t* bytes = (const uint8_t*)pkt;
    for(uint8_t i = 0; i < (uint8_t)sizeof(BMPacket); i++) {
        InfraredMessage msg = {
            .protocol = InfraredProtocolNEC,
            .address  = IR_DIGIFLIP_ADDR + i,  // 0xD1 + byte index
            .command  = bytes[i],               // actual byte value
            .repeat   = false,
        };
        InfraredSignal* sig = infrared_signal_alloc();
        infrared_signal_set_message(sig, &msg);
        if(infrared_signal_is_valid(sig)) {
            infrared_signal_transmit(sig);
        }
        infrared_signal_free(sig);
        furi_delay_ms(PACKET_BYTE_GAP_MS);
    }

    ctx->state           = IR_STATE_WAITING_PACKET;
    ctx->_bytes_received = 0;
    ctx->_deadline_tick  = furi_get_tick() + furi_ms_to_ticks(PACKET_TIMEOUT_MS);
}

const char* ir_state_label(IrState state) {
    switch(state) {
        case IR_STATE_IDLE:              return "Ready";
        case IR_STATE_SENDING_CHALLENGE: return "Searching...";
        case IR_STATE_WAITING_ACK:       return "Waiting...";
        case IR_STATE_SENDING_PACKET:    return "Connecting!";
        case IR_STATE_WAITING_PACKET:    return "Receiving...";
        case IR_STATE_DONE:              return "Fight!";
        case IR_STATE_ERROR_TIMEOUT:     return "Timed out";
        case IR_STATE_ERROR_CHECKSUM:    return "Corrupted";
        default:                         return "...";
    }
}
