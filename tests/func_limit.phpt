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

function %w wall %w cpu %w memory %w calls
foo %w %fs %w %fs %w %i b %w 1
