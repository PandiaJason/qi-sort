from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import sys
import os

class BuildExt(build_ext):
    def build_extensions(self):
        opts = ['-O3', '-std=c++17']
        if sys.platform == 'darwin':
            opts.append('-arch')
            opts.append('arm64')
            opts.append('-arch')
            opts.append('x86_64')
        elif sys.platform == 'win32':
            opts = ['/O2', '/std:c++17']

        for ext in self.extensions:
            ext.extra_compile_args = opts
        build_ext.build_extensions(self)

qi_module = Extension(
    'qi_sort_cpp',
    sources=['src/qi_c_api.cpp'],
    include_dirs=['include'],
    language='c++'
)

setup(
    name='qi-sort',
    version='1.0.0',
    author='Antigravity Team & Jason Pandia',
    description='Ultra-fast Quantum-Inspired Adaptive Radix Sorting Engine in C++17',
    long_description=open('README.md').read(),
    long_description_content_type='text/markdown',
    url='https://github.com/PandiaJason/qi-sort',
    py_modules=['qi_sort'],
    package_dir={'': 'bindings/python'},
    ext_modules=[qi_module],
    cmdclass={'build_ext': BuildExt},
    classifiers=[
        'Programming Language :: C++',
        'Programming Language :: Python :: 3',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
        'Topic :: Software Development :: Libraries :: Python Modules',
    ],
    python_requires='>=3.7',
)
