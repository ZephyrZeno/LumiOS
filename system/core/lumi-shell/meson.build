project('lumi-shell', 'c',
  version: '0.1.0',
  default_options: ['c_std=c11', 'warning_level=2'])

shell_inc = include_directories('include')
shell_test_inc = include_directories('include', 'src')

shell_src = files(
  'src/main.c',
  'src/catalog.c',
  'src/daemon.c',
  'src/session.c',
  'src/state.c',
  'src/watch.c',
)

executable('lumi-shell',
  shell_src,
  include_directories: shell_inc,
  install: true)

runtime_test = executable('test-runtime-refresh',
  ['src/state.c', 'src/daemon.c', 'src/session.c', 'src/watch.c', 'tests/test_runtime.c'],
  include_directories: shell_test_inc)
test('runtime-refresh', runtime_test)

if host_machine.system() != 'windows'
  daemon_test = executable('test-daemon-events',
    ['src/daemon.c', 'tests/test_daemon.c'],
    include_directories: shell_test_inc)
  test('daemon-events', daemon_test)
endif
