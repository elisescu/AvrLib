#ifndef DHT_DHT11_HPP_
#define DHT_DHT11_HPP_

#include "DHT.hpp"

namespace DHT {

namespace Impl {

/**
 * DHT11 driver. Gets a measurement on startup after which it goes to idle. Call measure() to initiate another measurement.
 *
 *      /------------\
 *      |            |
 *      |   DHT11    |
 *      |  (front)   |
 *      |            |
 *      |            |
 *      \-+--+--+--+-/
 *        |  |  |  |
 *        1  2  3  4
 *        |  |  x  |
 *     VCC/  |     \GND
 *           \DATA
 *
 * @param datapin_t DATA pin. We'll use the internal pull-up resistor of the micro.
 * @param powerpin_t VCC pin. The sensor will be powered by the microcontroller.
 */
template <typename datapin_t, typename powerpin_t, typename comparator_t, typename rt_t>
class DHT11: public DHT<datapin_t, powerpin_t, comparator_t, rt_t> {
    typedef DHT11<datapin_t, powerpin_t, comparator_t, rt_t> This;
    typedef DHT<datapin_t, powerpin_t, comparator_t, rt_t> Super;
    using Super::getLastFailure;
public:
    using Super::DHT;

    /**
     * Returns the temperature in tenths of degrees celcius, e.g. 320 for 32 degrees celcius.
     * However, the DHT11's precision is whole degrees only, and only temperatures between 0..100 degrees C.
     */
    Option<int16_t> getTemperature() const {
    	if (getLastFailure() == 0) {
    		return Super::getData(2) * 10;
    	} else {
    		return none();
    	}
    }

    /**
     * Returns the relative humidity in tenths of percent, e.g. 625 for 62.5%.
     * However, the DHT11's precision is whole percentages only.
     */
    Option<uint16_t> getHumidity() const {
    	if (getLastFailure() == 0) {
    		return Super::getData(0) * 10;
    	} else {
    		return none();
    	}
    }
};

}

template <typename datapin_t, typename powerpin_t, typename comparator_t, typename rt_t>
Impl::DHT11<datapin_t, powerpin_t, comparator_t, rt_t> DHT11(datapin_t &pin, powerpin_t &power, comparator_t &comp, rt_t &rt) {
    return { pin, power, comp, rt };
}

}



#endif /* DHT_DHT11_HPP_ */
