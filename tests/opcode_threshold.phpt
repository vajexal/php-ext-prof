--TEST--
Opcode threshold
--EXTENSIONS--
prof
--INI--
prof.mode=opcode
prof.opcode_threshold=1
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

╭────────┬──────╮
│ opcode │ time │
╰────────┴──────╯
