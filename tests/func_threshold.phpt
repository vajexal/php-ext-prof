--TEST--
Func threshold
--EXTENSIONS--
prof
--INI--
prof.mode=func
prof.func_threshold=1000000
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

function %w wall %w cpu %w memory %w calls
