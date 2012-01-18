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

#ifndef PIRANHA_THREADING_HPP
#define PIRANHA_THREADING_HPP

/** \file threading.hpp
 * \brief Threading primitives.
 * 
 * The functions, classes and typedefs in this file provide an abstraction layer to threading primitives on the host platform.
 * If standard C++ threading support is available, it will be used, otherwise Boost.Thread will be used.
 * 
 * @see http://www.boost.org/doc/libs/release/doc/html/thread.html
 * 
 * \todo rework the threading support as follows:
 * - introduce thread class that inherits from boost/std thread, and make it detach() on destruction -> eventually include in the
 *   dtor also the MPFR cleanup functions for threading;
 * - rework the parallel algorithms to use futures, promises and packaged tasks - it seems that by employing these we can
 *   avoid thread barriers, thread groups, multi-thread exception handling, etc.
 * \todo review the semantics of boost stuff vs std c++, we know for instance that future::get() is a bit different, check
 * there are no other surprises.
 */

#include "config.hpp"

#if defined(PIRANHA_USE_BOOST_THREAD)
	#include <boost/exception_ptr.hpp>
	#include <boost/thread/condition_variable.hpp>
	#include <boost/thread/future.hpp>
	#include <boost/thread/locks.hpp>
	#include <boost/thread/mutex.hpp>
	#include <boost/thread/thread.hpp>
#else
	#include <condition_variable>
	#include <exception>
	#include <future>
	#include <mutex>
	#include <thread>
#endif

namespace piranha
{

/// Thread type.
/**
 * Typedef for either <tt>std::thread</tt> or <tt>boost::thread</tt>.
 * 
 * @see http://www.boost.org/doc/libs/release/doc/html/thread/thread_management.html#thread.thread_management.thread
 */
#if defined(PIRANHA_USE_BOOST_THREAD)
typedef boost::thread thread;
#else
typedef std::thread thread;
#endif

/// Condition variable type.
/**
 * Typedef for either <tt>std::condition_variable</tt> or <tt>boost::condition_variable</tt>.
 * 
 * @see http://www.boost.org/doc/libs/release/doc/html/thread/synchronization.html#thread.synchronization.condvar_ref
 */
#if defined(PIRANHA_USE_BOOST_THREAD)
typedef boost::condition_variable condition_variable;
#else
typedef std::condition_variable condition_variable;
#endif

/// Mutex type.
/**
 * Typedef for either <tt>std::mutex</tt> or <tt>boost::mutex</tt>.
 * 
 * @see http://www.boost.org/doc/libs/release/doc/html/interprocess/synchronization_mechanisms.html
 */
#if defined(PIRANHA_USE_BOOST_THREAD)
typedef boost::mutex mutex;
#else
typedef std::mutex mutex;
#endif

/// Exception pointer type.
/**
 * Typedef for either <tt>std::exception_ptr</tt> or <tt>boost::exception_ptr</tt>.
 * 
 * @see http://www.boost.org/doc/libs/release/libs/exception/doc/exception_ptr.html
 */
#if defined(PIRANHA_USE_BOOST_THREAD)
typedef boost::exception_ptr exception_ptr;
#else
typedef std::exception_ptr exception_ptr;
#endif

/// Current exception.
/**
 * Wrapper around either <tt>std::current_exception()</tt> or <tt>boost::current_exception()</tt>
 * 
 * @return a piranha::exception_ptr that refers to the currently handled exception or a copy of the currently handled exception.
 * If the function needs to allocate memory and the attempt fails, it returns a piranha::exception_ptr that refers to an instance of <tt>std::bad_alloc</tt>.
 * 
 * @see http://www.boost.org/doc/libs/release/libs/exception/doc/current_exception.html
 */
inline exception_ptr current_exception()
{
#if defined(PIRANHA_USE_BOOST_THREAD)
	return boost::current_exception();
#else
	return std::current_exception();
#endif
}

/// Lock guard selector.
/**
 * Provides a typedef for either <tt>std::lock_guard<Lockable></tt> or <tt>boost::lock_guard<Lockable></tt>.
 * 
 * @see http://www.boost.org/doc/libs/release/doc/html/thread/synchronization.html#thread.synchronization.locks.lock_guard
 * 
 * \todo replace with template typedef, once it gets implemented.
 */
template <typename Lockable>
struct lock_guard
{
	/// Type definition.
#if defined(PIRANHA_USE_BOOST_THREAD)
	typedef boost::lock_guard<Lockable> type;
#else
	typedef std::lock_guard<Lockable> type;
#endif
};

/// Unique lock selector.
/**
 * Provides a typedef for either <tt>std::unique_lock<Lockable></tt> or <tt>boost::unique_lock<Lockable></tt>.
 * 
 * @see http://www.boost.org/doc/libs/release/doc/html/thread/synchronization.html#thread.synchronization.locks.unique_lock
 * 
 * \todo replace with template typedef, once it gets implemented.
 */
template <typename Lockable>
struct unique_lock
{
	/// Type definition.
#if defined(PIRANHA_USE_BOOST_THREAD)
	typedef boost::unique_lock<Lockable> type;
#else
	typedef std::unique_lock<Lockable> type;
#endif
};

/// Thread ID.
/**
 * Typedef for either <tt>std::thread::id</tt> or <tt>boost::thread::id</tt>.
 * 
 * @see http://www.boost.org/doc/libs/release/doc/html/thread/thread_management.html#thread.thread_management.thread.id
 */
#if defined(PIRANHA_USE_BOOST_THREAD)
typedef boost::thread::id thread_id;
#else
typedef std::thread::id thread_id;
#endif

/// Future selector.
/**
 * Provides a typedef for either <tt>std::future<T></tt> or <tt>boost::unique_future<T></tt>.
 * 
 * @see http://www.boost.org/doc/libs/release/doc/html/thread/synchronization.html#thread.synchronization.futures
 */
template <typename T>
struct future
{
	/// Type definition.
#if defined(PIRANHA_USE_BOOST_THREAD)
	typedef boost::unique_future<T> type;
#else
	typedef std::future<T> type;
#endif
};

/// Promise selector.
/**
 * Provides a typedef for either <tt>std::promise<T></tt> or <tt>boost::promise<T></tt>.
 * 
 * @see http://www.boost.org/doc/libs/release/doc/html/thread/synchronization.html#thread.synchronization.futures
 */
template <typename T>
struct promise
{
	/// Type definition.
#if defined(PIRANHA_USE_BOOST_THREAD)
	typedef boost::promise<T> type;
#else
	typedef std::promise<T> type;
#endif
};

/// Calling thread namespace.
/**
 * The functions in this namespace operate on the calling thread.
 * This namespace is meant to be equivalent to either <tt>std::this_thread</tt> or <tt>boost::this_thread</tt>.
 */
namespace this_thread
{

/// Get thread ID.
/**
 * @return piranha::thread_id of the calling thread.
 */
inline thread_id get_id()
{
#if defined(PIRANHA_USE_BOOST_THREAD)
	return boost::this_thread::get_id();
#else
	return std::this_thread::get_id();
#endif
}

}

}

#endif
