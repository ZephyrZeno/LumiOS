project('lumi-compositor', 'c',
  version: '0.1.0',
  default_options: ['c_std=c11', 'warning_level=2'])

compositor_inc = include_directories('include')

compositor_src = files('src/main.c')

executable('lumi-compositor',
  compositor_src,
  include_directories: compositor_inc,
  install: true)
