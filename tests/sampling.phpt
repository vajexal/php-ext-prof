--TEST--
Sampling mode
--EXTENSIONS--
prof
--INI--
prof.mode=sampling
--FILE--
<?php
function foo()
{
    for ($i = 0; $i < 10_000_000; $i++) {
    }
}

foo();
?>
--EXPECTF--
total time: %fs

total samples: %i
function %w hits
foo %w %i (100%)
