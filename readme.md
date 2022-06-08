php profiler

[![Build Status](https://github.com/vajexal/php-ext-prof/workflows/Build/badge.svg)](https://github.com/vajexal/php-ext-prof/actions)

Profiler can run in the following modes

- sampling (stops program every n microseconds and analyzes current stack)
- func (hooks into every function call)
- opcode (hooks into every opcode)

mode can be set through `prof.mode` ini setting or `PROF_MODE` env var

#### Sampling

```php
// a.php
function foo()
{
    for ($i = 0; $i < 10_000_000; $i++) {
    }
}

foo();
```

```bash
php -d prof.mode=sampling a.php
```

output

```
total time: 0.069855s

total samples: 69
function             hits
foo                  69 (100%)
```

##### Output modes:

- console
- callgrind (will generate `callgrind.out` file)
- pprof (will generate `cpu.pprof` file)

can be set throgh `prof.output_mode` ini setting or `PROF_OUTPUT_MODE` env var

##### Additional params

- sampling interval (`prof.sampling_interval` ini setting or `PROF_SAMPLING_INTERVAL` env var, default value is 1000 microseconds)
- sampling limit (`prof.sampling_limit` ini setting or `PROF_SAMPLING_LIMIT` env var, default value is 20, only in console output mode), show only top n functions
- sampling threshold (`prof.sampling_threshold` ini setting or `PROF_SAMPLING_THRESHOLD` env var, default value is 1), show only functions with > n calls

#### Func

```php
// a.php
function foo()
{
    usleep(1000);
}

foo();
```

```bash
php -d prof.mode=func a.php
```

output

```
total time: 0.002412s

function             wall        cpu         memory     calls
foo                  0.001083s   0.000034s   448 b      1
```

##### Additional params

- func limit (`prof.func_limit` ini setting or `PROF_FUNC_LIMIT` env var, default value is 50), show only top n functions by wall time
- func threshold (`prof.func_threshold` ini setting or `PROF_FUNC_THRESHOLD` env var, default value is 1 microsecond), show only functions with wall time > n

#### Opcode

```php
// a.php
function foo()
{
    usleep(1000);
}

foo();
```

```bash
php -d prof.mode=opcode a.php
```

output

```
total time: 0.002116s

line                      time
/app/t.php:5              0.001105s
/app/t.php:6              0.000015s
/app/t.php:8              0.000009s
```

### Limitations

- for now profiler works only in cli mode

### Requirements

- zlib

### Installation

```bash
phpize
./configure
make
make install
```

### Testing

```bash
make test
```
