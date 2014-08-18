JKSN specification
==================

JKSN, prononced as "Jackson", is an abbreviation of Jackson Compressed Serialize Notation, aims to be a binary serialization format compatible with JSON.

JKSN stream can be further compressed with GZIP to get better results.

File formats
------------

    [magic header] [control byte] [checksum] [control byte] [data bytes] [control byte] [data bytes] ...

### Magic header

This part contains four bytes `jk!`, it is used for safe Internet transmission and data type recognition. If the network bandwidth is limited, magic header can be omitted.

### Control byte

No data bytes are followed after 0x0n.

Special values:

    0x00: undefined
    0x01: null
    0x02: false
    0x03: true

Integers:

An Integer is followed after 0x1b 0x1c 0x1d 0x1e 0x1f.

    0x1n (where 0<=n<=a): represents a integer that is n
    0x1b: a signed 32-bit integer is followed
    0x1c: a signed 16-bit integer is followed
    0x1d: a signed 8-bit integer is followed
    0x1e: a negative variable length integer is followed
    0x1f: a positive variable length integer is followed

Float point numbers:

An float point number is followed after 0x2b 0x2c 0x2d.

    0x20: NaN
    0x21: a string control byte and a string containing a plain text JSON literal are followed
    0x2b: an IEEE long double (128-bit) number is followed
    0x2c: an IEEE double (64-bit) number is followed
    0x2d: an IEEE float (32-bit) number is followed
    0x2e: -Infinity
    0x2f: Infinity

UTF-16 strings:

    0x3n (where 0<=n<=b): a UTF-16 string containing n byte pairs are followed
    0x3c: an unsigned 8-bit integer is followed, representing the nearest previous UTF-16 string with this BKDR Hash (seed=131) value.
    0x3d: an unsigned 16-bit integer and a UTF-16 string containing that amount of byte pairs are followed
    0x3e: an unsigned 8-bit integer and a UTF-16 string containing that amount of byte pairs are followed
    0x3f: a positive variable length integer and a UTF-16 string containing that amount of byte pairs are followed

UTF-8 strings:

    0x4n (where 0<=n<=b): a UTF-8 string containing n bytes are followed
    0x4c: an unsigned 8-bit integer is followed, representing the nearest previous UTF-8 string with this BKDR Hash (seed=131) value.
    0x4d: an unsigned 16-bit integer and a UTF-8 string containing that amount of bytes are followed
    0x4e: an unsigned 8-bit integer and a UTF-8 string containing that amount of bytes are followed
    0x4f: a positive variable length integer and a UTF-8 string containing that amount of bytes are followed

Arrays:

Items are nested layers of control bytes and data bytes.

    0x8n (where 0<=n<=c): an array containing n items are followed
    0x8d: an unsigned 16-bit integer and an array containing that amount of items are followed
    0x8e: an unsigned 8-bit integer and an array containing that amount of items are followed
    0x8f: a positive variable length integer and an array containing that amount of items are followed

Objects:

    0x9n (where 0<=n<=c): an object containing n string-item pairs are followed
    0x9d: an unsigned 16-bit integer and an object containing that amount of string-item pairs are followed
    0x9e: an unsigned 8-bit integer and an object containing that amount of string-item pairs are followed
    0x9f: a positive variable length integer and an object containing that amount of string-item pairs are followed

Row-col swapped array:

    0xa0: unspecified, used to skip columns that are not specified
    0xan (where 1<=n<=c): a row-col swapped array containing n columns are followed
    0xad: an unsigned 16-bit integer and a row-col swapped array containing that amount of columns are followed
    0xae: an unsigned 16-bit integer and a row-col swapped array containing that amount of columns are followed
    0xae: a positive variable length integer and a row-col swapped array containing that amount of columns are followed

Checksums:

    0xf0: a CRC32 checksum
    0xf1: an MD5 checksum
    0xf2: a SHA-1 checksum
    0xf3: a SHA-2 checksum
    0xf8: a delayed CRC32 checksum will be present at the end of the stream
    0xf9: a delayed MD5 checksum will be present at the end of the stream
    0xfa: a delayed SHA-1 checksum will be present at the end of the stream
    0xfb: a delayed SHA-2 checksum will be present at the end of the stream

### Variable length integer

Every byte is separated into two parts, 1 continuation bit and 7 data bits. The last byte has a continuation bit of 0, others have 1.

For example

        A        B        C        D
    1ddddddd 1ddddddd 1ddddddd 0ddddddd

The number is read in big-endian.

So that the result number will be `(A & 0x7f)<<21 | (B & 0x7F)<<14 | (C & 0x7F)<<7 | D`.

If the control byte indicates it should be a negative number, the result will be `0-((A & 0x7f)<<21 | (B & 0x7F)<<14 | (C & 0x7F)<<7 | D)`.

### Row-col swapped array

Imagine that you have an array like this:

```json
[
    {"name": "Jason", "email": "jason@example.com", "phone": "777-777-7777"},
    {"name": "Jackson", "age": 17, "email": "jackson@example.com", "phone": "888-888-8888"}
]
```

It is better to preprocess it before transfer:

```json
{
    "name": ["Jason", "Jackson"],
    "age": [unspecified, 17],
    "email": ["jason@example.com", "jackson@example.com"],
    "phone": ["777-777-7777", "888-888-8888"]
}
```

This is a row-col swapped array. JKSN will transparently transform an array to a row-col swapped array if it takes less space, then transform it back after receiving.

This example, represented in JKSN without row-col swapping, is:

```c
"jk!" 0x82 0x93 0x44 "name" 0x45 "Jason" 0x45 "email" 0x4e 0x11 "jason@example.com"
                0x45 "phone" 0x4e 0x0c "777-777-7777"
           0x94 0x4c 0x2f 0x47 "Jackson" 0x43 "age" 0x1d 0x11
                0x4c 0x84 0x4e 0x13 "jackson@example.com" 0x4c 0xfe 0x4e 0x0c "888-888-8888"
```

If represented in row-col swapped JKSN, is:

```c
"jk!" 0xa4 0x44 "name" 0x82 0x45 "Jason" 0x47 "Jackson"
           0x43 "age" 0xa0 0x1d 0x11
           0x45 "email" 0x4e 0x11 "jason@example.com" 0x4e 0x13 "jackson@example.com"
           0x45 "phone" 0x4e 0x0c "777-777-7777" 0x4e 0x0c "888-888-8888"
```

### Checksum

Checksum is a optional part of JKSN stream. If the transmission media is reliable, or if you decided to GZIP the JKSN stream, checksum can be omitted.

Checksum indicates the checksum from the position immediately after the checksum to the very end of the JKSN stream.

A delayed checksum rearranges the form of JKSN, which puts the checksum to the end of JKSN stream, as the following format:

    [magic header] [0xf8/0xf9/0xfa/0xfb] [control byte] [data bytes] ... [control byte] [data bytes] [checksum]

### Representation decision

An JSON stream has multiple JKSN representation. The JKSN encoder should decide which method generates the smallest JKSN stream.

### License of this document

This document, as well as JKSN specification, is licensed under BSD license.

Copyright (c) 2014 StarBrilliant &lt;m13253@hotmail.com&gt;.
All rights reserved.

Redistribution and use in source and binary forms are permitted
provided that the above copyright notice and this paragraph are
duplicated in all such forms and that any documentation,
advertising materials, and other materials related to such
distribution and use acknowledge that the software was developed
by the <organization>. The name of the
<organization> may not be used to endorse or promote products derived
from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
