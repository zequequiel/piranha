/* Copyright 2009-2017 Francesco Biscani (bluescarni@gmail.com)

This file is part of the Piranha library.

The Piranha library is free software; you can redistribute it and/or modify
it under the terms of either:

  * the GNU Lesser General Public License as published by the Free
    Software Foundation; either version 3 of the License, or (at your
    option) any later version.

or

  * the GNU General Public License as published by the Free Software
    Foundation; either version 3 of the License, or (at your option) any
    later version.

or both in parallel, as here.

The Piranha library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received copies of the GNU General Public License and the
GNU Lesser General Public License along with the Piranha library.  If not,
see https://www.gnu.org/licenses/. */

#ifndef PIRANHA_MP_RATIONAL_HPP
#define PIRANHA_MP_RATIONAL_HPP

#include <array>
#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <piranha/binomial.hpp>
#include <piranha/config.hpp>
#include <piranha/detail/demangle.hpp>
#include <piranha/detail/mp_rational_fwd.hpp>
#include <piranha/exceptions.hpp>
#include <piranha/math.hpp>
#include <piranha/mp_integer.hpp>
#include <piranha/pow.hpp>
#include <piranha/print_tex_coefficient.hpp>
#include <piranha/s11n.hpp>
#include <piranha/safe_cast.hpp>
#include <piranha/type_traits.hpp>

namespace piranha
{

/// Multiple precision rational class.
/**
 * This class encapsulates two instances of piranha::mp_integer to represent an arbitrary-precision rational number
 * in terms of a numerator and a denominator. The meaning of the \p SSize template parameter is the same as in
 * piranha::mp_integer, that is, it represents the number of limbs stored statically in the numerator and
 * in the denominator.
 *
 * Unless otherwise specified, rational numbers are always kept in the usual canonical form in which numerator and
 * denominator are coprime, and the denominator is always positive. Zero is uniquely represented by 0/1.
 *
 * ## Interoperability with other types ##
 *
 * This class interoperates with the same types as piranha::mp_integer, plus piranha::mp_integer itself.
 *
 * ## Exception safety guarantee ##
 *
 * This class provides the strong exception safety guarantee for all operations. In case of memory allocation errors by
 * GMP routines, the program will terminate.
 *
 * ## Move semantics ##
 *
 * Move construction and move assignment will leave the moved-from object in an unspecified but valid state.
 */
template <std::size_t SSize>
class mp_rational
{
public:
    /// The underlying piranha::mp_integer type used to represent numerator and denominator.
    using int_type = mp_integer<SSize>;

private:
    // Shortcut for interop type detector.
    template <typename T>
    using is_interoperable_type = disjunction<mppp::mppp_impl::is_supported_interop<T>, std::is_same<T, int_type>>;
    // Enabler for ctor from num den pair.
    template <typename I0, typename I1>
    using nd_ctor_enabler
        = enable_if_t<conjunction<disjunction<std::is_integral<I0>, std::is_same<I0, int_type>>,
                                  disjunction<std::is_integral<I1>, std::is_same<I1, int_type>>>::value,
                      int>;
    // Enabler for generic ctor.
    template <typename T>
    using generic_ctor_enabler = enable_if_t<is_interoperable_type<T>::value, int>;
    // Enabler for in-place arithmetic operations with interop on the left.
    template <typename T>
    using generic_in_place_enabler
        = enable_if_t<conjunction<is_interoperable_type<T>, negation<std::is_const<T>>>::value, int>;
    // Generic constructor implementation.
    template <typename T, enable_if_t<std::is_integral<T>::value, int> = 0>
    void construct_from_interoperable(const T &x)
    {
        m_num = int_type(x);
        m_den = 1;
    }
    template <typename T, enable_if_t<std::is_same<T, int_type>::value, int> = 0>
    void construct_from_interoperable(const T &x)
    {
        m_num = x;
        m_den = 1;
    }
    template <typename Float, enable_if_t<std::is_floating_point<Float>::value, int> = 0>
    void construct_from_interoperable(const Float &x)
    {
        if (unlikely(!std::isfinite(x))) {
            piranha_throw(std::invalid_argument, "cannot construct a rational from a non-finite floating-point number");
        }
        // Denominator is always inited as 1.
        m_den = 1;
        if (x == Float(0)) {
            // m_den is 1 already.
            return;
        }
        Float abs_x = std::abs(x);
        const unsigned radix = static_cast<unsigned>(std::numeric_limits<Float>::radix);
        int_type i_part;
        int exp = std::ilogb(abs_x);
        while (exp >= 0) {
            i_part += math::pow(int_type(radix), exp);
            const Float tmp = std::scalbn(Float(1), exp);
            if (unlikely(tmp == HUGE_VAL)) {
                piranha_throw(std::invalid_argument, "output of std::scalbn is HUGE_VAL");
            }
            abs_x -= tmp;
            // Break out if x is an exact integer.
            if (abs_x == Float(0)) {
                // m_den is 1 already.
                m_num = i_part;
                if (x < Float(0)) {
                    m_num.neg();
                }
                return;
            }
            exp = std::ilogb(abs_x);
            if (unlikely(exp == INT_MAX || exp == FP_ILOGBNAN)) {
                piranha_throw(std::invalid_argument, "error calling std::ilogb");
            }
        }
        piranha_assert(abs_x < Float(1));
        // Lift up the decimal part into an integer.
        while (abs_x != Float(0)) {
            abs_x = std::scalbln(abs_x, 1);
            if (unlikely(abs_x == HUGE_VAL)) {
                piranha_throw(std::invalid_argument, "output of std::scalbn is HUGE_VAL");
            }
            const auto t_abs_x = std::trunc(abs_x);
            m_den *= radix;
            m_num *= radix;
            // NOTE: here t_abs_x is guaranteed to be in
            // [0,radix - 1], so the cast to unsigned should be ok.
            // Note that floating-point numbers are guaranteed to be able
            // to represent exactly at least a [-exp,+exp] exponent range
            // (see the minimum values for the FLT constants in the C standard).
            m_num += static_cast<unsigned>(t_abs_x);
            abs_x -= t_abs_x;
        }
        math::multiply_accumulate(m_num, i_part, m_den);
        canonicalise();
        if (x < Float(0)) {
            m_num.neg();
        }
    }
    // Enabler for conversion operator.
    template <typename T>
    using cast_enabler = generic_ctor_enabler<T>;
    // Conversion operator implementation.
    template <typename Float, enable_if_t<std::is_floating_point<Float>::value, int> = 0>
    Float convert_to_impl() const
    {
        // NOTE: there are better ways of doing this. For instance, here we might end up generating an inf even
        // if the result is actually representable. It also would be nice if this routine could short-circuit,
        // that is, for every rational generated from a float we get back exactly the same float after the cast.
        // The approach in GMP mpq might work for this, but it's not essential at the moment.
        return static_cast<Float>(m_num) / static_cast<Float>(m_den);
    }
    template <typename Integral, enable_if_t<std::is_integral<Integral>::value, int> = 0>
    Integral convert_to_impl() const
    {
        return static_cast<Integral>(static_cast<int_type>(*this));
    }
    template <typename MpInteger, enable_if_t<std::is_same<MpInteger, int_type>::value, int> = 0>
    MpInteger convert_to_impl() const
    {
        return m_num / m_den;
    }
    // In-place add.
    mp_rational &in_place_add(const mp_rational &other)
    {
        // NOTE: all this should never throw because we only operate on mp_integer objects,
        // no conversions involved, etc.
        const bool u1 = m_den.is_one(), u2 = other.m_den.is_one();
        if (u1 && u2) {
            // Both are integers, just add without canonicalising. This is safe if
            // this and other are the same object.
            m_num += other.m_num;
        } else if (u1) {
            // Only this is an integer.
            // NOTE: figure out a way here to use multiply_accumulate(). Most likely we need
            // a tmp copy, so better profile it first.
            m_num = m_num * other.m_den + other.m_num;
            m_den = other.m_den;
        } else if (u2) {
            // Only other is an integer.
            math::multiply_accumulate(m_num, m_den, other.m_num);
        } else if (m_den == other.m_den) {
            // Denominators are the same, add numerators and canonicalise.
            // NOTE: safe if this and other coincide.
            m_num += other.m_num;
            canonicalise();
        } else {
            // The general case.
            // NOTE: here dens are different (cannot coincide), and thus
            // this and other must be separate objects.
            m_num *= other.m_den;
            math::multiply_accumulate(m_num, m_den, other.m_num);
            m_den *= other.m_den;
            canonicalise();
        }
        return *this;
    }
    mp_rational &in_place_add(const int_type &other)
    {
        if (m_den.is_one()) {
            // If den is unitary, no need to multiply.
            m_num += other;
        } else {
            math::multiply_accumulate(m_num, m_den, other);
        }
        return *this;
    }
    template <typename T, enable_if_t<std::is_integral<T>::value, int> = 0>
    mp_rational &in_place_add(const T &n)
    {
        return in_place_add(int_type(n));
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    mp_rational &in_place_add(const T &x)
    {
        return (*this = static_cast<T>(*this) + x);
    }
    // Binary add.
    template <typename T>
    static mp_rational binary_plus_impl(const mp_rational &q1, const T &x)
    {
        auto retval(q1);
        retval += x;
        return retval;
    }
    static mp_rational binary_plus(const mp_rational &q1, const mp_rational &q2)
    {
        return binary_plus_impl(q1, q2);
    }
    template <typename T, enable_if_t<disjunction<std::is_integral<T>, std::is_same<T, int_type>>::value, int> = 0>
    static mp_rational binary_plus(const mp_rational &q1, const T &x)
    {
        return binary_plus_impl(q1, x);
    }
    template <typename T, enable_if_t<disjunction<std::is_integral<T>, std::is_same<T, int_type>>::value, int> = 0>
    static mp_rational binary_plus(const T &x, const mp_rational &q2)
    {
        return binary_plus(q2, x);
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static T binary_plus(const mp_rational &q1, const T &x)
    {
        return x + static_cast<T>(q1);
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static T binary_plus(const T &x, const mp_rational &q2)
    {
        return binary_plus(q2, x);
    }
    // In-place sub.
    mp_rational &in_place_sub(const mp_rational &other)
    {
        // NOTE: optimisations are possible here if we implement multiply_sub
        // or do some trickery with in-place negation + multiply_accumulate().
        // Keep it in mind for future optimisations.
        const bool u1 = m_den.is_one(), u2 = other.m_den.is_one();
        if (u1 && u2) {
            m_num -= other.m_num;
        } else if (u1) {
            m_num = m_num * other.m_den - other.m_num;
            m_den = other.m_den;
        } else if (u2) {
            m_num = m_num - m_den * other.m_num;
        } else if (m_den == other.m_den) {
            m_num -= other.m_num;
            canonicalise();
        } else {
            m_num *= other.m_den;
            // Negate temporarily in order to use multiply_accumulate.
            // NOTE: candidate for multiply_sub if we ever implement it.
            m_den.neg();
            math::multiply_accumulate(m_num, m_den, other.m_num);
            m_den.neg();
            m_den *= other.m_den;
            canonicalise();
        }
        return *this;
    }
    mp_rational &in_place_sub(const int_type &other)
    {
        if (m_den.is_one()) {
            m_num -= other;
        } else {
            m_den.neg();
            math::multiply_accumulate(m_num, m_den, other);
            m_den.neg();
        }
        return *this;
    }
    template <typename T, enable_if_t<std::is_integral<T>::value, int> = 0>
    mp_rational &in_place_sub(const T &n)
    {
        return in_place_sub(int_type(n));
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    mp_rational &in_place_sub(const T &x)
    {
        return (*this = static_cast<T>(*this) - x);
    }
    // Binary sub.
    template <typename T>
    static mp_rational binary_minus_impl(const mp_rational &q1, const T &x)
    {
        auto retval(q1);
        retval -= x;
        return retval;
    }
    static mp_rational binary_minus(const mp_rational &q1, const mp_rational &q2)
    {
        return binary_minus_impl(q1, q2);
    }
    template <typename T, enable_if_t<disjunction<std::is_integral<T>, std::is_same<T, int_type>>::value, int> = 0>
    static mp_rational binary_minus(const mp_rational &q1, const T &x)
    {
        return binary_minus_impl(q1, x);
    }
    template <typename T, enable_if_t<disjunction<std::is_integral<T>, std::is_same<T, int_type>>::value, int> = 0>
    static mp_rational binary_minus(const T &x, const mp_rational &q2)
    {
        auto retval = binary_minus(q2, x);
        retval.negate();
        return retval;
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static T binary_minus(const mp_rational &q1, const T &x)
    {
        return static_cast<T>(q1) - x;
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static T binary_minus(const T &x, const mp_rational &q2)
    {
        return -binary_minus(q2, x);
    }
    // In-place mult.
    mp_rational &in_place_mult(const mp_rational &other)
    {
        if (m_den.is_one() && other.m_den.is_one()) {
            m_num *= other.m_num;
        } else {
            // NOTE: no issue here if this and other are the same object.
            m_num *= other.m_num;
            m_den *= other.m_den;
            canonicalise();
        }
        return *this;
    }
    mp_rational &in_place_mult(const int_type &other)
    {
        m_num *= other;
        if (!m_den.is_one()) {
            canonicalise();
        }
        return *this;
    }
    template <typename T, enable_if_t<std::is_integral<T>::value, int> = 0>
    mp_rational &in_place_mult(const T &n)
    {
        return in_place_mult(int_type(n));
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    mp_rational &in_place_mult(const T &x)
    {
        return (*this = static_cast<T>(*this) * x);
    }
    // Binary mult.
    template <typename T>
    static mp_rational binary_mult_impl(const mp_rational &q1, const T &x)
    {
        auto retval(q1);
        retval *= x;
        return retval;
    }
    static mp_rational binary_mult(const mp_rational &q1, const mp_rational &q2)
    {
        return binary_mult_impl(q1, q2);
    }
    template <typename T, enable_if_t<disjunction<std::is_integral<T>, std::is_same<T, int_type>>::value, int> = 0>
    static mp_rational binary_mult(const mp_rational &q1, const T &x)
    {
        return binary_mult_impl(q1, x);
    }
    template <typename T, enable_if_t<disjunction<std::is_integral<T>, std::is_same<T, int_type>>::value, int> = 0>
    static mp_rational binary_mult(const T &x, const mp_rational &q2)
    {
        return binary_mult(q2, x);
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static T binary_mult(const mp_rational &q1, const T &x)
    {
        return x * static_cast<T>(q1);
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static T binary_mult(const T &x, const mp_rational &q2)
    {
        return binary_mult(q2, x);
    }
    // In-place div.
    mp_rational &in_place_div(const mp_rational &other)
    {
        // NOTE: need to do this, otherwise the cross num/dem
        // operations below will mess num and den up. This is ok
        // if this == 0, as this is checked in the outer operator.
        if (unlikely(this == &other)) {
            m_num = 1;
            m_den = 1;
        } else {
            m_num *= other.m_den;
            m_den *= other.m_num;
            canonicalise();
        }
        return *this;
    }
    mp_rational &in_place_div(const int_type &other)
    {
        m_den *= other;
        canonicalise();
        return *this;
    }
    template <typename T, enable_if_t<std::is_integral<T>::value, int> = 0>
    mp_rational &in_place_div(const T &n)
    {
        return in_place_div(int_type(n));
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    mp_rational &in_place_div(const T &x)
    {
        return (*this = static_cast<T>(*this) / x);
    }
    // Binary div.
    template <typename T>
    static mp_rational binary_div_impl(const mp_rational &q1, const T &x)
    {
        auto retval(q1);
        retval /= x;
        return retval;
    }
    static mp_rational binary_div(const mp_rational &q1, const mp_rational &q2)
    {
        return binary_div_impl(q1, q2);
    }
    template <typename T, enable_if_t<disjunction<std::is_integral<T>, std::is_same<T, int_type>>::value, int> = 0>
    static mp_rational binary_div(const mp_rational &q1, const T &x)
    {
        return binary_div_impl(q1, x);
    }
    template <typename T, enable_if_t<disjunction<std::is_integral<T>, std::is_same<T, int_type>>::value, int> = 0>
    static mp_rational binary_div(const T &x, const mp_rational &q2)
    {
        mp_rational retval(x);
        retval /= q2;
        return retval;
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static T binary_div(const mp_rational &q1, const T &x)
    {
        return static_cast<T>(q1) / x;
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static T binary_div(const T &x, const mp_rational &q2)
    {
        return x / static_cast<T>(q2);
    }
    // Equality operator.
    static bool binary_eq(const mp_rational &q1, const mp_rational &q2)
    {
        return q1.num() == q2.num() && q1.den() == q2.den();
    }
    template <typename T, enable_if_t<disjunction<std::is_integral<T>, std::is_same<T, int_type>>::value, int> = 0>
    static bool binary_eq(const mp_rational &q, const T &x)
    {
        return q.den().is_one() && q.num() == x;
    }
    template <typename T, enable_if_t<disjunction<std::is_integral<T>, std::is_same<T, int_type>>::value, int> = 0>
    static bool binary_eq(const T &x, const mp_rational &q)
    {
        return binary_eq(q, x);
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static bool binary_eq(const mp_rational &q, const T &x)
    {
        return static_cast<T>(q) == x;
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static bool binary_eq(const T &x, const mp_rational &q)
    {
        return binary_eq(q, x);
    }
    // Less-than operator.
    static bool binary_less_than(const mp_rational &q1, const mp_rational &q2)
    {
        // NOTE: this is going to be slow in general. The implementation in GMP
        // checks the limbs number before doing any multiplication, and probably
        // other tricks. If this ever becomes a bottleneck, we probably need to do
        // something similar (actually we could just use the view and piggy-back
        // on mpq_cmp()...).
        if (q1.m_den == q2.m_den) {
            return q1.m_num < q2.m_num;
        }
        return q1.num() * q2.den() < q2.num() * q1.den();
    }
    template <typename T, enable_if_t<disjunction<std::is_integral<T>, std::is_same<T, int_type>>::value, int> = 0>
    static bool binary_less_than(const mp_rational &q, const T &x)
    {
        return q.num() < q.den() * x;
    }
    template <typename T, enable_if_t<disjunction<std::is_integral<T>, std::is_same<T, int_type>>::value, int> = 0>
    static bool binary_less_than(const T &x, const mp_rational &q)
    {
        return q.den() * x < q.num();
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static bool binary_less_than(const mp_rational &q, const T &x)
    {
        return static_cast<T>(q) < x;
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static bool binary_less_than(const T &x, const mp_rational &q)
    {
        return x < static_cast<T>(q);
    }
    // Greater-than operator.
    static bool binary_greater_than(const mp_rational &q1, const mp_rational &q2)
    {
        if (q1.m_den == q2.m_den) {
            return q1.m_num > q2.m_num;
        }
        return q1.num() * q2.den() > q2.num() * q1.den();
    }
    template <typename T, enable_if_t<disjunction<std::is_integral<T>, std::is_same<T, int_type>>::value, int> = 0>
    static bool binary_greater_than(const mp_rational &q, const T &x)
    {
        return q.num() > q.den() * x;
    }
    template <typename T, enable_if_t<disjunction<std::is_integral<T>, std::is_same<T, int_type>>::value, int> = 0>
    static bool binary_greater_than(const T &x, const mp_rational &q)
    {
        return q.den() * x > q.num();
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static bool binary_greater_than(const mp_rational &q, const T &x)
    {
        return static_cast<T>(q) > x;
    }
    template <typename T, enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static bool binary_greater_than(const T &x, const mp_rational &q)
    {
        return x > static_cast<T>(q);
    }
    // mpq view class.
    class mpq_view
    {
        using mpq_struct_t = std::remove_extent<::mpq_t>::type;

    public:
        explicit mpq_view(const mp_rational &q) : m_n_view(q.num().get_mpz_view()), m_d_view(q.den().get_mpz_view())
        {
            // Shallow copy over to m_mpq the data from the views.
            auto n_ptr = m_n_view.get();
            auto d_ptr = m_d_view.get();
            mpq_numref(&m_mpq)->_mp_alloc = n_ptr->_mp_alloc;
            mpq_numref(&m_mpq)->_mp_size = n_ptr->_mp_size;
            mpq_numref(&m_mpq)->_mp_d = n_ptr->_mp_d;
            mpq_denref(&m_mpq)->_mp_alloc = d_ptr->_mp_alloc;
            mpq_denref(&m_mpq)->_mp_size = d_ptr->_mp_size;
            mpq_denref(&m_mpq)->_mp_d = d_ptr->_mp_d;
        }
        mpq_view(const mpq_view &) = delete;
        mpq_view(mpq_view &&) = default;
        mpq_view &operator=(const mpq_view &) = delete;
        mpq_view &operator=(mpq_view &&) = delete;
        operator mpq_struct_t const *() const
        {
            return get();
        }
        mpq_struct_t const *get() const
        {
            return &m_mpq;
        }

    private:
        decltype(std::declval<const int_type &>().get_mpz_view()) m_n_view;
        decltype(std::declval<const int_type &>().get_mpz_view()) m_d_view;
        mpq_struct_t m_mpq;
    };
    // Pow enabler.
    template <typename T>
    using pow_enabler
        = enable_if_t<std::is_same<decltype(math::pow(std::declval<const int_type &>(), std::declval<const T &>())),
                                   int_type>::value,
                      int>;
    // Serialization support.
    friend class boost::serialization::access;
    template <class Archive>
    void save(Archive &ar, unsigned) const
    {
        piranha::boost_save(ar, m_num);
        piranha::boost_save(ar, m_den);
    }
    template <class Archive, enable_if_t<!std::is_same<Archive, boost::archive::binary_iarchive>::value, int> = 0>
    void load(Archive &ar, unsigned)
    {
        int_type num, den;
        piranha::boost_load(ar, num);
        piranha::boost_load(ar, den);
        // This ensures that if we load from a bad archive with non-coprime
        // num and den or negative den, or... we get anyway a canonicalised
        // rational or an error.
        *this = mp_rational{std::move(num), std::move(den)};
    }
    template <class Archive, enable_if_t<std::is_same<Archive, boost::archive::binary_iarchive>::value, int> = 0>
    void load(Archive &ar, unsigned)
    {
        int_type num, den;
        piranha::boost_load(ar, num);
        piranha::boost_load(ar, den);
        m_num = std::move(num);
        m_den = std::move(den);
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
public:
    /// Default constructor.
    /**
     * This constructor will initialise the rational to zero (that is, the numerator is set to zero, the denominator
     * to 1).
     */
    mp_rational() : m_num(), m_den(1)
    {
    }
    /// Copy constructor.
    /**
     * @param other the construction argument.
     */
    mp_rational(const mp_rational &other) = default;
    /// Move constructor.
    /**
     * @param other the construction argument.
     */
    mp_rational(mp_rational &&other) noexcept : m_num(std::move(other.m_num)), m_den(std::move(other.m_den))
    {
        // Fix the denominator of other, as its state depends on the implementation of piranha::mp_integer.
        // Set it to 1, so other will have a valid canonical state regardless of what happens
        // to the numerator.
        other.m_den = 1;
    }
    /// Constructor from numerator/denominator pair.
    /**
     * \note
     * This constructor is enabled only if \p I0 and \p I1 are either integral types or piranha::integer.
     *
     * @param n numerator.
     * @param d denominator.
     *
     * @throws piranha::zero_division_error if the denominator is zero.
     * @throws unspecified any exception thrown by the invoked constructor of piranha::mp_integer.
     */
    template <typename I0, typename I1, nd_ctor_enabler<I0, I1> = 0>
    explicit mp_rational(const I0 &n, const I1 &d) : m_num(n), m_den(d)
    {
        if (unlikely(m_den.sgn() == 0)) {
            piranha_throw(zero_division_error, "zero denominator");
        }
        canonicalise();
    }
    /// Generic constructor.
    /**
     * \note
     * This constructor is enabled only if \p T is an interoperable type.
     *
     * @param x object used to construct \p this.
     *
     * @throws std::invalid_argument if the construction fails (e.g., construction from a non-finite
     * floating-point value).
     */
    template <typename T, generic_ctor_enabler<T> = 0>
    explicit mp_rational(const T &x)
    {
        construct_from_interoperable(x);
    }
    /// Constructor from C string.
    /**
     * The string must represent either a valid single piranha::mp_integer, or two valid piranha::mp_integer
     * separated by "/". The rational will be put in canonical form by this constructor.
     *
     * Note that if the string is not null-terminated, undefined behaviour will occur.
     *
     * @param str C string used for construction.
     *
     * @throws std::invalid_argument if the string is not formatted correctly.
     * @throws piranha::zero_division_error if the denominator, if present, is zero.
     * @throws unspecified any exception thrown by the constructor from string of piranha::mp_integer
     * or by memory errors in \p std::string.
     */
    explicit mp_rational(const char *str) : m_num(), m_den(1)
    {
        auto ptr = str;
        std::size_t num_size = 0u;
        while (*ptr != '\0' && *ptr != '/') {
            ++num_size;
            ++ptr;
        }
        m_num = int_type(std::string(str, str + num_size));
        if (*ptr == '/') {
            m_den = int_type(std::string(ptr + 1u));
            if (unlikely(math::is_zero(m_den))) {
                piranha_throw(zero_division_error, "zero denominator");
            }
            canonicalise();
        }
    }
    /// Constructor from C++ string.
    /**
     * Equivalent to the constructor from C string.
     *
     * @param str C string used for construction.
     *
     * @throws unspecified any exception thrown by the constructor from C string.
     */
    explicit mp_rational(const std::string &str) : mp_rational(str.c_str())
    {
    }
    /// Destructor.
    ~mp_rational()
    {
        // NOTE: no checks no the numerator as we might mess it up
        // with the low-level methods.
        piranha_assert(m_den.sgn() > 0);
    }
    /// Copy assignment operator.
    /**
     * @param other the assignment argument.
     *
     * @return a reference to \p this.
     */
    mp_rational &operator=(const mp_rational &other) = default;
    /// Move assignment operator.
    /**
     * @param other the assignment argument.
     *
     * @return a reference to \p this.
     */
    mp_rational &operator=(mp_rational &&other) noexcept
    {
        if (unlikely(this == &other)) {
            return *this;
        }
        m_num = std::move(other.m_num);
        m_den = std::move(other.m_den);
        // See comments in the move ctor.
        other.m_den = 1;
        return *this;
    }
    /// Generic assignment operator.
    /**
     * \note
     * This assignment operator is enabled only if \p T is an interoperable type.
     *
     * This operator will construct a temporary piranha::mp_rational from \p x and will then move-assign it
     * to \p this.
     *
     * @param x assignment target.
     *
     * @return reference to \p this.
     *
     * @throws unspecified any exception thrown by the generic constructor from interoperable type.
     */
    template <typename T, generic_ctor_enabler<T> = 0>
    mp_rational &operator=(const T &x)
    {
        return (*this = mp_rational(x));
    }
    /// Assignment operator from C string.
    /**
     * This assignment operator will construct a piranha::mp_rational from the string \p str
     * and will then move-assign the result to \p this.
     *
     * @param str C string.
     *
     * @return reference to \p this.
     *
     * @throws unspecified any exception thrown by the constructor from string.
     */
    mp_rational &operator=(const char *str)
    {
        return (*this = mp_rational(str));
    }
    /// Assignment operator from C++ string.
    /**
     * This assignment operator will construct a piranha::mp_rational from the string \p str
     * and will then move-assign the result to \p this.
     *
     * @param str C++ string.
     *
     * @return reference to \p this.
     *
     * @throws unspecified any exception thrown by the constructor from string.
     */
    mp_rational &operator=(const std::string &str)
    {
        return (*this = str.c_str());
    }
    /// Stream operator.
    /**
     * The printing format is as follows:
     * - only the numerator is printed if the denominator is 1,
     * - otherwise, numerator and denominator are printed separated by a '/' sign.
     *
     * @param os target stream.
     * @param q rational to be printed.
     *
     * @return reference to \p os.
     *
     * @throws unspecified any exception thrown by the streaming operator of piranha::mp_integer.
     */
    friend std::ostream &operator<<(std::ostream &os, const mp_rational &q)
    {
        if (q.m_den.is_one()) {
            os << q.m_num;
        } else {
            os << q.m_num << '/' << q.m_den;
        }
        return os;
    }
    /// Overload input stream operator for piranha::mp_rational.
    /**
     * Equivalent to extracting a line from the stream and then assigning it to \p q.
     *
     * @param is input stream.
     * @param q rational to which the contents of the stream will be assigned.
     *
     * @return reference to \p is.
     *
     * @throws unspecified any exception thrown by the constructor from string of piranha::mp_rational.
     */
    friend std::istream &operator>>(std::istream &is, mp_rational &q)
    {
        std::string tmp_str;
        std::getline(is, tmp_str);
        q = tmp_str;
        return is;
    }
    /// Get const reference to the numerator.
    /**
     * @return a const reference to the numerator.
     */
    const int_type &num() const
    {
        return m_num;
    }
    /// Get const reference to the denominator.
    /**
     * @return a const reference to the denominator.
     */
    const int_type &den() const
    {
        return m_den;
    }
    /// Get an \p mpq view of \p this.
    /**
     * This method will return an object of an unspecified type \p mpq_view which is implicitly convertible
     * to a const pointer to an \p mpq struct (and which can thus be used as a <tt>const mpq_t</tt>
     * parameter in GMP functions). In addition to the implicit conversion operator, the \p mpq struct pointer
     * can also be retrieved via the <tt>get()</tt> method of the \p mpq_view class.
     * The pointee will represent a GMP rational whose value is equal to \p this.
     *
     * Note that the returned \p mpq_view instance can only be move-constructed (the other constructors and the
     * assignment operators
     * are disabled). Additionally, the returned object and the pointer might reference internal data belonging to
     * \p this, and they can thus be used safely only during the lifetime of \p this.
     * Any modification to \p this will also invalidate the view and the pointer.
     *
     * @return an \p mpq view of \p this.
     */
    mpq_view get_mpq_view() const
    {
        return mpq_view{*this};
    }
    /// Canonicality check.
    /**
     * A rational number is in canonical form when numerator and denominator
     * are coprime. A zero numerator must be paired to a 1 denominator.
     *
     * If low-level methods are not used, this function will always return \p true.
     *
     * @return \p true if \p this is in canonical form, \p false otherwise.
     */
    bool is_canonical() const
    {
        // NOTE: here the GCD only involves operations on mp_integers
        // and thus it never throws. The construction from 1 in the comparisons will
        // not throw either.
        // NOTE: there should be no way to set a negative denominator, so no check is performed.
        // The condition is checked in the dtor.
        const auto gcd = math::gcd(m_num, m_den);
        return (m_num.sgn() != 0 && (gcd == 1 || gcd == -1)) || (m_num.sgn() == 0 && m_den == 1);
    }
    /// Canonicalise.
    /**
     * This method will convert \p this to the canonical form, if needed.
     *
     * @see piranha::mp_rational::is_canonical().
     */
    void canonicalise()
    {
        // If the top is null, den must be one.
        if (math::is_zero(m_num)) {
            m_den = 1;
            return;
        }
        // NOTE: here we can avoid the further division by gcd if it is one or -one.
        // Consider this as a possible optimisation in the future.
        const int_type gcd = math::gcd(m_num, m_den);
        piranha_assert(!math::is_zero(gcd));
        divexact(m_num, m_num, gcd);
        divexact(m_den, m_den, gcd);
        // Fix mismatch in signs.
        if (m_den.sgn() == -1) {
            m_num.neg();
            m_den.neg();
        }
        // NOTE: this could be a nice place to use the demote() method of mp_integer.
    }
    /// Conversion operator.
    /**
     * \note
     * This operator is enabled only if \p T is an interoperable type.
     *
     * The conversion to piranha::mp_integer is computed by dividing the numerator by the denominator.
     * The conversion to integral types is computed by casting first to piranha::mp_integer, then to
     * the target integral type. The conversion to floating-point types might generate non-finite values.
     *
     * @return the value of \p this converted to type \p T.
     *
     * @throws std::overflow_error if the conversion fails (e.g., the range of the target integral type
     * is insufficient to represent the value of <tt>this</tt>).
     */
    template <typename T, cast_enabler<T> = 0>
    explicit operator T() const
    {
        return convert_to_impl<T>();
    }
    /** @name Low-level interface
     * Low-level methods. These methods allow construction from \p mpq_t and direct mutable access to numerator and
     * denominator, and they will not keep the rational in canonical form.
     */
    //@{
    /// Constructor from \p mpq_t.
    /**
     * This constructor will construct the numerator from the numerator of \p q,
     * the denominator from the denominator of \p q. This constructor assumes
     * that \p q is already in canonical form. If that is
     * not the case, the behaviour will be undefined.
     *
     * @param q input GMP rational.
     *
     * @throws piranha::zero_division_error if the denominator is zero.
     */
    explicit mp_rational(const ::mpq_t q) : m_num(mpq_numref(q)), m_den(mpq_denref(q))
    {
        if (unlikely(m_den.sgn() == 0)) {
            piranha_throw(zero_division_error, "zero denominator");
        }
    }
    /// Mutable reference to the numerator.
    /**
     * @return mutable reference to the numerator.
     */
    int_type &_num()
    {
        return m_num;
    }
    /// Mutable reference to the denominator.
    /**
     * @return mutable reference to the denominator.
     */
    int_type &_den()
    {
        return m_den;
    }
    /// Set denominator.
    /**
     * This method will set the denominator to \p den without canonicalising the rational.
     *
     * @param den desired value for the denominator.
     *
     * @throws std::invalid_argument if \p den is not positive.
     */
    void _set_den(const int_type &den)
    {
        if (unlikely(den.sgn() <= 0)) {
            piranha_throw(std::invalid_argument, "cannot set non-positive denominator in rational");
        }
        m_den = den;
    }
    //@}
    /// Identity operator.
    /**
     * @return a copy of \p this.
     */
    mp_rational operator+() const
    {
        return mp_rational{*this};
    }
    /// Pre-increment operator.
    /**
     * @return reference to \p this after the increment.
     *
     * @throws unspecified any exception thrown by in-place addition.
     */
    mp_rational &operator++()
    {
        return operator+=(1);
    }
    /// Post-increment operator.
    /**
     * @return copy of \p this before the increment.
     *
     * @throws unspecified any exception thrown by the pre-increment operator.
     */
    mp_rational operator++(int)
    {
        const mp_rational retval(*this);
        ++(*this);
        return retval;
    }
    /// In-place addition.
    /**
     * \note
     * This operator is enabled only if \p T is an interoperable type or piranha::mp_rational.
     *
     * If \p T is not a float, the exact result will be computed. If \p T is a floating-point type, the following
     * sequence of operations takes place:
     *
     * - \p this is converted to an instance \p f of type \p T via the conversion operator,
     * - \p f is added to \p x,
     * - the result is assigned back to \p this.
     *
     * @param x argument for the addition.
     *
     * @return reference to \p this.
     *
     * @throws unspecified any exception thrown by the conversion operator, the generic constructor of
     * piranha::mp_integer,
     * or the generic assignment operator, if used.
     */
    template <typename T>
    auto operator+=(const T &x) -> decltype(this->in_place_add(x))
    {
        return in_place_add(x);
    }
    /// Generic in-place addition with piranha::mp_rational.
    /**
     * \note
     * This operator is enabled only if \p T is a non-const interoperable type.
     *
     * Add a piranha::mp_rational in-place. This method will first compute <tt>q + x</tt>, cast it back to \p T via \p
     * static_cast and finally assign the result to \p x.
     *
     * @param x first argument.
     * @param q second argument.
     *
     * @return reference to \p x.
     *
     * @throws unspecified any exception thrown by the binary operator or by casting piranha::mp_rational to \p T.
     */
    template <typename T, generic_in_place_enabler<T> = 0>
    friend T &operator+=(T &x, const mp_rational &q)
    {
        return x = static_cast<T>(q + x);
    }
    /// Generic binary addition involving piranha::mp_rational.
    /**
     * \note
     * This template operator is enabled only if either:
     * - \p T is piranha::mp_rational and \p U is an interoperable type,
     * - \p U is piranha::mp_rational and \p T is an interoperable type,
     * - both \p T and \p U are piranha::mp_rational.
     *
     * If no floating-point types are involved, the exact result of the operation will be returned as a
     * piranha::mp_rational.
     *
     * If one of the arguments is a floating-point value \p f of type \p F, the other argument will be converted to an
     * instance of type \p F
     * and added to \p f to generate the return value, which will then be of type \p F.
     *
     * @param x first argument
     * @param y second argument.
     *
     * @return <tt>x + y</tt>.
     *
     * @throws unspecified any exception thrown by:
     * - the corresponding in-place operator,
     * - the invoked constructor or the conversion operator, if used.
     */
    template <typename T, typename U>
    friend auto operator+(const T &x, const U &y) -> decltype(mp_rational::binary_plus(x, y))
    {
        return mp_rational::binary_plus(x, y);
    }
    /// Negate in-place.
    void negate()
    {
        m_num.neg();
    }
    /// Negated copy.
    /**
     * @return a negated copy of \p this.
     */
    mp_rational operator-() const
    {
        mp_rational retval(*this);
        retval.negate();
        return retval;
    }
    /// Pre-decrement operator.
    /**
     * @return reference to \p this after the decrement.
     *
     * @throws unspecified any exception thrown by in-place subtraction.
     */
    mp_rational &operator--()
    {
        return operator-=(1);
    }
    /// Post-decrement operator.
    /**
     * @return copy of \p this before the decrement.
     *
     * @throws unspecified any exception thrown by the pre-decrement operator.
     */
    mp_rational operator--(int)
    {
        const mp_rational retval(*this);
        --(*this);
        return retval;
    }
    /// In-place subtraction.
    /**
     * \note
     * This operator is enabled only if \p T is an interoperable type or piranha::mp_rational.
     *
     * If \p T is not a float, the exact result will be computed. If \p T is a floating-point type, the following
     * sequence of operations takes place:
     *
     * - \p this is converted to an instance \p f of type \p T via the conversion operator,
     * - \p x is subtracted from \p f,
     * - the result is assigned back to \p this.
     *
     * @param x argument for the subtraction.
     *
     * @return reference to \p this.
     *
     * @throws unspecified any exception thrown by the conversion operator, the generic constructor of
     * piranha::mp_integer,
     * or the generic assignment operator, if used.
     */
    template <typename T>
    auto operator-=(const T &x) -> decltype(this->in_place_sub(x))
    {
        return in_place_sub(x);
    }
    /// Generic in-place subtraction with piranha::mp_rational.
    /**
     * \note
     * This operator is enabled only if \p T is a non-const interoperable type.
     *
     * Subtract a piranha::mp_rational in-place. This method will first compute <tt>x - q</tt>, cast it back to \p T via
     * \p static_cast and finally assign the result to \p x.
     *
     * @param x first argument.
     * @param q second argument.
     *
     * @return reference to \p x.
     *
     * @throws unspecified any exception thrown by the binary operator or by casting piranha::mp_rational to \p T.
     */
    template <typename T, generic_in_place_enabler<T> = 0>
    friend T &operator-=(T &x, const mp_rational &q)
    {
        return x = static_cast<T>(x - q);
    }
    /// Generic binary subtraction involving piranha::mp_rational.
    /**
     * \note
     * This template operator is enabled only if either:
     * - \p T is piranha::mp_rational and \p U is an interoperable type,
     * - \p U is piranha::mp_rational and \p T is an interoperable type,
     * - both \p T and \p U are piranha::mp_rational.
     *
     * If no floating-point types are involved, the exact result of the operation will be returned as a
     * piranha::mp_rational.
     *
     * If one of the arguments is a floating-point value \p f of type \p F, the other argument will be converted to an
     * instance of type \p F
     * and subtracted from (or to) \p f to generate the return value, which will then be of type \p F.
     *
     * @param x first argument
     * @param y second argument.
     *
     * @return <tt>x - y</tt>.
     *
     * @throws unspecified any exception thrown by:
     * - the corresponding in-place operator,
     * - the invoked constructor or the conversion operator, if used.
     */
    template <typename T, typename U>
    friend auto operator-(const T &x, const U &y) -> decltype(mp_rational::binary_minus(x, y))
    {
        return mp_rational::binary_minus(x, y);
    }
    /// In-place multiplication.
    /**
     * \note
     * This operator is enabled only if \p T is an interoperable type or piranha::mp_rational.
     *
     * If \p T is not a float, the exact result will be computed. If \p T is a floating-point type, the following
     * sequence of operations takes place:
     *
     * - \p this is converted to an instance \p f of type \p T via the conversion operator,
     * - \p f is multiplied by \p x,
     * - the result is assigned back to \p this.
     *
     * @param x argument for the multiplication.
     *
     * @return reference to \p this.
     *
     * @throws unspecified any exception thrown by the conversion operator, the generic constructor of
     * piranha::mp_integer,
     * or the generic assignment operator, if used.
     */
    template <typename T>
    auto operator*=(const T &x) -> decltype(this->in_place_mult(x))
    {
        return in_place_mult(x);
    }
    /// Generic in-place multiplication with piranha::mp_rational.
    /**
     * \note
     * This operator is enabled only if \p T is a non-const interoperable type.
     *
     * Multiply by a piranha::mp_rational in-place. This method will first compute <tt>x * q</tt>, cast it back to \p T
     * via \p static_cast and finally assign the result to \p x.
     *
     * @param x first argument.
     * @param q second argument.
     *
     * @return reference to \p x.
     *
     * @throws unspecified any exception thrown by the binary operator or by casting piranha::mp_rational to \p T.
     */
    template <typename T, generic_in_place_enabler<T> = 0>
    friend T &operator*=(T &x, const mp_rational &q)
    {
        return x = static_cast<T>(x * q);
    }
    /// Generic binary multiplication involving piranha::mp_rational.
    /**
     * \note
     * This template operator is enabled only if either:
     * - \p T is piranha::mp_rational and \p U is an interoperable type,
     * - \p U is piranha::mp_rational and \p T is an interoperable type,
     * - both \p T and \p U are piranha::mp_rational.
     *
     * If no floating-point types are involved, the exact result of the operation will be returned as a
     * piranha::mp_rational.
     *
     * If one of the arguments is a floating-point value \p f of type \p F, the other argument will be converted to an
     * instance of type \p F
     * and multiplied by \p f to generate the return value, which will then be of type \p F.
     *
     * @param x first argument
     * @param y second argument.
     *
     * @return <tt>x * y</tt>.
     *
     * @throws unspecified any exception thrown by:
     * - the corresponding in-place operator,
     * - the invoked constructor or the conversion operator, if used.
     */
    template <typename T, typename U>
    friend auto operator*(const T &x, const U &y) -> decltype(mp_rational::binary_mult(x, y))
    {
        return mp_rational::binary_mult(x, y);
    }
    /// In-place division.
    /**
     * \note
     * This operator is enabled only if \p T is an interoperable type or piranha::mp_rational.
     *
     * If \p T is not a float, the exact result will be computed. If \p T is a floating-point type, the following
     * sequence of operations takes place:
     *
     * - \p this is converted to an instance \p f of type \p T via the conversion operator,
     * - \p f is divided by \p x,
     * - the result is assigned back to \p this.
     *
     * @param x argument for the division.
     *
     * @return reference to \p this.
     *
     * @throws piranha::zero_division_error if \p x is zero.
     * @throws unspecified any exception thrown by the conversion operator, the generic constructor of
     * piranha::mp_integer,
     * or the generic assignment operator, if used.
     */
    template <typename T>
    auto operator/=(const T &x) -> decltype(this->in_place_div(x))
    {
        if (unlikely(math::is_zero(x))) {
            piranha_throw(zero_division_error, "division of a rational by zero");
        }
        return in_place_div(x);
    }
    /// Generic in-place division with piranha::mp_rational.
    /**
     * \note
     * This operator is enabled only if \p T is a non-const interoperable type.
     *
     * Divide by a piranha::mp_rational in-place. This method will first compute <tt>x / q</tt>, cast it back to \p T
     * via \p static_cast and finally assign the result to \p x.
     *
     * @param x first argument.
     * @param q second argument.
     *
     * @return reference to \p x.
     *
     * @throws unspecified any exception thrown by the binary operator or by casting piranha::mp_rational to \p T.
     */
    template <typename T, generic_in_place_enabler<T> = 0>
    friend T &operator/=(T &x, const mp_rational &q)
    {
        return x = static_cast<T>(x / q);
    }
    /// Generic binary division involving piranha::mp_rational.
    /**
     * \note
     * This template operator is enabled only if either:
     * - \p T is piranha::mp_rational and \p U is an interoperable type,
     * - \p U is piranha::mp_rational and \p T is an interoperable type,
     * - both \p T and \p U are piranha::mp_rational.
     *
     * If no floating-point types are involved, the exact result of the operation will be returned as a
     * piranha::mp_rational.
     *
     * If one of the arguments is a floating-point value \p f of type \p F, the other argument will be converted to an
     * instance of type \p F
     * and divided by \p f (or viceversa) to generate the return value, which will then be of type \p F.
     *
     * @param x first argument
     * @param y second argument.
     *
     * @return <tt>x / y</tt>.
     *
     * @throws piranha::zero_division_error in case of division by zero.
     * @throws unspecified any exception thrown by:
     * - the corresponding in-place operator,
     * - the invoked constructor or the conversion operator, if used.
     */
    template <typename T, typename U>
    friend auto operator/(const T &x, const U &y) -> decltype(mp_rational::binary_div(x, y))
    {
        return mp_rational::binary_div(x, y);
    }
    /// Generic equality operator involving piranha::mp_rational.
    /**
     * \note
     * This template operator is enabled only if either:
     * - \p T is piranha::mp_rational and \p U is an interoperable type,
     * - \p U is piranha::mp_rational and \p T is an interoperable type,
     * - both \p T and \p U are piranha::mp_rational.
     *
     * If no floating-point types are involved, the exact result of the comparison will be returned.
     *
     * If one of the arguments is a floating-point value \p f of type \p F, the other argument will be converted to an
     * instance of type \p F
     * and compared to \p f.
     *
     * @param x first argument
     * @param y second argument.
     *
     * @return \p true if <tt>x == y</tt>, \p false otherwise.
     *
     * @throws unspecified any exception thrown by:
     * - the comparison operator of piranha::mp_integer,
     * - the invoked conversion operator, if used.
     */
    template <typename T, typename U>
    friend auto operator==(const T &x, const U &y) -> decltype(mp_rational::binary_eq(x, y))
    {
        return mp_rational::binary_eq(x, y);
    }
    /// Generic inequality operator involving piranha::mp_rational.
    /**
     * \note
     * This template operator is enabled only if either:
     * - \p T is piranha::mp_rational and \p U is an interoperable type,
     * - \p U is piranha::mp_rational and \p T is an interoperable type,
     * - both \p T and \p U are piranha::mp_rational.
     *
     * If no floating-point types are involved, the exact result of the comparison will be returned.
     *
     * If one of the arguments is a floating-point value \p f of type \p F, the other argument will be converted to an
     * instance of type \p F
     * and compared to \p f.
     *
     * @param x first argument
     * @param y second argument.
     *
     * @return \p true if <tt>x != y</tt>, \p false otherwise.
     *
     * @throws unspecified any exception thrown by the equality operator.
     */
    template <typename T, typename U>
    friend auto operator!=(const T &x, const U &y) -> decltype(!mp_rational::binary_eq(x, y))
    {
        return !mp_rational::binary_eq(x, y);
    }
    /// Generic less-than operator involving piranha::mp_rational.
    /**
     * \note
     * This template operator is enabled only if either:
     * - \p T is piranha::mp_rational and \p U is an interoperable type,
     * - \p U is piranha::mp_rational and \p T is an interoperable type,
     * - both \p T and \p U are piranha::mp_rational.
     *
     * If no floating-point types are involved, the exact result of the comparison will be returned.
     *
     * If one of the arguments is a floating-point value \p f of type \p F, the other argument will be converted to an
     * instance of type \p F
     * and compared to \p f.
     *
     * @param x first argument
     * @param y second argument.
     *
     * @return \p true if <tt>x < y</tt>, \p false otherwise.
     *
     * @throws unspecified any exception thrown by:
     * - the less-than operator of piranha::mp_integer,
     * - the invoked conversion operator, if used.
     */
    template <typename T, typename U>
    friend auto operator<(const T &x, const U &y) -> decltype(mp_rational::binary_less_than(x, y))
    {
        return mp_rational::binary_less_than(x, y);
    }
    /// Generic greater-than or equal operator involving piranha::mp_rational.
    /**
     * \note
     * This template operator is enabled only if either:
     * - \p T is piranha::mp_rational and \p U is an interoperable type,
     * - \p U is piranha::mp_rational and \p T is an interoperable type,
     * - both \p T and \p U are piranha::mp_rational.
     *
     * If no floating-point types are involved, the exact result of the comparison will be returned.
     *
     * If one of the arguments is a floating-point value \p f of type \p F, the other argument will be converted to an
     * instance of type \p F
     * and compared to \p f.
     *
     * @param x first argument
     * @param y second argument.
     *
     * @return \p true if <tt>x >= y</tt>, \p false otherwise.
     *
     * @throws unspecified any exception thrown by the less-than operator.
     */
    template <typename T, typename U>
    friend auto operator>=(const T &x, const U &y) -> decltype(!mp_rational::binary_less_than(x, y))
    {
        return !mp_rational::binary_less_than(x, y);
    }
    /// Generic greater-than operator involving piranha::mp_rational.
    /**
     * \note
     * This template operator is enabled only if either:
     * - \p T is piranha::mp_rational and \p U is an interoperable type,
     * - \p U is piranha::mp_rational and \p T is an interoperable type,
     * - both \p T and \p U are piranha::mp_rational.
     *
     * If no floating-point types are involved, the exact result of the comparison will be returned.
     *
     * If one of the arguments is a floating-point value \p f of type \p F, the other argument will be converted to an
     * instance of type \p F
     * and compared to \p f.
     *
     * @param x first argument
     * @param y second argument.
     *
     * @return \p true if <tt>x > y</tt>, \p false otherwise.
     *
     * @throws unspecified any exception thrown by:
     * - the greater-than operator of piranha::mp_integer,
     * - the invoked conversion operator, if used.
     */
    template <typename T, typename U>
    friend auto operator>(const T &x, const U &y) -> decltype(mp_rational::binary_greater_than(x, y))
    {
        return mp_rational::binary_greater_than(x, y);
    }
    /// Generic less-than or equal operator involving piranha::mp_rational.
    /**
     * \note
     * This template operator is enabled only if either:
     * - \p T is piranha::mp_rational and \p U is an interoperable type,
     * - \p U is piranha::mp_rational and \p T is an interoperable type,
     * - both \p T and \p U are piranha::mp_rational.
     *
     * If no floating-point types are involved, the exact result of the comparison will be returned.
     *
     * If one of the arguments is a floating-point value \p f of type \p F, the other argument will be converted to an
     * instance of type \p F
     * and compared to \p f.
     *
     * @param x first argument
     * @param y second argument.
     *
     * @return \p true if <tt>x <= y</tt>, \p false otherwise.
     *
     * @throws unspecified any exception thrown by the greater-than operator.
     */
    template <typename T, typename U>
    friend auto operator<=(const T &x, const U &y) -> decltype(!mp_rational::binary_greater_than(x, y))
    {
        return !mp_rational::binary_greater_than(x, y);
    }
    /// Exponentiation.
    /**
     * \note
     * This method is enabled only if piranha::mp_rational::int_type can be raised to the power of \p exp, yielding
     * piranha::mp_rational::int_type as a result.
     *
     * This method computes \p this raised to the integral power \p exp. Internally, the piranha::math::pow()
     * function is used on numerator and denominator. Negative powers will raise an error if the numerator of \p this
     * is zero.
     *
     * @param exp exponent.
     *
     * @return <tt>this ** exp</tt>.
     *
     * @throws piranha::zero_division_error if \p exp is negative and the numerator of \p this is zero.
     * @throws unspecified any exception thrown by piranha::math::pow().
     */
    template <typename T, pow_enabler<T> = 0>
    mp_rational pow(const T &exp) const
    {
        mp_rational retval;
        if (exp >= T(0)) {
            // For non-negative exponents, we can just raw-construct
            // a rational value.
            // NOTE: in case of exceptions here we are good, the worst that can happen
            // is that the numerator has some value and den is still 1 from the initialisation.
            retval.m_num = math::pow(num(), exp);
            retval.m_den = math::pow(den(), exp);
        } else {
            if (unlikely(math::is_zero(num()))) {
                piranha_throw(zero_division_error, "zero denominator in rational exponentiation");
            }
            // For negative exponents, invert.
            const int_type n_exp = -int_type(exp);
            // NOTE: exception safe here as well.
            retval.m_num = math::pow(den(), n_exp);
            retval.m_den = math::pow(num(), n_exp);
            if (retval.m_den.sgn() < 0) {
                math::negate(retval.m_num);
                math::negate(retval.m_den);
            }
        }
        return retval;
    }
    /// Absolute value.
    /**
     * @return absolute value of \p this.
     */
    mp_rational abs() const
    {
        mp_rational retval{*this};
        if (retval.m_num.sgn() < 0) {
            retval.m_num.neg();
        }
        return retval;
    }
    /// Hash value.
    /**
     * The hash value is calculated by combining the hash values of numerator and denominator.
     *
     * @return a hash value for this.
     */
    std::size_t hash() const
    {
        std::size_t retval = std::hash<int_type>()(m_num);
        boost::hash_combine(retval, std::hash<int_type>()(m_den));
        return retval;
    }

private:
    // Generic binomial implementation.
    template <typename T, enable_if_t<std::is_unsigned<T>::value, int> = 0>
    static bool generic_binomial_check_k(const T &, const T &)
    {
        return false;
    }
    template <typename T, enable_if_t<!std::is_unsigned<T>::value, int> = 0>
    static bool generic_binomial_check_k(const T &k, const T &zero)
    {
        return k < zero;
    }
    // Generic binomial implementation using the falling factorial. U must be an integer
    // type, T can be anything that supports basic arithmetics. k must be non-negative.
    template <typename T, typename U>
    static T generic_binomial(const T &x, const U &k)
    {
        const U zero(0), one(1);
        if (generic_binomial_check_k(k, zero)) {
            piranha_throw(std::invalid_argument, "negative k value in binomial coefficient");
        }
        // Zero at bottom results always in 1.
        if (k == zero) {
            return T(1);
        }
        T tmp(x), retval = x / T(k);
        --tmp;
        for (auto i = static_cast<U>(k - one); i >= one; --i, --tmp) {
            retval *= tmp;
            retval /= T(i);
        }
        return retval;
    }

public:
    /// Binomial coefficient.
    /**
     * \note
     * This method is enabled only if \p T is an integral type or piranha::mp_integer.
     *
     * Will return \p this choose \p n.
     *
     * @param n bottom argument for the binomial coefficient.
     *
     * @return \p this choose \p n.
     *
     * @throws unspecified any exception thrown by piranha::mp_integer::binomial()
     * or by arithmetic operations on piranha::mp_rational.
     */
    template <typename T, enable_if_t<disjunction<std::is_integral<T>, std::is_same<T, int_type>>::value, int> = 0>
    mp_rational binomial(const T &n) const
    {
        if (m_den.is_one()) {
            // If this is an integer, offload to mp_integer::binomial().
            return mp_rational{math::binomial(m_num, n), 1};
        }
        if (n < T(0)) {
            // (rational negative-int) will always give zero.
            return mp_rational{};
        }
        // (rational non-negative-int) uses the generic implementation.
        // NOTE: this is going to be really slow, it can be improved by orders
        // of magnitude.
        return generic_binomial(*this, n);
    }

#if defined(PIRANHA_WITH_MSGPACK)
private:
    template <typename Stream>
    using msgpack_pack_enabler
        = enable_if_t<conjunction<is_msgpack_stream<Stream>, has_msgpack_pack<Stream, int_type>>::value, int>;
    template <typename U>
    using msgpack_convert_enabler = enable_if_t<has_msgpack_convert<typename U::int_type>::value, int>;

public:
    /// Pack in msgpack format.
    /**
     * \note
     * This method is enabled only if \p Stream satisfies piranha::is_msgpack_stream and the type representing the
     * numerator and denominator satisfies piranha::has_msgpack_pack.
     *
     * This method will pack \p this into \p p. Rationals are packed as a numerator-denominator pairs.
     *
     * @param p the target <tt>msgpack::packer</tt>.
     * @param f the desired piranha::msgpack_format.
     *
     * @throws unspecified any exception thrown by:
     * - the public interface of <tt>msgpack::packer</tt>,
     * - piranha::msgpack_pack().
     */
    template <typename Stream, msgpack_pack_enabler<Stream> = 0>
    void msgpack_pack(msgpack::packer<Stream> &p, msgpack_format f) const
    {
        p.pack_array(2u);
        piranha::msgpack_pack(p, m_num, f);
        piranha::msgpack_pack(p, m_den, f);
    }
    /// Convert from msgpack object.
    /**
     * \note
     * This method is enabled only if the type representing the
     * numerator and denominator satisfies piranha::has_msgpack_convert.
     *
     * This method will convert \p o into \p this. If \p f is piranha::msgpack_format::portable
     * this method will check that the deserialized rational is in canonical form before assigning it to \p this,
     * otherwise no check will be performed and the behaviour will be undefined if the deserialized denominator is zero.
     *
     * @param o the source <tt>msgpack::object</tt>.
     * @param f the desired piranha::msgpack_format.
     *
     * @throws unspecified any exception thrown by:
     * - the public interface of <tt>msgpack::object</tt>,
     * - piranha::msgpack_convert(),
     * - the constructor of piranha::mp_rational from numerator and denominator.
     */
    template <typename U = mp_rational, msgpack_convert_enabler<U> = 0>
    void msgpack_convert(const msgpack::object &o, msgpack_format f)
    {
        std::array<msgpack::object, 2u> v;
        o.convert(v);
        int_type num, den;
        piranha::msgpack_convert(num, v[0], f);
        piranha::msgpack_convert(den, v[1], f);
        if (f == msgpack_format::binary) {
            m_num = std::move(num);
            m_den = std::move(den);
        } else {
            *this = mp_rational{std::move(num), std::move(den)};
        }
    }
#endif

private:
    int_type m_num;
    int_type m_den;
};

/// Alias for piranha::mp_rational with 1 static limb.
using rational = mp_rational<1>;

inline namespace literals
{

/// Literal for arbitrary-precision rationals.
/**
 * @param s literal string.
 *
 * @return a piranha::mp_rational constructed from \p s.
 *
 * @throws unspecified any exception thrown by the constructor of
 * piranha::mp_rational from string.
 */
inline rational operator"" _q(const char *s)
{
    return rational(s);
}
}

inline namespace impl
{

// TMP structure to detect mp_rational types.
template <typename>
struct is_mp_rational : std::false_type {
};

template <std::size_t SSize>
struct is_mp_rational<mp_rational<SSize>> : std::true_type {
};

// Detect if T and U are both mp_rational with same SSize.
template <typename, typename>
struct is_same_mp_rational : std::false_type {
};

template <std::size_t SSize>
struct is_same_mp_rational<mp_rational<SSize>, mp_rational<SSize>> : std::true_type {
};

// Detect if type T is an interoperable type for the mp_rational type Rational.
// NOTE: we need to split this in 2 as we might be using this in a context in which
// Rational is not an mp_rational, in which case we cannot and don't need to check
// against the inner int type.
template <typename T, typename Rational>
struct is_mp_rational_interoperable_type : mppp::mppp_impl::is_supported_interop<T> {
};

template <typename T, std::size_t SSize>
struct is_mp_rational_interoperable_type<T, mp_rational<SSize>>
    : disjunction<mppp::mppp_impl::is_supported_interop<T>, std::is_same<T, typename mp_rational<SSize>::int_type>> {
};
}

/// Specialisation of the piranha::print_tex_coefficient() functor for piranha::mp_rational.
template <std::size_t SSize>
struct print_tex_coefficient_impl<mp_rational<SSize>> {
    /// Call operator.
    /**
     * @param os target stream.
     * @param cf coefficient to be printed.
     *
     * @throws unspecified any exception thrown by streaming piranha::mp_integer to \p os.
     */
    void operator()(std::ostream &os, const mp_rational<SSize> &cf) const
    {
        if (math::is_zero(cf.num())) {
            os << "0";
            return;
        }
        if (cf.den().is_one()) {
            os << cf.num();
            return;
        }
        auto num = cf.num();
        if (num.sgn() < 0) {
            os << "-";
            num.neg();
        }
        os << "\\frac{" << num << "}{" << cf.den() << "}";
    }
};

namespace math
{

/// Specialisation of the implementation of piranha::math::is_zero() for piranha::mp_rational.
template <std::size_t SSize>
struct is_zero_impl<mp_rational<SSize>> {
    /// Call operator.
    /**
     * @param q piranha::mp_rational to be tested.
     *
     * @return \p true if \p q is zero, \p false otherwise.
     */
    bool operator()(const mp_rational<SSize> &q) const
    {
        return is_zero(q.num());
    }
};

/// Specialisation of the implementation of piranha::math::is_unitary() for piranha::mp_rational.
template <std::size_t SSize>
struct is_unitary_impl<mp_rational<SSize>> {
    /// Call operator.
    /**
     * @param q piranha::mp_rational to be tested.
     *
     * @return \p true if \p q is equal to one, \p false otherwise.
     */
    bool operator()(const mp_rational<SSize> &q) const
    {
        return is_unitary(q.num()) && is_unitary(q.den());
    }
};

/// Specialisation of the implementation of piranha::math::negate() for piranha::mp_rational.
template <std::size_t SSize>
struct negate_impl<mp_rational<SSize>> {
    /// Call operator.
    /**
     * @param q piranha::mp_rational to be negated.
     */
    void operator()(mp_rational<SSize> &q) const
    {
        q.negate();
    }
};
}

inline namespace impl
{

// Enabler for the pow specialisation.
template <typename T, typename U>
using math_rational_pow_enabler
    = enable_if_t<disjunction<conjunction<is_mp_rational<T>, is_mp_rational_interoperable_type<U, T>>,
                              conjunction<is_mp_rational<U>, is_mp_rational_interoperable_type<T, U>>,
                              is_same_mp_rational<T, U>>::value>;
}

namespace math
{

/// Specialisation of the implementation of piranha::math::pow() for piranha::mp_rational.
/**
 * This specialisation is activated when one of the arguments is piranha::mp_rational
 * and the other is either piranha::mp_rational or an interoperable type for piranha::mp_rational.
 *
 * The implementation follows these rules:
 * - if the base is rational and the exponent an integral type or piranha::mp_integer, then
 *   piranha::mp_rational::pow() is used;
 * - if the non-rational argument is a floating-point type, then the rational argument is converted
 *   to that floating-point type and piranha::math::pow() is used;
 * - if both arguments are rational the result will be rational and the success of the operation depends
 *   on the values of the operands;
 * - if the base is an integral type or piranha::mp_integer and the exponent a rational, the result is computed
 *   via the integral--integer specialisation of piranha::math::pow() and the the success of the operation depends
 *   on the values of the operands.
 */
template <typename T, typename U>
struct pow_impl<T, U, math_rational_pow_enabler<T, U>> {
private:
    template <std::size_t SSize, typename T2>
    static auto impl(const mp_rational<SSize> &b, const T2 &e) -> decltype(b.pow(e))
    {
        return b.pow(e);
    }
    template <std::size_t SSize, typename T2, enable_if_t<std::is_floating_point<T2>::value, int> = 0>
    static T2 impl(const mp_rational<SSize> &b, const T2 &e)
    {
        return math::pow(static_cast<T2>(b), e);
    }
    template <std::size_t SSize, typename T2, enable_if_t<std::is_floating_point<T2>::value, int> = 0>
    static T2 impl(const T2 &e, const mp_rational<SSize> &b)
    {
        return math::pow(e, static_cast<T2>(b));
    }
    template <std::size_t SSize>
    static mp_rational<SSize> impl(const mp_rational<SSize> &b, const mp_rational<SSize> &e)
    {
        // Special casing.
        if (is_unitary(b)) {
            return b;
        }
        if (is_zero(b)) {
            const auto sign = e.num().sgn();
            if (sign > 0) {
                // 0**q = 1
                return mp_rational<SSize>(0);
            }
            if (sign == 0) {
                // 0**0 = 1
                return mp_rational<SSize>(1);
            }
            // 0**-q -> division by zero.
            piranha_throw(zero_division_error, "unable to raise zero to a negative power");
        }
        if (!e.den().is_one()) {
            piranha_throw(std::invalid_argument,
                          "unable to raise rational to a rational power whose denominator is not 1");
        }
        return b.pow(e.num());
    }
    template <std::size_t SSize, typename T2,
              enable_if_t<disjunction<std::is_integral<T2>, is_mp_integer<T2>>::value, int> = 0>
    static auto impl(const T2 &b, const mp_rational<SSize> &e) -> decltype(math::pow(b, e.num()))
    {
        using ret_type = decltype(math::pow(b, e.num()));
        if (is_unitary(b)) {
            return ret_type(b);
        }
        if (is_zero(b)) {
            const auto sign = e.num().sgn();
            if (sign > 0) {
                return ret_type(0);
            }
            if (sign == 0) {
                return ret_type(1);
            }
            piranha_throw(zero_division_error, "unable to raise zero to a negative power");
        }
        if (!e.den().is_one()) {
            piranha_throw(std::invalid_argument,
                          "unable to raise an integral to a rational power whose denominator is not 1");
        }
        return math::pow(b, e.num());
    }
    using ret_type = decltype(impl(std::declval<const T &>(), std::declval<const U &>()));

public:
    /// Call operator.
    /**
     * @param b base.
     * @param e exponent.
     *
     * @returns <tt>b**e</tt>.
     *
     * @throws std::invalid_argument if the result cannot be computed.
     * @throws unspecified any exception thrown by:
     * - piranha::math::pow(),
     * - piranha::mp_rational::pow(),
     * - converting piranha::mp_rational to a floating-point type.
     */
    ret_type operator()(const T &b, const U &e) const
    {
        return impl(b, e);
    }
};

/// Specialisation of the implementation of piranha::math::sin() for piranha::mp_rational.
template <std::size_t SSize>
struct sin_impl<mp_rational<SSize>> {
    /// Call operator.
    /**
     * @param q argument.
     *
     * @return sine of \p q.
     *
     * @throws std::invalid_argument if the argument is not zero.
     */
    mp_rational<SSize> operator()(const mp_rational<SSize> &q) const
    {
        if (is_zero(q)) {
            return mp_rational<SSize>(0);
        }
        piranha_throw(std::invalid_argument, "cannot compute the sine of a non-zero rational");
    }
};

/// Specialisation of the implementation of piranha::math::cos() for piranha::mp_rational.
template <std::size_t SSize>
struct cos_impl<mp_rational<SSize>> {
    /// Call operator.
    /**
     * @param q argument.
     *
     * @return cosine of \p q.
     *
     * @throws std::invalid_argument if the argument is not zero.
     */
    mp_rational<SSize> operator()(const mp_rational<SSize> &q) const
    {
        if (is_zero(q)) {
            return mp_rational<SSize>(1);
        }
        piranha_throw(std::invalid_argument, "cannot compute the cosine of a non-zero rational");
    }
};

/// Specialisation of the implementation of piranha::math::abs() for piranha::mp_rational.
template <std::size_t SSize>
struct abs_impl<mp_rational<SSize>> {
    /// Call operator.
    /**
     * @param q input parameter.
     *
     * @return absolute value of \p q.
     */
    mp_rational<SSize> operator()(const mp_rational<SSize> &q) const
    {
        return q.abs();
    }
};

/// Specialisation of the implementation of piranha::math::partial() for piranha::mp_rational.
template <std::size_t SSize>
struct partial_impl<mp_rational<SSize>> {
    /// Call operator.
    /**
     * @return an instance of piranha::mp_rational constructed from zero.
     */
    mp_rational<SSize> operator()(const mp_rational<SSize> &, const std::string &) const
    {
        return mp_rational<SSize>{};
    }
};
}

inline namespace impl
{

// Binomial follows the same rules as pow.
template <typename T, typename U>
using math_rational_binomial_enabler = math_rational_pow_enabler<T, U>;
}

namespace math
{

/// Specialisation of the implementation of piranha::math::binomial() for piranha::mp_rational.
/**
 * This specialisation is activated when one of the arguments is piranha::mp_rational and the other is either
 * piranha::mp_rational or an interoperable type for piranha::mp_rational.
 *
 * The implementation follows these rules:
 * - if the top is rational and the bottom an integral type or piranha::mp_integer, then
 *   piranha::mp_rational::binomial() is used;
 * - if the non-rational argument is a floating-point type, then the rational argument is converted
 *   to that floating-point type and piranha::math::binomial() is used;
 * - if both arguments are rational, they are both converted to \p double and then piranha::math::binomial()
 *   is used;
 * - if the top is an integral type or piranha::mp_integer and the bottom a rational, then both
 *   arguments are converted to \p double and piranha::math::binomial() is used.
 */
template <typename T, typename U>
struct binomial_impl<T, U, math_rational_binomial_enabler<T, U>> {
private:
    template <std::size_t SSize, typename T2>
    static auto impl(const mp_rational<SSize> &x, const T2 &y) -> decltype(x.binomial(y))
    {
        return x.binomial(y);
    }
    template <std::size_t SSize, typename T2, enable_if_t<std::is_floating_point<T2>::value, int> = 0>
    static T2 impl(const mp_rational<SSize> &x, const T2 &y)
    {
        return math::binomial(static_cast<T2>(x), y);
    }
    template <std::size_t SSize, typename T2, enable_if_t<std::is_floating_point<T2>::value, int> = 0>
    static T2 impl(const T2 &x, const mp_rational<SSize> &y)
    {
        return math::binomial(x, static_cast<T2>(y));
    }
    template <std::size_t SSize>
    static double impl(const mp_rational<SSize> &x, const mp_rational<SSize> &y)
    {
        return math::binomial(static_cast<double>(x), static_cast<double>(y));
    }
    template <std::size_t SSize, typename T2,
              enable_if_t<disjunction<std::is_integral<T2>, is_mp_integer<T2>>::value, int> = 0>
    static double impl(const T2 &x, const mp_rational<SSize> &y)
    {
        return math::binomial(static_cast<double>(x), static_cast<double>(y));
    }
    using ret_type = decltype(impl(std::declval<const T &>(), std::declval<const U &>()));

public:
    /// Call operator.
    /**
     * @param x top argument.
     * @param y bottom argument.
     *
     * @returns \f$ x \choose y \f$.
     *
     * @throws unspecified any exception thrown by:
     * - piranha::mp_rational::binomial(),
     * - converting piranha::mp_rational or piranha::mp_integer to a floating-point type.
     */
    ret_type operator()(const T &x, const U &y) const
    {
        return impl(x, y);
    }
};
}

inline namespace impl
{

template <typename To, typename From>
using sc_rat_enabler
    = enable_if_t<disjunction<conjunction<is_mp_rational<To>,
                                          disjunction<std::is_arithmetic<From>, is_mp_integer<From>>>,
                              conjunction<is_mp_rational<From>,
                                          disjunction<std::is_integral<To>, is_mp_integer<To>>>>::value>;
}

/// Specialisation of piranha::safe_cast() for conversions involving piranha::mp_rational.
/**
 * \note
 * This specialisation is enabled in the following cases:
 * - \p To is a rational type and \p From is either an arithmetic type or piranha::mp_integer,
 * - \p To is an integral type or piranha::mp_integer, and \p From is piranha::mp_rational.
 */
template <typename To, typename From>
struct safe_cast_impl<To, From, sc_rat_enabler<To, From>> {
private:
    template <typename T, enable_if_t<disjunction<std::is_arithmetic<T>, is_mp_integer<T>>::value, int> = 0>
    static To impl(const T &x)
    {
        try {
            // NOTE: checks for finiteness of an fp value are in the ctor.
            return To(x);
        } catch (const std::invalid_argument &) {
            piranha_throw(safe_cast_failure, "cannot convert value " + boost::lexical_cast<std::string>(x)
                                                 + " of type '" + detail::demangle<T>()
                                                 + "' to a rational, as the conversion would not preserve the value");
        }
    }
    template <typename T, enable_if_t<is_mp_rational<T>::value, int> = 0>
    static To impl(const T &q)
    {
        if (unlikely(!q.den().is_one())) {
            piranha_throw(safe_cast_failure, "cannot convert the rational value " + boost::lexical_cast<std::string>(q)
                                                 + " to the integral type '" + detail::demangle<To>()
                                                 + "', as the rational value as non-unitary denominator");
        }
        try {
            return static_cast<To>(q);
        } catch (const std::overflow_error &) {
            piranha_throw(safe_cast_failure, "cannot convert the rational value " + boost::lexical_cast<std::string>(q)
                                                 + " to the integral type '" + detail::demangle<To>()
                                                 + "', as the conversion cannot preserve the value");
        }
    }

public:
    /// Call operator.
    /**
     * The conversion is performed via piranha::mp_rational's constructor and conversion operator.
     *
     * @param x input value.
     *
     * @return \p x converted to \p To.
     *
     * @throws piranha::safe_cast_failure if the conversion fails.
     */
    To operator()(const From &x) const
    {
        return impl(x);
    }
};

inline namespace impl
{

template <typename Archive, std::size_t SSize>
using mp_rational_boost_save_enabler
    = enable_if_t<has_boost_save<Archive, typename mp_rational<SSize>::int_type>::value>;

template <typename Archive, std::size_t SSize>
using mp_rational_boost_load_enabler
    = enable_if_t<has_boost_load<Archive, typename mp_rational<SSize>::int_type>::value>;
}

/// Specialisation of piranha::boost_save() for piranha::mp_rational.
/**
 * \note
 * This specialisation is enabled only if the numerator/denominator type of piranha::mp_rational satisfies
 * piranha::has_boost_save.
 *
 * The rational will be serialized as a numerator/denominator pair.
 *
 * @throws unspecified any exception thrown by piranha::boost_save().
 */
template <typename Archive, std::size_t SSize>
struct boost_save_impl<Archive, mp_rational<SSize>, mp_rational_boost_save_enabler<Archive, SSize>>
    : boost_save_via_boost_api<Archive, mp_rational<SSize>> {
};

/// Specialisation of piranha::boost_load() for piranha::mp_rational.
/**
 * \note
 * This specialisation is enabled only if the numerator/denominator type of piranha::mp_rational satisfies
 * piranha::has_boost_load.
 *
 * If \p Archive is boost::archive::binary_iarchive, the serialized numerator/denominator pair is loaded
 * as-is, without canonicality checks. Otherwise, the rational will be canonicalised after deserialization.
 *
 * @throws unspecified any exception thrown by piranha::boost_load() or by the constructor of piranha::mp_rational
 * from numerator and denominator.
 */
template <typename Archive, std::size_t SSize>
struct boost_load_impl<Archive, mp_rational<SSize>, mp_rational_boost_load_enabler<Archive, SSize>>
    : boost_load_via_boost_api<Archive, mp_rational<SSize>> {
};

#if defined(PIRANHA_WITH_MSGPACK)

inline namespace impl
{

template <typename Stream, typename T>
using mp_rational_msgpack_pack_enabler
    = enable_if_t<conjunction<is_mp_rational<T>, is_detected<msgpack_pack_member_t, Stream, T>>::value>;

template <typename T>
using mp_rational_msgpack_convert_enabler
    = enable_if_t<conjunction<is_mp_rational<T>, is_detected<msgpack_convert_member_t, T>>::value>;
}

/// Specialisation of piranha::msgpack_pack() for piranha::mp_rational.
/**
 * \note
 * This specialisation is enabled only if \p T is an instance of piranha::mp_rational supporting the
 * piranha::mp_rational::msgpack_pack() method.
 */
template <typename Stream, typename T>
struct msgpack_pack_impl<Stream, T, mp_rational_msgpack_pack_enabler<Stream, T>> {
    /// Call operator.
    /**
     * The call operator will use the piranha::mp_rational::msgpack_pack() method of \p q.
     *
     * @param p the source <tt>msgpack::packer</tt>.
     * @param q the input rational.
     * @param f the desired piranha::msgpack_format.
     *
     * @throws unspecified any exception thrown by piranha::mp_rational::msgpack_pack().
     */
    void operator()(msgpack::packer<Stream> &p, const T &q, msgpack_format f) const
    {
        q.msgpack_pack(p, f);
    }
};

/// Specialisation of piranha::msgpack_convert() for piranha::mp_rational.
/**
 * \note
 * This specialisation is enabled only if \p T is an instance of piranha::mp_rational supporting the
 * piranha::mp_rational::msgpack_convert() method.
 */
template <typename T>
struct msgpack_convert_impl<T, mp_rational_msgpack_convert_enabler<T>> {
    /// Call operator.
    /**
     * The call operator will use the piranha::mp_rational::msgpack_convert() method of \p q.
     *
     * @param q the target rational.
     * @param o the source <tt>msgpack::object</tt>.
     * @param f the desired piranha::msgpack_format.
     *
     * @throws unspecified any exception thrown by piranha::mp_rational::msgpack_convert().
     */
    void operator()(T &q, const msgpack::object &o, msgpack_format f) const
    {
        q.msgpack_convert(o, f);
    }
};

#endif
}

namespace std
{

/// Specialisation of \p std::hash for piranha::mp_rational.
template <std::size_t SSize>
struct hash<piranha::mp_rational<SSize>> {
    /// Result type.
    typedef size_t result_type;
    /// Argument type.
    typedef piranha::mp_rational<SSize> argument_type;
    /// Hash operator.
    /**
     * @param q piranha::mp_rational whose hash value will be returned.
     *
     * @return <tt>q.hash()</tt>.
     *
     * @see piranha::mp_rational::hash()
     */
    result_type operator()(const argument_type &q) const
    {
        return q.hash();
    }
};
}

#endif
