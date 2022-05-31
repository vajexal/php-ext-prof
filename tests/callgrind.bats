setup() {
    load '/bats-support/load'
    load '/bats-assert/load'
    load '/bats-file/load'
}

teardown() {
    rm -f callgrind.out
}

@test 'callgrind' {
  assert_not_exists callgrind.out
  PROF_MODE=sampling PROF_OUTPUT_MODE=callgrind php -d extension=prof.so tests/example.php
  run callgrind_annotate callgrind.out
  assert_success
  assert_line --regexp '^[0-9]+ \(100\.0\%\)  tests\/example\.php\:foo$'
}
