JKSN specification
==================

JKSN, prononced as "Jackson", is an abbreviation of "JKSN Compressed Serialize Notation", aims to be a binary serialization format compatible with JSON.

JKSN is suitable for the situation that network bandwidth is expensive but processing time is cheap.

JKSN stream may be further compressed with GZIP to get better results.

File formats
------------

    [magic header] [control byte] [checksum] [control byte] [data bytes] [control byte] [data bytes] ...

One JSON stream must only produce one value. So once a complete value is parsed, the JKSN stream must be terminated.

MIME type `application/x-jksn` and filename extension `.jksn` are preferred.

### Magic header

This part contains four bytes `jk!`, it is used for safe Internet transmission and data type recognition. If the network bandwidth is limited, magic header may be omitted.

### Control byte

No data bytes are followed after `0x0n`.

#### Special values:

    0x00: undefined
    0x01: null
    0x02: false
    0x03: true
    0x0f: a representation (a control byte and data bytes) of a string containing a plain text JSON literal is followed

#### Integers:

An integer is followed after `0x1b` `0x1c` `0x1d` `0x1e` `0x1f`.

All integers are big endian.

    0x1n (where 0<=n<=a): represents an integer that is n
    0x1b: a signed 32-bit integer is followed
    0x1c: a signed 16-bit integer is followed
    0x1d: a signed 8-bit integer is followed
    0x1e: a negative variable length integer is followed
    0x1f: a positive variable length integer is followed

#### Float point numbers:

An IEEE 754 float point number is followed after `0x2b` `0x2c` `0x2d`.

The most significant bit, that is the IEEE 754 sign bit, is transferred first.

    0x20: NaN
    0x2b: an IEEE 754 long double (128-bit) number is followed
    0x2c: an IEEE 754 double (64-bit) number is followed
    0x2d: an IEEE 754 float (32-bit) number is followed
    0x2e: -Infinity
    0x2f: Infinity

#### UTF-16 strings:

All UTF-16 strings are little endian, that is for ASCII characters, the second byte is zero.

    0x3n (where 0<=n<=b): a UTF-16 string containing n byte pairs is followed
    0x3c: an unsigned 8-bit integer is followed, representing the nearest previous UTF-16 or UTF-8 string with this DJB Hash value.
    0x3d: an unsigned 16-bit integer and a UTF-16 string containing that amount of byte pairs is followed
    0x3e: an unsigned 8-bit integer and a UTF-16 string containing that amount of byte pairs is followed
    0x3f: a positive variable length integer and a UTF-16 string containing that amount of byte pairs is followed

#### UTF-8 strings:

    0x4n (where 0<=n<=c): a UTF-8 string containing n bytes is followed
    0x4d: an unsigned 16-bit integer and a UTF-8 string containing that amount of bytes is followed
    0x4e: an unsigned 8-bit integer and a UTF-8 string containing that amount of bytes is followed
    0x4f: a positive variable length integer and a UTF-8 string containing that amount of bytes is followed

#### Blob strings:

    0x5n (where 0<=n<=b): a blob string containing n bytes is followed
    0x5c: an unsigned 8-bit integer is followed, representing the nearest previous blob string with this DJB Hash value.
    0x5d: an unsigned 16-bit integer and a blob string containing that amount of bytes is followed
    0x5e: an unsigned 8-bit integer and a blob string containing that amount of bytes is followed
    0x5f: a positive variable length integer and a blob string containing that amount of bytes is followed

#### Hashtable refresher:

Hashtable refresher is an array of string that can be transferred before the value or inside arrays or objects. It does not produce any values but forces the build of a hashtable that can be used with `0x3c` or `0x5c`.

Normally hashtable referesher is not useful since the hashtable is built during the first occurence of a string, however it is useful if there is a persistent connection exchanging continuous JKSN stream.

There are two hashtables with size 256, one for text strings (UTF-16, UTF-8), the other for blob strings.

    0x70: clear the hashtable
    0x7n (where 1<=n<=c): an array of string containing n strings is followed
    0x7d: an unsigned 16-bit integer and an array of string containing that amount of strings is followed
    0x7e: an unsigned 8-bit integer and an array of string containing that amount of strings is followed
    0x7f: a positive variable length integer and an array of string containing that amount of strings is followed

#### Arrays:

Items are nested layers of control bytes and data bytes.

    0x8n (where 0<=n<=c): an array containing n items is followed
    0x8d: an unsigned 16-bit integer and an array containing that amount of items is followed
    0x8e: an unsigned 8-bit integer and an array containing that amount of items is followed
    0x8f: a positive variable length integer and an array containing that amount of items is followed

#### Objects:

    0x9n (where 0<=n<=c): an object containing n string-item pairs is followed
    0x9d: an unsigned 16-bit integer and an object containing that amount of string-item pairs is followed
    0x9e: an unsigned 8-bit integer and an object containing that amount of string-item pairs is followed
    0x9f: a positive variable length integer and an object containing that amount of string-item pairs is followed

#### Row-col swapped arrays:

    0xa0: "unspecified", used to skip columns that are not specified
    0xan (where 1<=n<=c): a row-col swapped array containing n columns is followed
    0xad: an unsigned 16-bit integer and a row-col swapped array containing that amount of columns is followed
    0xae: an unsigned 8-bit integer and a row-col swapped array containing that amount of columns is followed
    0xaf: a positive variable length integer and a row-col swapped array containing that amount of columns is followed

#### Delta encoded integers:

An Integer is followed after `0xbb` `0xbc` `0xbd` `0xbe` `0xbf`, representing an integer that is larger than the previous occurred integer by n.

    0xb0: represents the same previous occurred integer
    0xbn (where 1<=n<=5): represents an integer that is larger than the previous occurred integer by n
    0xbn (where 6<=n<=a): represents an integer that is larger than the previous occurred integer by n-11
    0xbb: a signed 32-bit integer is followed
    0xbc: a signed 16-bit integer is followed
    0xbd: a signed 8-bit integer is followed
    0xbe: a negative variable length integer is followed
    0xbf: a positive variable length integer is followed

#### Checksums:

    0xf0: a CRC32 checksum will be immediately followed
    0xf1: an MD5 checksum will be immediately followed
    0xf2: a SHA-1 checksum will be immediately followed
    0xf3: a SHA-256 checksum will be immediately followed
    0xf4: a SHA-512 checksum will be immediately followed
    0xf8: a delayed CRC32 checksum will be present at the end of the stream
    0xf9: a delayed MD5 checksum will be present at the end of the stream
    0xfa: a delayed SHA-1 checksum will be present at the end of the stream
    0xfb: a delayed SHA-256 checksum will be present at the end of the stream
    0xfc: a delayed SHA-512 checksum will be present at the end of the stream

#### Pragmas:

Pragmas are interpreter-specific directives. They are stored in the form of values, transferred before the real value or inside objects or arrays, but do not produce values. Interpreter that can not understand them may simply ignore them.

    0xff: a representation (a control byte and data bytes) of a value (usually a string) is followed

### Variable length integer

Every byte is separated into two parts, 1 continuation bit and 7 data bits. The last byte has a continuation bit of 0, others have 1.

For example

        A        B        C        D
    1ddddddd 1ddddddd 1ddddddd 0ddddddd

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
                0x45 "phone" 0x4c "777-777-7777"
           0x94 0x3c 0xc1 0x47 "Jackson" 0x43 "age" 0x1d 0x11
                0x3c 0xc8 0x4e 0x13 "jackson@example.com" 0x3c 0x9a 0x4c "888-888-8888"
```

If represented in row-col swapped JKSN, is:

```c
"jk!" 0xa4 0x44 "name" 0x82 0x45 "Jason" 0x47 "Jackson"
           0x43 "age" 0x82 0xa0 0x1d 0x11
           0x45 "email" 0x82 0x4e 0x11 "jason@example.com" 0x4e 0x13 "jackson@example.com"
           0x45 "phone" 0x82 0x4c "777-777-7777" 0x4c "888-888-8888"
```

Row-col swapped array can definitely be nested. However, nested row-col swapped array might cost too much computing time. It is recommended to choose a balanced nesting depth.

### DJB Hash

The algorithm of DJB Hash is listed below:

```python
def DJBHash(string):
    hash = 0
    for i in string:
        hash = hash + (hash << 5) + ord(i)
    return hash & 0xff
```

When hashing UTF-16 strings, the hash function processes every byte instead of every byte pair.

### Checksum

Checksum is a optional part of JKSN stream. If the transmission media is reliable, or if you decided to GZIP the JKSN stream, checksum may be omitted.

Checksum indicates the checksum from the position immediately after the checksum to the very end of the JKSN stream.

A delayed checksum rearranges the form of JKSN, which puts the checksum to the end of JKSN stream, as the following format:

    [magic header] [0xf8/0xf9/0xfa/0xfb/0xfc] [control byte] [data bytes] ... [control byte] [data bytes] [checksum]

### Representation decision

An JSON stream has multiple JKSN representation. The JKSN encoder should decide which method generates the smallest JKSN stream in reasonable time.

Since implementing the full functionality of JKSN costs too much resources and does not suit all situation, the application may only implement a subset of JKSN specification. This requires the sender and receiver using features both supported.

### Comparison

Take the example used in row-col swapping section.

| Compression method                     | Size          | Percent saved     |
| -------------------------------------- | ------------- | ----------------- |
|   JSON, not stripping whitespace       |   170 bytes   |   13.3% larger    |
| **JSON, stripped whitespace**          |   150 bytes   |   base            |
|   JSON, row-col swapped, stripped      |   143 bytes   |   4.6% smaller    |
| **JSON, stripped, gzip -9**            |   112 bytes   |   25.3% smaller   |
|   JSON, swapped, stripped, gzip -9     |   127 bytes   |   15.3% smaller   |
|   JKSN, no swapping                    |   112 bytes   |   25.3% smaller   |
|   JKSN, transparent swapped            |   109 bytes   |   27.3% smaller   |
|   JKSN, no swap, gzip -9               |   111 bytes   |   26.0% smaller   |
| **JKSN, transparent swapped, gzip -9** | **107 bytes** | **28.7% smaller** |

A non gzipped JKSN does even better than JSON compressed in any method above.

What about BSON, MessagePack, BJSON, ubJSON?

| Format                                       | Size      | Implementation                                       |
| -------------------------------------------- | --------- | ---------------------------------------------------- |
| BSON                                         | 172 bytes | [Python 3](http://bsonspec.org/implementations.html) |
| Universal Binary Json                        | 150 bytes | [Node.JS](https://github.com/Sannis/node-ubjson)     |
| MessagePack                                  | 120 bytes | [Node.JS](https://github.com/msgpack/msgpack-node)   |
| JSONH                                        | 116 bytes | [Python 2](https://github.com/WebReflection/JSONH)   |
| BJSON with builtin adaptive huffman encoding | 115 bytes | [Node.JS](https://github.com/asterick/BJSON)         |
| **JKSN**                                     | 109 bytes | [Python 3](https://github.com/m13253/JKSN)           |

### Reference implementation

There is a Python reference implementation in the `python` directory in this repository.

There are other implementations in other languages as well. Some of them implemented a part of the functionality.

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
