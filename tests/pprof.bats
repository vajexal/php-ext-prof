setup() {
    load '/bats-support/load'
    load '/bats-assert/load'
    load '/bats-file/load'
}

teardown() {
    rm -f cpu.pprof
}

@test 'pprof' {
  assert_not_exists cpu.pprof
  PROF_MODE=sampling PROF_OUTPUT_MODE=pprof php -d extension=prof.so tests/example.php
  run go tool pprof -top cpu.pprof
  assert_success
  assert_line --index 5 --regexp '^\s+[0-9]+ms\s+100\%\s+100\%\s+[0-9]+ms\s+100\%\s+foo$'
}
