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

function %w wall %w cpu %w memory %w calls
foo %w %fs %w %fs %w %i b %w 1
