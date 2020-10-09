#!/usr/bin/env python

from distutils.core import setup
# distutils: provide developer convient way to publish new module
# publish package 'bimacs'
# to launch it: python setup.py sdist
# automatically generated log file MANIFEST, generated module in disk folder named k_b_d-1.3.1.tar.gz
# after installation of the module, user can import module and use the function

setup(name='kit_bimacs_dataset',
      version='1.3.1',
      description='KIT Bimanual Actions Dataset',
      author='Christian R. G. Dreher',
      author_email='c.dreher@kit.edu',
      url='https://gitlab.com/dreher/kit_bimacs_dataset',
      packages=['bimacs'],
)
