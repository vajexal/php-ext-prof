--TEST--
Func mode
--EXTENSIONS--
prof
--INI--
prof.mode=func
--FILE--
<?php
function foo()
{
    usleep(100);
}

foo();
?>
--EXPECTF--
total time: %fs

╭──────────┬───────────┬───────────┬────────┬───────╮
│ function │ wall      │ cpu       │ memory │ calls │
├──────────┼───────────┼───────────┼────────┼───────┤
│ foo      │ %fs%w │ %fs%w │ %d b%w │ 1     │
╰──────────┴───────────┴───────────┴────────┴───────╯
