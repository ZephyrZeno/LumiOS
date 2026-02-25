project('lumi-toolkit', 'c',
  version: '0.1.0',
  default_options: ['c_std=c11', 'warning_level=2'])

toolkit_inc = include_directories('include')

toolkit_src = files('src/widget.c')

lumi_toolkit_lib = static_library('lumi-toolkit',
  toolkit_src,
  include_directories: toolkit_inc,
  install: true)

lumi_toolkit_dep = declare_dependency(
  include_directories: toolkit_inc,
  link_with: lumi_toolkit_lib)
