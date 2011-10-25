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

#include "../src/type_traits.hpp"

#define BOOST_TEST_MODULE type_traits_test
#include <boost/test/unit_test.hpp>

#include <string>
#include <tuple>
#include <type_traits>

#include "../src/base_term.hpp"
#include "../src/config.hpp"
#include "../src/integer.hpp"
#include "../src/monomial.hpp"

using namespace piranha;

PIRANHA_DECLARE_HAS_TYPEDEF(foo_type);

struct foo
{
	typedef int foo_type;
};

struct bar {};

BOOST_AUTO_TEST_CASE(type_traits_has_typedef_test)
{
	BOOST_CHECK(has_typedef_foo_type<foo>::value);
	BOOST_CHECK(!has_typedef_foo_type<bar>::value);
}

BOOST_AUTO_TEST_CASE(type_traits_is_cv_ref)
{
	BOOST_CHECK_EQUAL(is_cv_or_ref<int>::value,false);
	BOOST_CHECK_EQUAL(is_cv_or_ref<int &>::value,true);
	BOOST_CHECK_EQUAL(is_cv_or_ref<const int>::value,true);
	BOOST_CHECK_EQUAL(is_cv_or_ref<const volatile int>::value,true);
	BOOST_CHECK_EQUAL(is_cv_or_ref<const volatile int &>::value,true);
	BOOST_CHECK_EQUAL(is_cv_or_ref<volatile int>::value,true);
	BOOST_CHECK_EQUAL(is_cv_or_ref<int * const>::value,true);
	BOOST_CHECK_EQUAL(is_cv_or_ref<int const *>::value,false);
}

BOOST_AUTO_TEST_CASE(type_traits_is_nonconst_rvalue_ref_test)
{
	BOOST_CHECK_EQUAL(is_nonconst_rvalue_ref<int>::value,false);
	BOOST_CHECK_EQUAL(is_nonconst_rvalue_ref<int &>::value,false);
	BOOST_CHECK_EQUAL(is_nonconst_rvalue_ref<const int>::value,false);
	BOOST_CHECK_EQUAL(is_nonconst_rvalue_ref<const volatile int>::value,false);
	BOOST_CHECK_EQUAL(is_nonconst_rvalue_ref<const volatile int &>::value,false);
	BOOST_CHECK_EQUAL(is_nonconst_rvalue_ref<volatile int>::value,false);
	BOOST_CHECK_EQUAL(is_nonconst_rvalue_ref<volatile int &&>::value,true);
	BOOST_CHECK_EQUAL(is_nonconst_rvalue_ref<volatile int const &&>::value,false);
	BOOST_CHECK_EQUAL(is_nonconst_rvalue_ref<const int &&>::value,false);
	BOOST_CHECK_EQUAL(is_nonconst_rvalue_ref<int &&>::value,true);
}

struct trivial {};

struct nontrivial_copy
{
	nontrivial_copy(nontrivial_copy &&) piranha_noexcept_spec(false) {}
	nontrivial_copy &operator=(nontrivial_copy &&) piranha_noexcept_spec(false)
	{
		return *this;
	}
	nontrivial_copy(const nontrivial_copy &other):n(other.n) {}
	int n;
};

struct nontrivial_dtor
{
	nontrivial_dtor(const nontrivial_dtor &) = default;
	nontrivial_dtor(nontrivial_dtor &&) piranha_noexcept_spec(false) {}
	nontrivial_dtor &operator=(nontrivial_dtor &&) piranha_noexcept_spec(false)
	{
		return *this;
	}
	~nontrivial_dtor() piranha_noexcept_spec(false)
	{
		n = 0;
	}
	int n;
};

BOOST_AUTO_TEST_CASE(type_traits_is_trivially_copyable)
{
	BOOST_CHECK_EQUAL(is_trivially_copyable<int>::value,true);
	BOOST_CHECK_EQUAL(is_trivially_copyable<trivial>::value,true);
	BOOST_CHECK_EQUAL(is_trivially_copyable<nontrivial_dtor>::value,true);
	BOOST_CHECK_EQUAL(is_trivially_copyable<nontrivial_copy>::value,false);
	BOOST_CHECK_EQUAL(is_trivially_copyable<std::string>::value,false);
}

BOOST_AUTO_TEST_CASE(type_traits_is_trivially_destructible)
{
	BOOST_CHECK_EQUAL(is_trivially_destructible<int>::value,true);
	BOOST_CHECK_EQUAL(is_trivially_destructible<trivial>::value,true);
	BOOST_CHECK_EQUAL(is_trivially_destructible<nontrivial_copy>::value,true);
	BOOST_CHECK_EQUAL(is_trivially_destructible<nontrivial_dtor>::value,false);
	BOOST_CHECK_EQUAL(is_trivially_destructible<std::string>::value,false);
}

#if defined(PIRANHA_HAVE_NOEXCEPT)
BOOST_AUTO_TEST_CASE(type_traits_nothrow_type_traits)
{
	BOOST_CHECK_EQUAL(is_nothrow_move_constructible<int>::value,true);
	BOOST_CHECK_EQUAL(is_nothrow_move_constructible<trivial>::value,true);
	BOOST_CHECK_EQUAL(is_nothrow_move_constructible<integer>::value,true);
	BOOST_CHECK_EQUAL(is_nothrow_move_constructible<nontrivial_dtor>::value,false);
	BOOST_CHECK_EQUAL(is_nothrow_move_constructible<nontrivial_copy>::value,false);
	BOOST_CHECK_EQUAL(is_nothrow_move_assignable<int>::value,true);
	BOOST_CHECK_EQUAL(is_nothrow_move_assignable<trivial>::value,true);
	BOOST_CHECK_EQUAL(is_nothrow_move_assignable<integer>::value,true);
	BOOST_CHECK_EQUAL(is_nothrow_move_assignable<nontrivial_dtor>::value,false);
	BOOST_CHECK_EQUAL(is_nothrow_move_assignable<nontrivial_copy>::value,false);
	BOOST_CHECK_EQUAL(is_nothrow_destructible<int>::value,true);
	BOOST_CHECK_EQUAL(is_nothrow_destructible<trivial>::value,true);
	BOOST_CHECK_EQUAL(is_nothrow_destructible<integer>::value,true);
	BOOST_CHECK_EQUAL(is_nothrow_destructible<nontrivial_dtor>::value,false);
	BOOST_CHECK_EQUAL(is_nothrow_destructible<nontrivial_copy>::value,true);
}

#endif

BOOST_AUTO_TEST_CASE(type_traits_is_tuple)
{
	BOOST_CHECK(is_tuple<std::tuple<>>::value);
	BOOST_CHECK(is_tuple<std::tuple<int>>::value);
	BOOST_CHECK(!is_tuple<std::string>::value);
}
