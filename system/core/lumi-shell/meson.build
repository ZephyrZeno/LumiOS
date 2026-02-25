project('lumi-shell', 'c',
  version: '0.1.0',
  default_options: ['c_std=c11', 'warning_level=2'])

shell_inc = include_directories('include')

shell_src = files('src/main.c')

executable('lumi-shell',
  shell_src,
  include_directories: shell_inc,
  install: true)
