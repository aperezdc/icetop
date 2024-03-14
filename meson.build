project('icetop', 'cpp',
	version: '0.1',
	license: 'GPL3')

cpp = meson.get_compiler('cpp')

tickit = dependency('tickit',  required: true)
icecc = dependency('icecc',   required: true, version: '>= 1.0')

libdill = dependency('libdill', required: false, version: '>= 1.0', modules: 'libdill::dill')
if not libdill.found()
  libdill = cpp.find_library('dill',
    has_headers: 'libdill.h',
    required: true,
  )
endif

cpp_args = []
if cpp.has_argument('-Wall')
  cpp_args += ['-Wall']
endif

if cpp.has_argument('-std=gnu++14')
  cpp_args += ['-std=gnu++14']
elif cpp.has_argument('-std=c++14')
  cpp_args += ['-std=c++14']
else
  error('Your C++ compiler does not seem to support C++14')
endif

# Check whether icecc was compiled using the old C++ ABI, and work around this
# to be able to still use C++11/14 features while linking to the icecc library
abi_test_code = '''
#include <icecc/comm.h>
int main() {
  DiscoverSched sched("foo");
}
'''

if cpp.links(abi_test_code,
	name: 'IceCC uses the new C++11 ABI',
	args: cpp_args + ['-D_GLIBCXX_USE_CXX11_ABI=1'],
	dependencies: icecc)
  cpp_args += ['-D_GLIBCXX_USE_CXX11_ABI=1']
elif cpp.links(abi_test_code,
	name: 'IceCC uses the old pre-C++11 ABI',
	args: cpp_args + ['-D_GLIBCXX_USE_CXX11_ABI=0'],
	dependencies: icecc)
  cpp_args += ['-D_GLIBCXX_USE_CXX11_ABI=0']
else
  error('Could not determine which C++ ABI was used to build IceCC')
endif


icetop = executable('icetop',
	'icetop.cc',
	'util/getenv.cc',
	'util/getenv.hh',
	'util/ti.cc',
	'util/ti.hh',
	dependencies: [libdill, icecc, tickit],
	cpp_args: cpp_args,
	install: true)
