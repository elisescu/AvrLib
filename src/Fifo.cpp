#include "Fifo.hpp"

const Writer::VTable AbstractFifo::writerVTable = {
        &AbstractFifo::writeStart,
        &AbstractFifo::writeEnd,
        &AbstractFifo::write };

void AbstractFifo::writeStart(void *delegate) {
    ((AbstractFifo*)delegate)->markWrite();
}

void AbstractFifo::writeEnd(void *delegate) {
    ((AbstractFifo*)delegate)->commitWrite();
}

bool AbstractFifo::write(void *delegate, uint8_t b) {
    return ((AbstractFifo*)delegate)->append(b);
}

uint8_t AbstractFifo::getSize() const {
    AtomicScope _;
    return (markedEnd() > markedHead()) ? markedEnd() - markedHead() :
           (markedEnd() < markedHead()) ? capacity - markedHead() + markedEnd() :
           0;
}

bool AbstractFifo::append(uint8_t b) {
    AtomicScope _;

    if (hasSpace()) {
        if (end < capacity) {
            buffer[end] = b;
            end++;
        } else {
            buffer[0] = b;
            end = 1;
        }
        return true;
    } else {
        return false;
    }
}

bool AbstractFifo::remove(uint8_t &b) {
    AtomicScope _;

    if (hasContent()) {
        b = buffer[head];
        if (head < capacity - 1) {
            head++;
        } else {
            head = 0;
            if (end == capacity) {
                if (readMark == NO_MARK) {
                    end = 0;
                } else {
                    endAfterReadReset = true;
                }
            }
        }
        return true;
    } else {
        return false;
    }
}

void AbstractFifo::clear() {
    head = 0;
    end = 0;
    writeMark = NO_MARK;
    readMark = NO_MARK;
    endAfterReadReset = false;
}

Writer AbstractFifo::out() {
    return Writer(&writerVTable, this);
}
