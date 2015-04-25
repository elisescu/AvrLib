#ifndef __AVR__
#include <type_traits>
#else

#ifndef _GLIBCXX_TYPE_TRAITS
#define _GLIBCXX_TYPE_TRAITS 1

// For some reason, avr-gcc does not bundle type_traits.h. We copy some of its definitions here.

namespace std {

// Primary template.
/// Define a member typedef @c type only if a boolean constant is true.
template<bool, typename _Tp = void>
  struct enable_if
  { };

// Partial specialization for true.
template<typename _Tp>
  struct enable_if<true, _Tp>
  { typedef _Tp type; };

/// integral_constant
template<typename _Tp, _Tp __v>
  struct integral_constant
  {
    static constexpr _Tp                  value = __v;
    typedef _Tp                           value_type;
    typedef integral_constant<_Tp, __v>   type;
    constexpr operator value_type() const { return value; }
#if __cplusplus > 201103L

#define __cpp_lib_integral_constant_callable 201304

    constexpr value_type operator()() const { return value; }
#endif
  };

template<typename _Tp, _Tp __v>
  constexpr _Tp integral_constant<_Tp, __v>::value;

/// The type used as a compile-time boolean with true value.
typedef integral_constant<bool, true>     true_type;

/// The type used as a compile-time boolean with false value.
typedef integral_constant<bool, false>    false_type;

/// is_enum
template<typename _Tp>
  struct is_enum
  : public integral_constant<bool, __is_enum(_Tp)>
  { };

/// is_class
template<typename _Tp>
  struct is_class
  : public integral_constant<bool, __is_class(_Tp)>
  { };

/// The underlying type of an enum.
template<typename _Tp>
  struct underlying_type
  {
    typedef __underlying_type(_Tp) type;
  };

template<typename, typename>
    struct is_same;

template<typename, typename>
    struct is_same
  : public false_type { };

template<typename _Tp>
    struct is_same<_Tp, _Tp>
  : public true_type { };

/// remove_const
template<typename _Tp>
  struct remove_const
  { typedef _Tp     type; };

template<typename _Tp>
  struct remove_const<_Tp const>
  { typedef _Tp     type; };

/// remove_volatile
template<typename _Tp>
  struct remove_volatile
  { typedef _Tp     type; };

template<typename _Tp>
  struct remove_volatile<_Tp volatile>
  { typedef _Tp     type; };

/// remove_cv
template<typename _Tp>
  struct remove_cv
  {
    typedef typename
    remove_const<typename remove_volatile<_Tp>::type>::type     type;
  };


template<typename>
    struct __is_pointer_helper
    : public false_type { };

  template<typename _Tp>
    struct __is_pointer_helper<_Tp*>
    : public true_type { };

  /// is_pointer
  template<typename _Tp>
    struct is_pointer
    : public __is_pointer_helper<typename remove_cv<_Tp>::type>::type
    { };

}

#endif
#endif

