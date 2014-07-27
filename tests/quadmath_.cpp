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

#include "../src/quadmath.hpp"

#define BOOST_TEST_MODULE quadmath_test
#include <boost/test/unit_test.hpp>

#include "../src/environment.hpp"
#include "../src/math.hpp"

using namespace piranha;

BOOST_AUTO_TEST_CASE(quadmath_io_test)
{
	environment env;
	__float128 x = 1.3_Q;
	__float128 y = 1.3f;
	__float128 z = 1.3l;
	std::cout << x << '\n';
	std::cout << y << '\n';
	std::cout << z << '\n';
}

BOOST_AUTO_TEST_CASE(quadmath_math_test)
{
	__float128 a, b, c;
	a = 0.5_Q;
	b = 1.5_q;
	c = 2.5_Q;
	math::multiply_accumulate(a,b,c);
	BOOST_CHECK(a == 4.25_q);
}
