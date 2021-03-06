#pragma once

#include "Time/RealTimer.hpp"
#include "Time/UnitLiterals.hpp"
#include "Ack.hpp"
#include "Packet.hpp"
#include "RFM12.hpp"
#include "Strings.hpp"

namespace HopeRF {

using namespace Time;

typedef STRB(1,2,3,5,8,13,21,34,55,89,144) ResendDelays;

template <typename rfm_t, typename rt_t, typename T>
class TxState {
    rfm_t *const rfm;
    rt_t *const rt;
    const uint16_t nodeId;
    const uint8_t resendOffset; // Fixed offset to add to resend delay, depending on nodeId, to combat interference from several nodes

    T state;
    uint8_t seq = 0;
    bool tx = false;
    VariableDeadline<rt_t> resend = { *rt };
    uint8_t resendCount = 0;

    void send() {
        Packet<T> packet = { seq, nodeId, state };
        rfm->write_fsk(Headers::TXSTATE, &packet);
        tx = true;
        resend.schedule(10_ms * uint8_t(ResendDelays::charAt(resendCount) + resendOffset));
    }
public:
    TxState(rfm_t &r, rt_t &t, T initial, uint16_t _nodeId):
        rfm(&r), rt(&t), state(initial), nodeId(_nodeId),
        resendOffset(((_nodeId) ^ (_nodeId >> 4) ^ (_nodeId >> 8) ^ (_nodeId >> 12)) & 0x000F) {
        send();
    }

    void setState(T t) {
        if (t != state) {
            seq++;
            state = t;
            resendCount = 0;
            send();
        }
    }

    void loop() {
        if (readAck(rfm->in(), seq, nodeId)) {
            tx = false;
            resend.cancel();
            resendCount = 0;
        } else if (tx && resend.isNow()) {
            if (resendCount < ResendDelays::size() - 1) {
                resendCount++;
            }
            send();
        }
    }
};

}


