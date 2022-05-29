--TEST--
Sampling threshold
--EXTENSIONS--
prof
--INI--
prof.mode=sampling
prof.sampling_threshold=100000
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
