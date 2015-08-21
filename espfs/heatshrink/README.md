# heatshrink

A data compression/decompression library for embedded/real-time systems.

## Key Features:

- **Low memory usage (as low as 50 bytes)**      
    It is useful for some cases with less than 50 bytes, and useful
    for many general cases with < 300 bytes.
- **Incremental, bounded CPU use**  
    You can chew on input data in arbitrarily tiny bites.
    This is a useful property in hard real-time environments.
- **Can use either static or dynamic memory allocation**      
    The library doesn't impose any constraints on memory management.
- **ISC license**      
    You can use it freely, even for commercial purposes.

## Getting Started:

There is a standalone command-line program, `heatshrink`, but the
encoder and decoder can also be used as libraries, independent of each
other. To do so, copy `heatshrink_common.h`, `heatshrink_config.h`, and
either `heatshrink_encoder.c` or `heatshrink_decoder.c` (and their
respective header) into your project.

Dynamic allocation is used by default, but in an embedded context, you
probably want to statically allocate the encoder/decoder. Set
`HEATSHRINK_DYNAMIC_ALLOC` to 0 in `heatshrink_config.h`.

## More Information and Benchmarks:

heatshrink is based on [LZSS], since it's particularly suitable for
compression in small amounts of memory. It can use an optional, small
[index] to make compression significantly faster, but otherwise can run
in under 100 bytes of memory. The index currently adds 2^(window size+1)
bytes to memory usage for compression, and temporarily allocates 512
bytes on the stack during index construction.

For more information, see the [blog post] for an overview, and the
`heatshrink_encoder.h` / `heatshrink_decoder.h` header files for API
documentation.

[blog post]: http://spin.atomicobject.com/2013/03/14/heatshrink-embedded-data-compression/
[index]: http://spin.atomicobject.com/2014/01/13/lightweight-indexing-for-embedded-systems/
[LZSS]: http://en.wikipedia.org/wiki/Lempel-Ziv-Storer-Szymanski

## Build Status

  [![Build Status](https://travis-ci.org/atomicobject/heatshrink.png)](http://travis-ci.org/atomicobject/heatshrink)
