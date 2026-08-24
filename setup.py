from setuptools import setup, Extension
import os
import platform

os.environ['OPT'] = '-DNDEBUG -O3'
os.environ['CFLAGS'] = '-DNDEBUG -O3'
os.environ['CXXFLAGS'] = '-DNDEBUG -O3 -std=c++17'

cpp_args = ['-O3', '-DNDEBUG', '-std=c++17', '-ffast-math', '-funroll-loops', '-flto']
link_args = ['-flto']
if platform.system() == 'Darwin':
    cpp_args.extend(['-mcpu=native'])
elif platform.system() == 'Linux':
    cpp_args.extend(['-mavx2', '-mbmi2', '-mpopcnt', '-march=native'])

ext_modules = [
    Extension(
        'qi_sort_cpp',
        sources=['src/qi_python_module.cpp', 'src/qi_c_api.cpp'],
        include_dirs=['.', 'include'],
        extra_compile_args=cpp_args,
        extra_link_args=link_args,
        language='c++',
    ),
]

setup(
    name='qi_sort',
    version='0.3.12',
    description='Quantum-Inspired Adaptive Radix Sorting Engine',
    author='Jason Pandia',
    url='https://github.com/PandiaJason/qi-sort',
    py_modules=['qi_sort'],
    package_dir={'': 'bindings/python'},
    ext_modules=ext_modules,
    python_requires='>=3.8',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'Topic :: Software Development :: Libraries',
        'Programming Language :: C++',
        'Programming Language :: Python :: 3',
    ],
)
