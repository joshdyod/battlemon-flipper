#include "battlemon_ir.h"
#include <furi.h>
#include <infrared_worker.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Timing constants
// ---------------------------------------------------------------------------
#define CHALLENGE_TIMEOUT_MS  10000u
#define PACKET_TIMEOUT_MS      5000u
#define PACKET_BYTE_GAP_MS       80u

// ---------------------------------------------------------------------------
// TX: send one NEC message using InfraredWorker
// infrared_worker_set_decoded_signal sets the message, then
// infrared_worker_tx_get_signal_steady_callback is used as the signal
// provider so the worker sends it once and stops.
// This call blocks until transmission completes (~70ms for NEC).
// ---------------------------------------------------------------------------
static void ir_tx_msg(InfraredWorker* worker, uint32_t address, uint32_t command) {
    InfraredMessage msg = {
        .protocol = InfraredProtocolNEC,
        .address  = address,
        .command  = command,
        .repeat   = false,
    };
    infrared_worker_set_decoded_signal(worker, &msg);
    infrared_worker_tx_set_get_signal_callback(
        worker, infrared_worker_tx_get_signal_steady_callback, worker);
    infrared_worker_tx_start(worker);
    furi_delay_ms(70);   // NEC frame is ~67ms; wait for it to finish
    infrared_worker_tx_stop(worker);
}

// ---------------------------------------------------------------------------
// RX callback — runs on InfraredWorker thread; queues decoded messages
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
    // Accept our reserved address range: 0xD1 (challenge/ack) and 0xD1..0xD8 (packet bytes)
    if(msg->address < IR_DIGIFLIP_ADDR || msg->address > IR_DIGIFLIP_ADDR + 8u) return;

    furi_mutex_acquire(s_rxq.mutex, FuriWaitForever);
    uint8_t next = (s_rxq.head + 1) & 15u;
    if(next != s_rxq.tail) {
        s_rxq.buf[s_rxq.head] = *msg;
        s_rxq.head = next;
    }
    furi_mutex_release(s_rxq.mutex);
}

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
    infrared_worker_rx_enable_signal_decoding(s_worker, true);
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
    ctx->state           = IR_STATE_SENDING_CHALLENGE;
    ctx->packet_ready    = false;
    ctx->_bytes_received = 0;
    ctx->_deadline_tick  = furi_get_tick() + furi_ms_to_ticks(CHALLENGE_TIMEOUT_MS);
}

IrState ir_tick(IrContext* ctx) {
    uint32_t now = furi_get_tick();
    InfraredMessage msg;

    switch(ctx->state) {

        case IR_STATE_SENDING_CHALLENGE:
            // Stop RX briefly, transmit challenge, restart RX
            infrared_worker_rx_stop(s_worker);
            ir_tx_msg(s_worker, IR_DIGIFLIP_ADDR, IR_CMD_CHALLENGE);
            infrared_worker_rx_start(s_worker);

            while(rx_pop(&msg)) {
                if(msg.address == IR_DIGIFLIP_ADDR &&
                   msg.command == IR_CMD_CHALLENGE) {
                    // Peer also challenging — ACK them
                    infrared_worker_rx_stop(s_worker);
                    ir_tx_msg(s_worker, IR_DIGIFLIP_ADDR, IR_CMD_ACK);
                    infrared_worker_rx_start(s_worker);
                    ctx->state        = IR_STATE_SENDING_PACKET;
                    ctx->_deadline_tick = now + furi_ms_to_ticks(PACKET_TIMEOUT_MS);
                    return ctx->state;
                }
                if(msg.address == IR_DIGIFLIP_ADDR &&
                   msg.command == IR_CMD_ACK) {
                    ctx->state        = IR_STATE_SENDING_PACKET;
                    ctx->_deadline_tick = now + furi_ms_to_ticks(PACKET_TIMEOUT_MS);
                    return ctx->state;
                }
            }
            if(now > ctx->_deadline_tick) ctx->state = IR_STATE_ERROR_TIMEOUT;
            break;

        case IR_STATE_WAITING_PACKET:
            while(rx_pop(&msg)) {
                if(msg.address < IR_DIGIFLIP_ADDR + 1u ||
                   msg.address > IR_DIGIFLIP_ADDR + 8u) continue;
                uint8_t idx = (uint8_t)(msg.address - IR_DIGIFLIP_ADDR - 1u);
                if(idx < (uint8_t)sizeof(BMPacket)) {
                    ctx->_rx_buf[idx] = (uint8_t)msg.command;
                    ctx->_bytes_received |= (uint8_t)(1u << idx);
                }
            }
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
    // Stop RX, send 8 bytes sequentially, restart RX
    infrared_worker_rx_stop(s_worker);

    const uint8_t* bytes = (const uint8_t*)pkt;
    for(uint8_t i = 0; i < (uint8_t)sizeof(BMPacket); i++) {
        // address = IR_DIGIFLIP_ADDR + 1 + byte_index (0xD2..0xD9)
        // command = actual byte value
        ir_tx_msg(s_worker, IR_DIGIFLIP_ADDR + 1u + i, bytes[i]);
        furi_delay_ms(PACKET_BYTE_GAP_MS);
    }

    infrared_worker_rx_start(s_worker);
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
