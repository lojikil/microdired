# Overview

`microdired` is an experiment that came about when I first saw [nnn](https://github.com/jarun/nnn), which features
`vim`-like keystrokes. It looks extremely interesting, but I thought to myself "what if we made an `ed(1)`-like
file editor? what would that look like?" This started as a [simple gist](https://gist.github.com/lojikil/aae3c9aa948a421b894e361688f4ab87)
and grew into something fun to tinker with. Probably the _most_ useful part of this program is the parser for 
`ed(1)`-like command languages: the parser parses:

- ranges
- globs
- commands
- arguments

into a simple data structure. I've thought about turning `microdired` into something like [Holzmann's `pico(1)`](http://man.cat-v.org/unix_10th/1/pico)
but I have not gotten there just yet.

# Language

The language that this `ed(1)`-alike parser consumes is as follows:

`n,m(glob)cmd arg0 arg1 ... argN`

- `n,m`: a start line, and optional completion line
- `(glob)`: a Unix-shell `glob`-alike
- `cmd`: an actual command, like `l` or `def`
- `argN`: character string arguments

# Commands:

- `.`: print the current working directory
- `..`: go up to the parent directory
- `/somedirectory`: go to `/somedirectory`
- `l`/`L`: print directory with or without entry numbers
- `p`/`P`: pretty print directory with or without entry numbers
- `c`/`C`: create a file/directory
- `f`: print only files
- `F`: pretty print only files
- `d`: print only directories
- `D`: pretty print only directories
- `e`: invoke `$EDITOR`, falling back on internal `ed(1)`
- `E`: invoke internal `ed(1)`
- `m`: `more`, but built in
- `M`: mode, as in `chmod`
- `t`: `test`-like interface
- `!`: execute a unix shell
