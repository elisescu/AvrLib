#include "Fifo.hpp"

const Writer::VTable AbstractFifo::writerVTable = {
        &AbstractFifo::writeStart,
        &AbstractFifo::writeCommit,
        &AbstractFifo::writeRollback,
        &AbstractFifo::write,
        &AbstractFifo::isWriting };

const Reader::VTable AbstractFifo::readerVTable = {
        &AbstractFifo::readStart,
        &AbstractFifo::readCommit,
        &AbstractFifo::readRollback,
        &AbstractFifo::readByte,
        &AbstractFifo::getRemaining,
        &AbstractFifo::isReading
};

void AbstractFifo::writeStart(void *delegate) {
    ((AbstractFifo*)delegate)->markWrite();
}

void AbstractFifo::writeCommit(void *delegate) {
    ((AbstractFifo*)delegate)->commitWrite();
}

void AbstractFifo::writeRollback(void *delegate) {
    ((AbstractFifo*)delegate)->resetWrite();
}

bool AbstractFifo::write(void *delegate, uint8_t b) {
    return ((AbstractFifo*)delegate)->append(b);
}

bool AbstractFifo::isWriting(void *delegate) {
    return ((AbstractFifo*)delegate)->isWriteMarked();
}

void AbstractFifo::readStart(void *delegate) {
    ((AbstractFifo*)delegate)->markRead();
}

void AbstractFifo::readCommit(void *delegate) {
    ((AbstractFifo*)delegate)->commitRead();
}

void AbstractFifo::readRollback(void *delegate) {
    ((AbstractFifo*)delegate)->resetRead();
}

bool AbstractFifo::readByte(void *delegate, uint8_t &target) {
    return ((AbstractFifo*)delegate)->read(target);
}

uint8_t AbstractFifo::getRemaining(void *delegate) {
    return ((AbstractFifo*)delegate)->getSize();
}

bool AbstractFifo::isReading(void *delegate) {
    return ((AbstractFifo*)delegate)->isReading();
}
/*
uint8_t AbstractFifo::markedOrWritePos() const {
    return (writeMark == NO_MARK) ? writePos : writeMark;
}

uint8_t AbstractFifo::markedOrReadPos() const {
    return (readMark == NO_MARK) ? readPos : readMark;
}
*/
uint8_t AbstractFifo::getSize() const {
    AtomicScope _;
    return (markedOrWritePos() > markedOrReadPos()) ? markedOrWritePos() - markedOrReadPos() :
           (markedOrWritePos() < markedOrReadPos()) ? bufferSize - markedOrReadPos() + markedOrWritePos() :
           0;
}

bool AbstractFifo::append(uint8_t b) {
    AtomicScope _;

    if (hasSpace()) {
        buffer[writePos] = b;
        writePos++;
        if (writePos >= bufferSize) {
            writePos -= bufferSize;
        }
        return true;
    } else {
        return false;
    }
}

bool AbstractFifo::reserve(volatile uint8_t * &ptr) {
    AtomicScope _;
    if (isWriteMarked() && hasSpace()) {
        ptr = buffer + writePos;
        writePos++;
        if (writePos >= bufferSize) {
            writePos -= bufferSize;
        }
        return true;
    } else {
        return false;
    }
}

bool AbstractFifo::read(uint8_t &b) {
    AtomicScope _;

    if (hasContent()) {
        b = buffer[readPos];
        readPos++;
        if (readPos >= bufferSize) {
            readPos -= bufferSize;
        }
        return true;
    } else {
        return false;
    }
}

void AbstractFifo::clear() {
    readPos = 0;
    writePos = 0;
    writeMark = NO_MARK;
    readMark = NO_MARK;
}

Writer AbstractFifo::out() {
    return Writer(&writerVTable, this);
}

Reader AbstractFifo::in() {
    return Reader(&readerVTable, this);
}

bool AbstractFifo::isEmpty() const {
    AtomicScope _;
    return markedOrWritePos() == readPos;
}

bool AbstractFifo::hasContent() const {
    AtomicScope _;
    return markedOrWritePos() != readPos;
}

bool AbstractFifo::isFull() const {
    AtomicScope _;
    return writePos == (( markedOrReadPos() - 1 + bufferSize) % bufferSize);
}

uint8_t AbstractFifo::peek() const {
    AtomicScope _;
    if (hasContent()) {
        return buffer[readPos];
    } else {
        return 0;
    }
}

void AbstractFifo::resetWrite() {
    AtomicScope _;

    if (writeMark != NO_MARK) {
        writePos = writeMark;
        writeMark = NO_MARK;
    }
}

void AbstractFifo::resetRead() {
    AtomicScope _;

    if (readMark != NO_MARK) {
        readPos = readMark;
        readMark = NO_MARK;
    }
}


