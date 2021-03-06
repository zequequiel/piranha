from setuptools import setup
from setuptools.dist import Distribution
import sys

NAME = 'pyranha'
VERSION = '@piranha_VERSION@'
DESCRIPTION = 'A computer algebra system for celestial mechanics'
LONG_DESCRIPTION = 'Pyranha is a Python CAS specialised for celestial mechanics applications.'
URL = 'https://github.com/bluescarni/piranha'
AUTHOR = 'Francesco Biscani'
AUTHOR_EMAIL = 'bluescarni@gmail.com'
LICENSE = 'GPLv3+/LGPL3+'
CLASSIFIERS = [
    # How mature is this project? Common values are
    #   3 - Alpha
    #   4 - Beta
    #   5 - Production/Stable
    'Development Status :: 4 - Beta',

    'Operating System :: OS Independent',

    'Intended Audience :: Science/Research',
    'Topic :: Scientific/Engineering',
    'Topic :: Scientific/Engineering :: Astronomy',
    'Topic :: Scientific/Engineering :: Mathematics',
    'Topic :: Scientific/Engineering :: Physics',

    'License :: OSI Approved :: GNU General Public License v3 or later (GPLv3+)',
    'License :: OSI Approved :: GNU Lesser General Public License v3 or later (LGPLv3+)',

    'Programming Language :: Python :: 2',
    'Programming Language :: Python :: 3'
]
KEYWORDS = 'computer_algebra CAS science math physics astronomy'
INSTALL_REQUIRES = ['numpy','mpmath']
PLATFORMS = ['Unix','Windows','OSX']

class BinaryDistribution(Distribution):
    def has_ext_modules(foo):
        return True

# Setup the list of external dlls.
import os.path
mingw_wheel_libs = 'mingw_wheel_libs_python{}.txt'.format(sys.version_info[0])
l = open(mingw_wheel_libs,'r').readlines()
DLL_LIST = [os.path.basename(_[:-1]) for _ in l]

setup(name=NAME,
    version=VERSION,
    description=DESCRIPTION,
    long_description=LONG_DESCRIPTION,
    url=URL,
    author=AUTHOR,
    author_email=AUTHOR_EMAIL,
    license=LICENSE,
    classifiers=CLASSIFIERS,
    keywords=KEYWORDS,
    platforms=PLATFORMS,
    install_requires=INSTALL_REQUIRES,
    packages=['pyranha','pyranha._tutorial'],
    # Include pre-compiled extension
    package_data={'pyranha': ['_core.pyd'] + DLL_LIST},
    distclass=BinaryDistribution)
