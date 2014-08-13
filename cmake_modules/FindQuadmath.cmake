# Module to find libquadmath. The situation is a bit complicated, as in some systems libquadmath is shipped
# as a regular library in /usr/lib and similar, on others it is, together with libstdc++/libgfortran/etc.,
# in some internal GCC path.
#
# What we do here is to avoid looking for the header (as, according to the libquadmath
# doc, we are supposed to be able to just include it wherever it is), and, if we do not find the library
# in a standard location, we will attempt a direct linking to the library. Then we test with a small executable
# and if it fails we mark the library as not found.

INCLUDE(CheckCXXSourceCompiles)

IF(QUADMATH_LIBRARIES)
	# Already in cache, be silent.
	SET(QUADMATH_FIND_QUIETLY TRUE)
ENDIF()

# First try to find it in the usual places.
FIND_LIBRARY(QUADMATH_LIBRARIES NAMES quadmath)

IF(NOT QUADMATH_LIBRARIES)
	MESSAGE(STATUS "libquadmath was not found in a standard path, trying direct linking.")
	SET(QUADMATH_LIBRARIES quadmath)
ENDIF()

# Test if it works.
SET(CMAKE_REQUIRED_LIBRARIES "${QUADMATH_LIBRARIES}")
CHECK_CXX_SOURCE_COMPILES("
extern \"C\" {
#include <quadmath.h>
}
int main()
{
    __float128 foo = ::sqrtq(123.456);
}" QUADMATH_COMPILE_CHECK)
UNSET(CMAKE_REQUIRED_LIBRARIES)

IF(NOT QUADMATH_COMPILE_CHECK)
	MESSAGE(STATUS "The compile test for libquadmath failed, marking library as not found.")
	UNSET(QUADMATH_LIBRARIES)
ENDIF()

INCLUDE(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(Quadmath DEFAULT_MSG QUADMATH_LIBRARIES)

MARK_AS_ADVANCED(QUADMATH_LIBRARIES)
