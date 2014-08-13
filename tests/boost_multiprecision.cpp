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

#include "../src/boost_multiprecision.hpp"

#define BOOST_TEST_MODULE boost_multiprecision_test
#include <boost/test/unit_test.hpp>

#include "../src/environment.hpp"
#include "../src/math.hpp"
#include "../src/type_traits.hpp"

using namespace piranha;

BOOST_AUTO_TEST_CASE(bmp_io_test)
{
	environment env;
	bmp_static_float<33> x = 1.17f;
	std::cout << x << '\n';
	x = 1.17;
	std::cout << x << '\n';
	x = 1.17l;
	std::cout << x << '\n';
	math::multiply_accumulate(x,x,x);
	std::cout << x << '\n';
	std::cout << is_cf<bmp_static_float<33>>::value << '\n';
}
