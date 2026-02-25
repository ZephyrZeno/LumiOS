project('lumi-render', 'c',
  version: '0.1.0',
  default_options: ['c_std=c11', 'warning_level=2'])

render_inc = include_directories('include')

render_src = files('src/render.c')

lumi_render_lib = static_library('lumi-render',
  render_src,
  include_directories: render_inc,
  install: true)

lumi_render_dep = declare_dependency(
  include_directories: render_inc,
  link_with: lumi_render_lib)
