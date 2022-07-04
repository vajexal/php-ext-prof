--TEST--
Func limit
--EXTENSIONS--
prof
--INI--
prof.mode=func
prof.func_limit=1
--FILE--
<?php
function foo()
{
    usleep(100);
}

function bar()
{
    usleep(10);
}

foo();
bar();
?>
--EXPECTF--
total time: %fs

╭──────────┬───────────┬───────────┬────────┬───────╮
│ function │ wall      │ cpu       │ memory │ calls │
├──────────┼───────────┼───────────┼────────┼───────┤
│ foo      │ %fs%w │ %fs%w │ %d b%w │ 1     │
╰──────────┴───────────┴───────────┴────────┴───────╯
