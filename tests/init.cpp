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

#include <piranha/init.hpp>

#define BOOST_TEST_MODULE init_test
#include <boost/test/included/unit_test.hpp>

#include <piranha/config.hpp>
#include <piranha/settings.hpp>
#include <piranha/thread_pool.hpp>

using namespace piranha;

struct dummy {
    ~dummy()
    {
        // NOTE: cannot use BOOST_CHECK here because this gets invoked outside the test case.
        piranha_assert(shutdown());
    }
};

static dummy d;

BOOST_AUTO_TEST_CASE(init_main_test)
{
    settings::set_n_threads(3);
    // Multiple concurrent constructions.
    auto f0 = thread_pool::enqueue(0, []() { init(); });
    auto f1 = thread_pool::enqueue(1, []() { init(); });
    auto f2 = thread_pool::enqueue(2, []() { init(); });
    f0.wait();
    f1.wait();
    f2.wait();
    BOOST_CHECK(!shutdown());
    BOOST_CHECK_EQUAL(piranha_init_statics<>::s_failed.load(), 2u);
}
