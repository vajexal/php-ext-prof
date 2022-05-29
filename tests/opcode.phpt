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

line %w time
%s:4 %w %fs
%s:5 %w %fs
%s:7 %w %fs
