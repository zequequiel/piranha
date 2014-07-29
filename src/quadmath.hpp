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

#ifndef PIRANHA_QUADMATH_HPP
#define PIRANHA_QUADMATH_HPP

// Need to include this first in order to check whether quadmath
// support was enabled.
#include "config.hpp"

#if defined(PIRANHA_HAVE_QUADMATH)

#include <iostream>
#include <quadmath.h>
#include <stdexcept>

#include "exceptions.hpp"

inline __float128 operator "" _q(const char *s)
{
	return ::strtoflt128(s,nullptr);
}

inline std::ostream &operator<<(std::ostream &os, const __float128 &x)
{
	// Plenty of buffer.
	char buf[128u];
	// Check that our assumption is correct. This should be converted to
	// a string and passed into the format string really, but for now this
	// will do.
	static_assert(33 == FLT128_DIG,"Invalid value for FLT128_DIG.");
	// NOTE: here we use 32 because for printf this is the number of digits past the decimal point,
	// wherease FLT128_DIG is the number of total digits in the mantissa.
	const int retval = ::quadmath_snprintf(buf,sizeof(buf),"%.32Qe",x);
	if (unlikely(retval < 0)) {
		piranha_throw(std::invalid_argument,"quadmath_snprintf() returned an error");
	}
	if (unlikely(static_cast<unsigned>(retval) >= sizeof(buf))) {
		piranha_throw(std::invalid_argument,"quadmath_snprintf() returned a truncated output");
	}
	os << buf;
	return os;
}

#endif

#endif
