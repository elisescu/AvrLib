#ifndef STREAMS_PROTOCOL_HPP_
#define STREAMS_PROTOCOL_HPP_

#include "gcc_type_traits.h"
#include "TypeTraits.hpp"
#include "ReadResult.hpp"

namespace Streams {
namespace Impl {

template <typename T, typename P>
struct WithProtocol {
    T * const t;
    WithProtocol(T *_t): t(_t) {}
};

template <typename T, typename P>
struct WithProtocolConst {
    const T * const t;
    WithProtocolConst(const T *_t): t(_t) {}
};

template <typename fifo_t, typename T, typename P>
ReadResult read1(fifo_t &fifo, WithProtocol<T, P> wrapper) {
    return P::read1(fifo, wrapper.t);
}

template <typename sem, typename fifo_t, typename T, typename P>
bool write1(fifo_t &fifo, WithProtocol<T, P> wrapper) {
    return P::template write1<sem>(fifo, wrapper.t);
}

template <typename sem, typename fifo_t, typename T, typename P>
bool write1(fifo_t &fifo, WithProtocolConst<T, P> wrapper) {
    return P::template write1<sem>(fifo, wrapper.t);
}

} // namespace Streams
} // namespace Impl

#include "ReadingN.hpp"
#include "WritingN.hpp"
#include "Format.hpp"

namespace Streams {

template <typename This>
struct Protocol;

template <typename tp, typename = void>
struct has_T: std::false_type {};

template <typename tp>
struct has_T<tp, typename exists<typename tp::T>::type>: std::true_type {};

namespace find_T_Impl {

template<typename check, typename... types>
struct find_T {
    // fallback for empty
    // T was not found in any arguments
};

template<typename head_t, typename... tail_t>
struct find_T<typename std::enable_if<has_T<head_t>::value>::type, head_t, tail_t...> {
    // has has T
    typedef typename head_t::T type;
};

template<typename head_t, typename... tail_t>
struct find_T<typename std::enable_if<!has_T<head_t>::value>::type, head_t, tail_t...> {
    // has no T
    typedef typename find_T<void, tail_t...>::type type;
};

}

template <typename... types> using find_T = find_T_Impl::find_T<void, types...>;

/**
 * Returns a wrapper indicating to read/write [t] using the protocol given as template parameters.
 * This can be used in order to read/write a struct that does not have a DefaultProtocol typedef
 * (or customize a struct that does have one).
 */
template <typename... types>
Impl::WithProtocol<typename find_T<types...>::type,
                   typename Protocol<typename find_T<types...>::type>::template Seq<types...>>
                   as (typename find_T<types...>::type *t) {
    return t;
}

template <typename... types>
Impl::WithProtocolConst<typename find_T<types...>::type,
                        typename Protocol<typename find_T<types...>::type>::template Seq<types...>>
                        as (const typename find_T<types...>::type *t) {
    return t;
}

template <typename This, typename... types>
struct total_length {
    constexpr static uint8_t apply(const This *t) {
        uint8_t result = 0;
        using expand_variadic_pack  = int[]; // dirty trick, see below
          (void)expand_variadic_pack{0, ((result += types::length(t)), void(), 0)... };
        //auto a = {(result += types::length(t))... }; (void)a;
        return result;
    }
};

template <typename This>
struct Protocol {

    /** Indicates a sequence of protocol elements that should be written/read one after the other */
    template <typename... types>
    struct Seq {
        typedef This T;

        template <typename fifo_t>
        static ReadResult read1(fifo_t &fifo, This *t) {
            return Impl::readN(fifo, ReadResult::Valid, types::forReading(t)...);
        }

        template <typename sem, typename fifo_t>
        static bool write1(fifo_t &fifo, const This *t) {
            return Impl::writeN<sem>(fifo, types::forWriting(t)...);
        }

        static uint8_t length(const This *t) {
            return total_length<This,types...>::apply(t);
        }

        /** write this Seq's binary representation as a protobuf message field */
        template <uint8_t fieldIdx>
        static auto forWriting(const This *t) {
            return ::Streams::Nested([t] (auto write) {
                uint16_t l = Seq<types...>::length(t);
                return write(
                    uint8_t(fieldIdx << 3 | Streams::Impl::LENGTH_DELIMITED),
                    Streams::Protobuf::BareVarint<uint16_t>(l),
                    types::forWriting(t)...
                );
            });
        }
    };

    template <typename... types>
    struct Seq<Seq<types...>>: public Seq<types...> {};

    template <typename T>
    using Single = Seq<T>;

    /** Serializes a field as binary, little endian */
    template <typename int_t, int_t This::*field>
    struct Binary: public Single<Binary<int_t, field>> {
        static int_t* forReading(This *t) {
            return &(t->*field);
        }

        static int_t forWriting(const This *t) {
            return t->*field;
        }

        static constexpr uint8_t length(const This *t) {
            return sizeof(int_t);
        }
    };

    /** Serializes a field as decimal */
    template <typename int_t, int_t This::*field>
    struct Decimal: public Single<Decimal<int_t, field>> {
        static ::Streams::Impl::Decimal<int_t*> forReading(This *t) {
            return ::Streams::Decimal(&(t->*field));
        }

        static ::Streams::Impl::Decimal<int_t> forWriting(const This *t) {
            return ::Streams::Decimal(t->*field);
        }

        static uint8_t length(const This *t) {
            int_t i = t->*field;
            uint8_t l = 1;
            while (i > 9) {
                l++;
                i = i / 10;
            }
            return l;
        }
    };

    /** Serializes a field as hexadecimal, with fixed length (padded with zeroes) */
    template <typename int_t, int_t This::*field>
    struct Hexadecimal: public Single<Hexadecimal<int_t, field>> {
        static ::Streams::Impl::Hexadecimal<int_t*> forReading(This *t) {
            return ::Streams::Hexadecimal(&(t->*field));
        }

        static ::Streams::Impl::Hexadecimal<int_t> forWriting(const This *t) {
            return ::Streams::Hexadecimal(t->*field);
        }

        static constexpr uint8_t length(const This *t) {
            return sizeof(int_t) * 2;
        }
    };


    /** Serializes padding of indicated length bytes, written as 0, ignored during read */
    template <uint8_t _length>
    struct Padding: public Single<Padding<_length>> {
        static Streams::Padding forReading(This *t) {
            return Streams::Padding(_length);
        }

        static Streams::Padding forWriting(const This *t) {
            return Streams::Padding(_length);
        }

        static constexpr uint8_t length(const This *t) {
            return _length;
        }
    };

    // In addition, you can also include a fixed String or ByteString token in a Protocol by saying
    // STR("foo") or BSTR(1,2,3). During writing the string will be written, during reading the string
    // will be verified.

    template <typename F, F This::*field, typename P = typename F::DefaultProtocol>
    struct Object: public Single<Object<F, field, P>> {
        static Impl::WithProtocol<F, P> forReading(This *t) {
            return &(t->*field);
        }
        static const Impl::WithProtocolConst<F, P> forWriting(const This *t) {
            return &(t->*field);
        }
        static uint8_t length(const This *t) {
            return P::template length(&(t->*field));
        }
    };

    template <bool (This::*condition)() const, typename... elements>
    struct Conditional: public Single<Conditional<condition, elements...>> {
        static auto forReading(This *t) {
            return ::Streams::Nested([t] (auto read) {
                return ((t->*condition)()) ? read(elements::forReading(t)...) : ReadResult::Valid;
            });
        }

        static auto forWriting(const This *t) {
            return ::Streams::Nested([t] (auto write) {
                return ((t->*condition)()) ? write(elements::forWriting(t)...) : true;
            });
        }

        static uint8_t length(const This *t) {
            return ((t->*condition)()) ? total_length<This, elements...>::apply(t) : 0;
        }
    };

    template <bool (This::*condition)() const, typename... elements>
    struct Conditional<condition, Seq<elements...>>: public Conditional<condition, elements...> {

    };

    template<typename ElementType, uint8_t count, ElementType (This::*field)[count]>
    struct Array: public Single<Array<ElementType, count, field>> {
        static ElementType (*forReading(This *t)) [count] {
            return &(t->*field);
        }

        static const ElementType (*forWriting(const This *t)) [count] {
            return &(t->*field);
        }

        static uint8_t length(const This *t) {
            static_assert(std::is_integral<ElementType>::value, "Element type must be an integral type.");
            return sizeof(ElementType);
        }
    };
};

}

#endif /* STREAMS_PROTOCOL_HPP_ */
