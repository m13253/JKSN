JKSN specification
==================

JKSN, prononced as "Jackson", is an abbreviation of Jackson Compressed Serialize Notation, aims to be a binary serialization format compatible with JSON.

JKSN stream can be further compressed with GZIP to get better results.

File formats
------------

    [magic header] [control byte] [checksum] [control byte] [data bytes] [control byte] [data bytes] ...

One JSON stream can only produce one value. So once a complete value is parsed, the JKSN stream should be terminated.

### Magic header

This part contains four bytes `jk!`, it is used for safe Internet transmission and data type recognition. If the network bandwidth is limited, magic header can be omitted.

### Control byte

No data bytes are followed after `0x0n`.

Special values:

    0x00: undefined
    0x01: null
    0x02: false
    0x03: true

Integers:

An Integer is followed after `0x1b` `0x1c` `0x1d` `0x1e` `0x1f`.

    0x1n (where 0<=n<=a): represents a integer that is n
    0x1b: a signed 32-bit integer is followed
    0x1c: a signed 16-bit integer is followed
    0x1d: a signed 8-bit integer is followed
    0x1e: a negative variable length integer is followed
    0x1f: a positive variable length integer is followed

Float point numbers:

A float point number is followed after `0x2b` `0x2c` `0x2d`.

    0x20: NaN
    0x21: a string control byte and a string containing a plain text JSON literal are followed
    0x2b: an IEEE long double (128-bit) number is followed
    0x2c: an IEEE double (64-bit) number is followed
    0x2d: an IEEE float (32-bit) number is followed
    0x2e: -Infinity
    0x2f: Infinity

UTF-16 strings:

    0x3n (where 0<=n<=b): a little-endian UTF-16 string containing n byte pairs is followed
    0x3c: an unsigned 8-bit integer is followed, representing the nearest previous little-endian UTF-16 string with this BKDR Hash (seed=131, one byte pair as a unit) value.
    0x3d: an unsigned 16-bit integer and a little-endian UTF-16 string containing that amount of byte pairs is followed
    0x3e: an unsigned 8-bit integer and a little-endian UTF-16 string containing that amount of byte pairs is followed
    0x3f: a positive variable length integer and a little-endian UTF-16 string containing that amount of byte pairs is followed

UTF-8 strings:

    0x4n (where 0<=n<=b): a UTF-8 string containing n bytes is followed
    0x4c: an unsigned 8-bit integer is followed, representing the nearest previous UTF-8 string with this BKDR Hash (seed=131) value.
    0x4d: an unsigned 16-bit integer and a UTF-8 string containing that amount of bytes is followed
    0x4e: an unsigned 8-bit integer and a UTF-8 string containing that amount of bytes is followed
    0x4f: a positive variable length integer and a UTF-8 string containing that amount of bytes is followed

Blob strings:

    0x5n (where 0<=n<=b): a blob string containing n bytes is followed
    0x5c: an unsigned 8-bit integer is followed, representing the nearest previous blob string with this BKDR Hash (seed=131) value.
    0x5d: an unsigned 16-bit integer and a blob string containing that amount of bytes is followed
    0x5e: an unsigned 8-bit integer and a blob string containing that amount of bytes is followed
    0x5f: a positive variable length integer and a blob string containing that amount of bytes is followed

Arrays:

Items are nested layers of control bytes and data bytes.

    0x8n (where 0<=n<=c): an array containing n items is followed
    0x8d: an unsigned 16-bit integer and an array containing that amount of items is followed
    0x8e: an unsigned 8-bit integer and an array containing that amount of items is followed
    0x8f: a positive variable length integer and an array containing that amount of items is followed

Objects:

    0x9n (where 0<=n<=c): an object containing n string-item pairs is followed
    0x9d: an unsigned 16-bit integer and an object containing that amount of string-item pairs is followed
    0x9e: an unsigned 8-bit integer and an object containing that amount of string-item pairs is followed
    0x9f: a positive variable length integer and an object containing that amount of string-item pairs is followed

Row-col swapped array:

    0xa0: "unspecified", used to skip columns that are not specified
    0xan (where 1<=n<=c): a row-col swapped array containing n columns is followed
    0xad: an unsigned 16-bit integer and a row-col swapped array containing that amount of columns is followed
    0xae: an unsigned 16-bit integer and a row-col swapped array containing that amount of columns is followed
    0xaf: a positive variable length integer and a row-col swapped array containing that amount of columns is followed

Hashtable refresher:

Hashtable refresher is an array of string that can be transferred before the value or inside arrays or objects. It does not produce any values but forces the build of a hashtable that can be used with `0x3c` `0x4c` `0x5c`.

Normally hashtable referesher is not useful since the hashtable is built during the first occurence of a string, however it is useful if there is a persistent connection exchanging continuous JKSN stream.

    0xb0: clear the hashtable
    0xbn (where 1<=n<=c): an array of string containing n strings is followed
    0xbd: an unsigned 16-bit integer and an array of string containing that amount of strings is followed
    0xbe: an unsigned 16-bit integer and an array of string containing that amount of strings is followed
    0xbf: a positive variable length integer and an array of string containing that amount of strings is followed

Checksums:

    0xe0: a CRC32 checksum will be immediately followed
    0xe1: an MD5 checksum will be immediately followed
    0xe2: a SHA-1 checksum will be immediately followed
    0xe3: a SHA-2 checksum will be immediately followed
    0xe8: a delayed CRC32 checksum will be present at the end of the stream
    0xe9: a delayed MD5 checksum will be present at the end of the stream
    0xea: a delayed SHA-1 checksum will be present at the end of the stream
    0xeb: a delayed SHA-2 checksum will be present at the end of the stream

Pragmas:

Pragmas are strings that can be transferred before the value or inside arrays or objects. They are much like comments, which do not produce any values. They contain interpreter-specific directives. Interpreter that can not understand them can simply ignore them.

    0xfn (where 0<=n<=b): a pragma string containing n bytes are followed
    0xfc: an unsigned 8-bit integer is followed, representing the nearest previous pragma string with this BKDR Hash (seed=131) value.
    0xfd: an unsigned 16-bit integer and a pragma string containing that amount of bytes are followed
    0xfe: an unsigned 8-bit integer and a pragma string containing that amount of bytes are followed
    0xff: a positive variable length integer and a pragma string containing that amount of bytes are followed

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

Row-col swapped array can definitely be nested. However, nested row-col swapped array might cost too much computing time. It is recommended to choose a balanced nesting depth.

### Checksum

Checksum is a optional part of JKSN stream. If the transmission media is reliable, or if you decided to GZIP the JKSN stream, checksum can be omitted.

Checksum indicates the checksum from the position immediately after the checksum to the very end of the JKSN stream.

A delayed checksum rearranges the form of JKSN, which puts the checksum to the end of JKSN stream, as the following format:

    [magic header] [0xe8/0xe9/0xea/0xeb] [control byte] [data bytes] ... [control byte] [data bytes] [checksum]

### Representation decision

An JSON stream has multiple JKSN representation. The JKSN encoder should decide which method generates the smallest JKSN stream.

### Comparison

Take the example used in row-col swapping.

| Compression method                 | Size      | Percent saved |
| ---------------------------------- | --------- | ------------- |
| JSON, not stripping whitespace     | 170 bytes | 13.3% larger  |
| JSON, stripped whitespace          | 150 bytes | base          |
| JSON, row-col swapped, stripped    | 143 bytes | 4.6% smaller  |
| JSON, stripped, gzip -9            | 112 bytes | 25.3% smaller |
| JSON, swapped, stripped, gzip -9   | 127 bytes | 15.3% smaller |
| JKSN, no swapping                  | 114 bytes | 24.0% smaller |
| JKSN, transparent swapped          | 108 bytes | 28.0% smaller |
| JKSN, no swap, gzip -9             | 113 bytes | 24.7% smaller |
| JKSN, transparent swapped, gzip -9 | 106 bytes | 29.3% smaller |

A non gzipped JKSN does even better than JSON compressed in any method.

### License of this document

This document, as well as JKSN specification, is licensed under BSD license.

Copyright (c) 2014 StarBrilliant &lt;m13253@hotmail.com&gt;.
All rights reserved.

Redistribution and use in source and binary forms are permitted
provided that the above copyright notice and this paragraph are
duplicated in all such forms and that any documentation,
advertising materials, and other materials related to such
distribution and use acknowledge that the software was developed by
StarBrilliant.
The name of StarBrilliant may not be used to endorse or promote
products derived from this software without specific prior written
permission.

THIS SOFTWARE IS PROVIDED **AS IS** AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
