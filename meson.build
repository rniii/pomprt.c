project('pomrpt', 'c',
  default_options: ['c_std=c99', 'warning_level=3'],
  license: 'Apache-2.0',
  version: '0.1.0')

pomprt_inc = include_directories('include')

pomprt_lib = static_library('pomprt', 'src/pomprt.c',
  include_directories: pomprt_inc,
  install: true)

pomprt_dep = declare_dependency(
  link_with: pomprt_lib,
  include_directories: pomprt_inc)

executable('example', 'example.c',
  dependencies: pomprt_dep)
