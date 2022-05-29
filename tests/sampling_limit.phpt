--TEST--
Sampling limit
--EXTENSIONS--
prof
--INI--
prof.mode=sampling
prof.sampling_limit=1
--FILE--
<?php
function foo()
{
    for ($i = 0; $i < 10_000_000; $i++) {
    }
}

function bar()
{
    for ($i = 0; $i < 1_000_000; $i++) {
    }
}

foo();
bar();
?>
--EXPECTF--
total time: %fs

total samples: %i
function %w hits
foo %w %i (%i%)
