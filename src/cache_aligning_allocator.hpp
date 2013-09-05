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

#ifndef PIRANHA_CACHE_ALIGNING_ALLOCATOR_HPP
#define PIRANHA_CACHE_ALIGNING_ALLOCATOR_HPP

#include <boost/numeric/conversion/cast.hpp>
#include <cstddef>

#include "aligned_memory.hpp"
#include "dynamic_aligning_allocator.hpp"
#include "settings.hpp"

namespace piranha
{

/// Allocator that tries to align memory to the cache line size.
/**
 * This allocator will try to allocate memory aligned to the cache line size (as reported by piranha::settings).
 * 
 * Exception safety and move semantics are equivalent to piranha::dynamic_aligning_allocator.
 * 
 * @author Francesco Biscani (bluescarni@gmail.com)
 */
template <typename T>
class cache_aligning_allocator: public dynamic_aligning_allocator<T>
{
		using base = dynamic_aligning_allocator<T>;
		static std::size_t determine_alignment()
		{
#if !defined(PIRANHA_HAVE_MEMORY_ALIGNMENT_PRIMITIVES)
			return 0u;
#endif
			try {
				const std::size_t alignment = boost::numeric_cast<std::size_t>(settings::get_cache_line_size());
				if (!alignment_check<T>(alignment)) {
					return 0u;
				}
				return alignment;
			} catch (...) {
				return 0u;
			}
		}
	public:
		/// Default constructor.
		/**
		 * Will invoke the base constructor with an alignment value determined as follows:
		 * - if no memory alignment primitives are available on the host platform, the value will be zero;
		 * - if the cache line size reported by piranha::settings::get_cache_line_size() passes the checks
		 *   performed by piranha::alignment_check() of \p T, it will be used as construction value;
		 * - otherwise, zero will be used.
		 */
		cache_aligning_allocator():base(determine_alignment()) {}
		/// Defaulted copy constructor.
		cache_aligning_allocator(const cache_aligning_allocator &) = default;
		/// Defaulted move constructor.
		cache_aligning_allocator(cache_aligning_allocator &&) = default;
		/// Copy-constructor from different instance.
		/**
		 * Will forward the call to the corresponding constructor in piranha::dynamic_aligning_allocator.
		 *
		 * @param[in] other construction argument.
		 */
		template <typename U>
		explicit cache_aligning_allocator(const cache_aligning_allocator<U> &other):base(other) {}
		/// Move-constructor from different instance.
		/**
		 * Will forward the call to the corresponding constructor in piranha::dynamic_aligning_allocator.
		 *
		 * @param[in] other construction argument.
		 */
		template <typename U>
		explicit cache_aligning_allocator(cache_aligning_allocator<U> &&other):base(std::move(other)) {}
		/// Defaulted destructor.
		~cache_aligning_allocator() = default;
		/// Defaulted copy assignment operator.
		cache_aligning_allocator &operator=(const cache_aligning_allocator &) = default;
		/// Defaulted move assignment operator.
		cache_aligning_allocator &operator=(cache_aligning_allocator &&) = default;
};

}

#endif
