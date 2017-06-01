#ifndef HAL_ATMEL_POWER_HPP_
#define HAL_ATMEL_POWER_HPP_

#include "SleepMode.hpp"
#include "HAL/Atmel/InterruptHandlers.hpp"
#include "AtomicScope.hpp"
#include "HAL/Atmel/Device.hpp"
#include "Time/Units.hpp"
#include "Tasks/TaskState.hpp"
#include "Logging.hpp"
#include "HAL/attributes.hpp"

namespace HAL {
namespace Atmel {

using namespace Time;
using namespace InterruptHandlers;

/**
 * Returns whether s1 indicates a deeper sleep than s2.
 */
inline bool operator > (SleepMode s1, SleepMode s2) {
	return ((uint8_t)s1) > ((uint8_t)s2);
}

enum class SleepGranularity: uint8_t {
    _16ms = 0,
    _32ms = 1,
    _64ms = 2,
    _125ms = 3,
    _250ms = 4,
    _500ms = 5,
    _1000ms = 6,
    _2000ms = 7,
    _4000ms = 8,
    _8000ms = 9
};

namespace Impl {

INLINE static void sleep_bod_disable() {
    uint8_t tmp = MCUCR;
    tmp |= (BODS | BODSE);
    MCUCR = tmp;
    tmp &= ~BODSE;
    MCUCR = tmp;
}

INLINE static void sleep_cpu() {
    do {
        __asm__ __volatile__ ( "sleep" "\n\t" :: );
    } while(0);
}

/**
 * Powers down (sleeps) into the given sleep mode, until a hardware interrupt wakes the microcontroller up again.
 * Which hardware interrupts are available, and how long it takes to wake up, depends on the sleep mode.
 */
void sleep(SleepMode mode) {
    uint8_t adcsraSave = ADCSRA;
    ADEN.clear(); // disable the ADC
    switch(mode) {
    case SleepMode::POWER_DOWN:
        (~SM0 | SM1 | ~SM2).apply(); break;
    case SleepMode::STANDBY:
        (~SM0 | SM1 | SM2).apply(); break;
    case SleepMode::IDLE:
        (~SM0 | ~SM1 | ~SM2).apply(); break;
    }

    {
        AtomicScope::SEI _;
        SE.set(); // sleep enable
        sleep_bod_disable();
    }
    sleep_cpu();
    SE.clear(); // sleep disable
    ADCSRA = adcsraSave;
}


template <typename rt_t>
class Power {
    typedef Power<rt_t> This;
    typedef Logging::Log<Loggers::Power> log;

    rt_t *rt;
    volatile uint8_t _watchdogCounter;

    void setWatchdogInterrupts (int8_t mode) {
        // correct for the fact that WDP3 is *not* in bit position 3!
        if (mode & (1 << 3))
            mode ^= (1 << 3) | WDP3;
        // pre-calculate the WDTCSR value, can't do it inside the timed sequence
        // we only generate interrupts, no reset
        uint8_t wdtcsr = mode >= 0 ? WDIE | mode : 0;
        WDRF.clear();
        {
        	AtomicScope::SEI _;
			WDTCSR |= WDCE | WDE; // timed sequence
			WDTCSR = wdtcsr;
        }
    }

    void onWatchdog() {
        _watchdogCounter++;
    }

    bool doSleepFor(Milliseconds ms, SleepMode mode, SleepGranularity maxGranularity) {
    	log::debug(F("Z: "), dec(ms.getValue()), F("ms in "), '0' + uint8_t(mode));

        if (mode != SleepMode::IDLE) {
        	log::flush();
        }

        bool interrupted = false;
        uint32_t millisSleep = ms.getValue();
        if (millisSleep <= 16) {
            return false;
        }

        uint32_t msleft = millisSleep;
        // only slow down for periods longer than the watchdog granularity
        while (msleft >= 16) {
            uint8_t wdp = 0; // wdp 0..9 corresponds to roughly 16..8192 ms
            // calc wdp as log2(msleft/16), i.e. loop & inc while next value is ok
            for (uint32_t m = msleft; m >= 32; m >>= 1)
                if (++wdp >= uint8_t(maxGranularity))
                    break;
            _watchdogCounter = 0;
            setWatchdogInterrupts(wdp);
            sleep(mode);
            setWatchdogInterrupts(-1); // off
            // when interrupted, our best guess is that half the time has passed
            uint32_t halfms = 8 << wdp;
            msleft -= halfms;
            if (_watchdogCounter == 0) {
                interrupted = true; // lost some time, but got interrupted
                break;
            }
            msleft -= halfms;
        }

        if (mode != SleepMode::IDLE) {
            // adjust ticks for the delay we've just had. Not for IDLE, since timers keep running
            // there.
            millisSleep -= msleft;
            auto ms = Milliseconds(millisSleep);
            rt->haveSlept(ms);
        }

        log::debug(F("W: "), '0' + interrupted);
        return interrupted;
    }

    bool sleepUntilAnyLT(Milliseconds timeout, SleepMode mode) {
        return doSleepFor(timeout, mode, SleepGranularity::_8000ms);
    }

    template <typename periodic_t, typename... periodic_ts>
    bool sleepUntilAnyLT(Milliseconds timeout, SleepMode mode, const periodic_t &head, const periodic_ts&... tail) {
        auto headTime = toMillisOn<rt_t>(head.timeLeft());
        if (headTime < timeout) {
            return sleepUntilAnyLT(headTime, mode, tail...);
        } else {
            return sleepUntilAnyLT(timeout, mode, tail...);
        }
    }

public:
    typedef On<This, Int_WDT_, &This::onWatchdog> Handlers;

    Power(rt_t &_rt): rt(&_rt) {}

    /**
     * Attempts to sleep (power down) for at most until the given Deadline or Periodic timer fires.
     * A hardware or pin change interrupt can cause premature wake-up.
     * Sleeping is performed by sleeping multiple consecutive watchdog timer intervals. If an interrupt occurs
     * during a watchdog timer interval, the function guesses that 1/2 of that interval's time has elapsed. This causes
     * real-time keeping to go out of sync by, on average 1/2 of the interval time. Set [maxGranularity] to the
     * highest interval that's acceptable (higher values means less power used).
     *
     * Returns whether any hardware interrupts have interrupted sleep.
     */
    template <typename periodic_t>
    bool sleepUntil(const periodic_t &p, SleepMode mode, SleepGranularity maxGranularity) {
        return sleepFor(p.timeLeft(), mode, maxGranularity);
    }

    template <typename periodic_t>
    bool sleepUntil(const periodic_t &p, SleepMode mode) {
        return sleepFor(p.timeLeft(), mode, SleepGranularity::_8000ms);
    }

    // FIXME assert that the periodics are of the same rt_t
    /**
     * Sleeps until the EARLIEST of the given periodic or deadline instances fires.
     */
    template <typename periodic_t, typename... periodic_ts>
    bool sleepUntilAny(SleepMode mode, const periodic_t &head, const periodic_ts&... tail) {
        return sleepUntilAnyLT(toMillisOn<rt_t>(head.timeLeft()), mode, tail...);
    }

    bool sleepUntilTasks() {
    	log::debug(F("Error: no non-idle tasks"));
    	log::flush();
    	// all tasks are idle... programmer error. let's not sleep just to be safe.
    	return false;
    }

    bool sleepUntilTasks(TaskState task) {
    	if (task.isIdle()) {
    		return sleepUntilTasks();
    	} else {
    		return sleepFor(task.timeLeft(), task.getMaxSleepMode(), SleepGranularity::_8000ms);
    	}
    }

    template <typename... types>
    bool sleepUntilTasks(TaskState task1, TaskState task2, types... tail) {
    	if (task1.isIdle()) {
    		return sleepUntilTasks(task2, tail...);
    	} else {
    		if (task2.isIdle()) {
    			return sleepUntilTasks(task1, tail...);
    		} else {
    			// both tasks are non-idle, let's compare them.
    	    	auto mode = (task1.getMaxSleepMode() > task2.getMaxSleepMode()) ? task2.getMaxSleepMode() : task1.getMaxSleepMode();
    	    	Milliseconds time1 = task1.timeLeft();
    	    	Milliseconds time2 = task2.timeLeft();
    	    	auto time = (time1 > time2) ? time2 : time1;
    	    	return sleepUntilTasks(TaskState::busy(time, mode), tail...);
    		}
    	}
    }

    /**
     * Attempts to sleep (power down) for at most the given time.
     * A hardware or pin change interrupt can cause premature wake-up.
     * Sleeping is performed by sleeping multiple consecutive watchdog timer intervals. If an interrupt occurs
     * during a watchdog timer interval, the function guesses that 1/2 of that interval's time has elapsed. This causes
     * real-time keeping to go out of sync by, on average 1/2 of the interval time. Set [maxGranularity] to the
     * highest interval that's acceptable (higher values means less power used).
     *
     * Returns whether any hardware interrupts have interrupted sleep.
     *
     * @param time Time to sleep, subclass of RuntimeTimeUnit. Must be >16ms, and granularity
     * is only about 16ms.
     * @param mode Sleep mode to enter while sleeping
     * @param maxGranularity Coursest granularity for individual sleep steps. Courser granularity will give higher
     * power savings, but less time precision.
     */
    template <typename time_t>
    bool sleepFor(time_t time, SleepMode mode, SleepGranularity maxGranularity) {
        return doSleepFor(toMillisOn<rt_t>(time), mode, maxGranularity);
    }

    template <typename time_t>
    bool sleepFor(time_t time, SleepMode mode) {
        return sleepFor(time, mode, SleepGranularity::_8000ms);
    }

    bool sleepFor(Milliseconds ms, SleepMode mode, SleepGranularity maxGranularity) {
        return doSleepFor(ms, mode, maxGranularity);
    }
};
}

template <typename rt_t>
Impl::Power<rt_t> Power(rt_t &rt) {
    return Impl::Power<rt_t>(rt);
}

}
}



#endif /* HAL_ATMEL_POWER_HPP_ */
