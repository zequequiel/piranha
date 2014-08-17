/***************************************************************************
 *   Copyright (C) 2009-2011 by Francesco Biscani                          *
 *   bluescarni@gmail.com                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef PIRANHA_BOOST_MULTIPRECISION_HPP
#define PIRANHA_BOOST_MULTIPRECISION_HPP

#include "config.hpp"

#if defined(PIRANHA_HAVE_BOOST_MULTIPRECISION)

#include <boost/multiprecision/mpfr.hpp>
#include <type_traits>

#include "detail/mpfr.hpp"
#include "math.hpp"
#include "type_traits.hpp"

namespace piranha
{

/// Shortcut for the <tt>boost::multiprecision</tt> namespace.
namespace bmp = boost::multiprecision;

namespace detail
{

template <unsigned Digits10>
struct static_float_def
{
	static_assert(Digits10 > 0u,"Invalid number of digits.");
	using type = bmp::number<bmp::mpfr_float_backend<Digits10,bmp::allocate_stack>,bmp::et_off>;
};

template <typename T>
struct is_bmp_float
{
	static const bool value = false;
};

template <unsigned Digits10, bmp::mpfr_allocation_type Alloc>
struct is_bmp_float<bmp::number<bmp::mpfr_float_backend<Digits10,Alloc>,bmp::et_off>>
{
	static const bool value = true;
};

}

template <typename T>
struct enable_noexcept_checks<T,typename std::enable_if<detail::is_bmp_float<T>::value>::type>
{
	static const bool value = false;
};

template <typename T>
const bool enable_noexcept_checks<T,typename std::enable_if<detail::is_bmp_float<T>::value>::type>::value;

template <unsigned Digits10>
using bmp_static_float = typename detail::static_float_def<Digits10>::type;

using bmp_float = bmp::number<bmp::mpfr_float_backend<0u>,bmp::et_off>;

namespace math
{

template <typename T>
struct multiply_accumulate_impl<T,T,T,typename std::enable_if<detail::is_bmp_float<T>::value>::type>
{
	T &operator()(T &x, const T &y, const T &z) const noexcept
	{
		// NOTE: here same reasoning as in real.multiply_accumulate().
		// ::mpfr_fma(x.backend().data(),y.backend().data(),z.backend().data(),x.backend().data(),MPFR_RNDN);
		static thread_local T tmp;
		::mpfr_mul(tmp.backend().data(),y.backend().data(),z.backend().data(),MPFR_RNDN);
		::mpfr_add(x.backend().data(),x.backend().data(),tmp.backend().data(),MPFR_RNDN);
		return x;
	}
};

template <typename T>
struct is_zero_impl<T,typename std::enable_if<detail::is_bmp_float<T>::value>::type>
{
	bool operator()(const T &x) const noexcept
	{
		return mpfr_sgn(x.backend().data()) == 0;
	}
};

template <typename T>
struct negate_impl<T,typename std::enable_if<detail::is_bmp_float<T>::value>::type>
{
	void operator()(T &x) const noexcept
	{
		::mpfr_neg(x.backend().data(),x.backend().data(),MPFR_RNDN);
	}
};

}

}

#endif

#endif
