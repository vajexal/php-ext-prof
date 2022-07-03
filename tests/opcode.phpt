--TEST--
Opcode mode
--EXTENSIONS--
prof
--INI--
prof.mode=opcode
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

╭───%S───╮
│ opcode%w │ time      │
├───%S───┼───────────┤
│ %s:4 │ %fs │
│ %s:5 │ %fs │
│ %s:7 │ %fs │
╰───%S───╯
