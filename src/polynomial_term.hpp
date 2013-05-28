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

#ifndef PIRANHA_POLYNOMIAL_TERM_HPP
#define PIRANHA_POLYNOMIAL_TERM_HPP

#include <boost/concept/assert.hpp>
#include <type_traits>
#include <vector>

#include "base_term.hpp"
#include "concepts/multipliable_term.hpp"
#include "detail/series_fwd.hpp"
#include "detail/series_multiplier_fwd.hpp"
#include "forwarding.hpp"
#include "kronecker_monomial.hpp"
#include "math.hpp"
#include "monomial.hpp"
#include "power_series_term.hpp"
#include "symbol_set.hpp"
#include "symbol.hpp"
#include "univariate_monomial.hpp"
#include "type_traits.hpp"

namespace piranha
{

namespace detail
{

template <typename T>
struct polynomial_term_key
{
	typedef monomial<T> type;
};

template <typename T>
struct polynomial_term_key<univariate_monomial<T>>
{
	typedef univariate_monomial<T> type;
};

template <typename T>
struct polynomial_term_key<kronecker_monomial<T>>
{
	typedef kronecker_monomial<T> type;
};

}

/// Polynomial term.
/**
 * This class extends piranha::base_term for use in polynomials. The coefficient type \p Cf is generic,
 * the key type is determined as follows:
 * 
 * - if \p ExpoType is piranha::univariate_monomial of \p T, the key will also be piranha::univariate_monomial of \p T,
 * - if \p ExpoType is piranha::kronecker_monomial of \p T, the key will also be piranha::kronecker_monomial of \p T,
 * - otherwise, the key will be piranha::monomial of \p ExpoType.
 * 
 * Examples:
 * @code
 * polynomial_term<double,int>
 * @endcode
 * is a multivariate polynomial term with double-precision coefficient and \p int exponents.
 * @code
 * polynomial_term<double,static_size<int,5>>
 * @endcode
 * is a multivariate polynomial term with double-precision coefficient and a maximum of 5 \p int exponents.
 * @code
 * polynomial_term<double,univariate_monomial<int>>
 * @endcode
 * is a univariate polynomial term with double-precision coefficient and \p int exponent.
 * @code
 * polynomial_term<double,kronecker_monomial<>>
 * @endcode
 * is a multivariate polynomial term with double-precision coefficient and integral exponents packed into a piranha::kronecker_monomial.
 * 
 * This class is a model of the piranha::concept::MultipliableTerm concept.
 * 
 * \section type_requirements Type requirements
 * 
 * - \p Cf must satisfy the following type traits:
 *   - piranha::is_cf,
 *   - piranha::is_multipliable and piranha::is_multipliable_in_place,
 *   - piranha::has_multiply_accumulate.
 * - \p ExpoType must be suitable for use in piranha::monomial, or be piranha::univariate_monomial or piranha::kronecker_monomial.
 * 
 * \section exception_safety Exception safety guarantee
 * 
 * This class provides the same guarantee as piranha::base_term.
 * 
 * \section move_semantics Move semantics
 * 
 * Move semantics is equivalent to piranha::base_term's move semantics.
 * 
 * @author Francesco Biscani (bluescarni@gmail.com)
 */
template <typename Cf, typename ExpoType>
class polynomial_term: public power_series_term<base_term<Cf,typename detail::polynomial_term_key<ExpoType>::type,polynomial_term<Cf,ExpoType>>>
{
		PIRANHA_TT_CHECK(is_cf,Cf);
		PIRANHA_TT_CHECK(is_multipliable,Cf);
		PIRANHA_TT_CHECK(is_multipliable_in_place,Cf);
		PIRANHA_TT_CHECK(has_multiply_accumulate,Cf);
		typedef power_series_term<base_term<Cf,typename detail::polynomial_term_key<ExpoType>::type,polynomial_term<Cf,ExpoType>>> base;
		// Make friend with series multipliers.
		template <typename Series1, typename Series2, typename Enable>
		friend class series_multiplier;
	public:
		/// Result type for the multiplication by another term.
		typedef polynomial_term multiplication_result_type;
		/// Defaulted default constructor.
		polynomial_term() = default;
		/// Defaulted copy constructor.
		polynomial_term(const polynomial_term &) = default;
		/// Defaulted move constructor.
		polynomial_term(polynomial_term &&) = default;
		PIRANHA_FORWARDING_CTOR(polynomial_term,base)
		/// Trivial destructor.
		~polynomial_term() noexcept(true)
		{
			BOOST_CONCEPT_ASSERT((concept::MultipliableTerm<polynomial_term>));
		}
		/// Defaulted copy assignment operator.
		polynomial_term &operator=(const polynomial_term &) = default;
		/// Defaulted move assignment operator.
		polynomial_term &operator=(polynomial_term &&) = default;
		/// Term multiplication.
		/**
		 * Multiplication of \p this by \p other will produce a single term whose coefficient is the
		 * result of the multiplication of the current coefficient by the coefficient of \p other
		 * and whose key is the element-by-element sum of the vectors of the exponents of the two terms.
		 * 
		 * The operator used for coefficient multiplication will be in-place multiplication
		 * if the coefficient types (\p Cf and \p Cf2) are not series, otherwise it will be
		 * the binary multiplication operator.
		 * 
		 * This method provides the basic exception safety guarantee: in face of exceptions, \p retval will be left in an
		 * undefined but valid state.
		 * 
		 * @param[out] retval return value of the multiplication.
		 * @param[in] other argument of the multiplication.
		 * @param[in] args reference set of arguments.
		 * 
		 * @throws unspecified any exception thrown by:
		 * - the assignment operators of the coefficient type,
		 * - the <tt>multiply()</tt> method of the key type,
		 * - the multiplication operators of the coefficient types.
		 */
		template <typename Cf2, typename ExpoType2>
		void multiply(polynomial_term &retval, const polynomial_term<Cf2,ExpoType2> &other, const symbol_set &args) const
		{
			cf_mult_impl(retval,other);
			this->m_key.multiply(retval.m_key,other.m_key,args);
		}
		/// Partial derivative.
		/**
		 * Will return a vector of polynomial terms representing the partial derivative of \p this with respect to
		 * symbol \p s. The partial derivative is computed via piranha::math::partial() and the differentiation method
		 * of the monomial type. This method requires the coefficient type to
		 * - be multipliable by piranha::integer and/or by the monomial exponent type,
		 * - satisfy the piranha::is_differentiable type trait.
		 * 
		 * @param[in] s piranha::symbol with respect to which the derivative will be calculated.
		 * @param[in] args reference set of arguments.
		 * 
		 * @return partial derivative of \p this with respect to \p s.
		 * 
		 * @throws unspecified any exception throw by:
		 * - piranha::math::partial() and piranha::math::is_zero(),
		 * - coefficient, term and key constructors,
		 * - memory allocation errors in standard containers,
		 * - the differentiation method of the monomial type,
		 * - the multiplication operator of the coefficient type.
		 */
		std::vector<polynomial_term> partial(const symbol &s, const symbol_set &args) const
		{
			PIRANHA_TT_CHECK(is_differentiable,Cf);
			std::vector<polynomial_term> retval;
			auto cf_partial = math::partial(this->m_cf,s.get_name());
			if (!math::is_zero(cf_partial)) {
				retval.push_back(polynomial_term(std::move(cf_partial),this->m_key));
			}
			auto key_partial = this->m_key.partial(s,args);
			if (!math::is_zero(key_partial.first)) {
				retval.push_back(polynomial_term(this->m_cf * key_partial.first,std::move(key_partial.second)));
			}
			return retval;
		}
	private:
		// Overload if no coefficient is series.
		template <typename Cf2, typename ExpoType2>
		void cf_mult_impl(polynomial_term &retval, const polynomial_term<Cf2,ExpoType2> &other,
			typename std::enable_if<!std::is_base_of<detail::series_tag,Cf>::value &&
			!std::is_base_of<detail::series_tag,Cf2>::value>::type * = nullptr) const
		{
			retval.m_cf = this->m_cf;
			retval.m_cf *= other.m_cf;
		}
		// Overload if at least one coefficient is series.
		template <typename Cf2, typename ExpoType2>
		void cf_mult_impl(polynomial_term &retval, const polynomial_term<Cf2,ExpoType2> &other,
			typename std::enable_if<std::is_base_of<detail::series_tag,Cf>::value ||
			std::is_base_of<detail::series_tag,Cf2>::value>::type * = nullptr) const
		{
			retval.m_cf = this->m_cf * other.m_cf;
		}
};

}

#endif
