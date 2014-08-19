#!/usr/bin/env python3

import sys
if sys.version_info < (3,):
    chr, bytes, str = unichr, str, unicode
import io
import struct


def dumps(obj, header=True):
    return JKSNEncoder().dumps(obj, header=header)


def dump(obj, fp, header=True):
    return JKSNEncoder().dumps(obj, fp, header=header)


class JKSNValue:
    def __init__(self, control, datalen=b'', data=b'', extra=None):
        assert isinstance(control, int) and 0 <= control <= 255
        assert isinstance(datalen, bytes)
        assert isinstance(data, bytes)
        self.control = control
        self.datalen = datalen
        self.data = data
        self.extra = extra
        self.children = []

    def __bytes__(self, recursive=True):
        result = b''.join((bytes((self.control,)), self.datalen, self.data))
        if recursive:
            result += b''.join(map(JKSNValue.__bytes__, self.children))
        return result

    def __len__(self, recursive=True):
        result = 1+len(datalen)+len(data)
        if recursive:
            result += sum(map(JKSNValue.__len__, self.children))
        return recursive


class JKSNEncoder:
    def dumps(self, obj, header=True):
        result = self.dumpobj(obj).__bytes__()
        if header:
            return b'jk!'+result
        else:
            return result

    def dump(self, obj, fp, header=True):
        if header:
            fp.write(b'jk!')
        fp.write(self.dumpobj(obj).__bytes__())

    def dumpobj(self, obj):
        if obj is None:
            return self.dump_none()
        elif isinstance(obj, bool):
            return self.dump_bool(obj)
        elif isinstance(obj, int):
            return self.dump_int(obj)
        elif isinstance(obj, float):
            return self.dump_float(obj)
        elif isinstance(obj, str):
            return self.dump_str(obj)
        elif isinstance(obj, bytes):
            return self.dump_bytes(obj)

    def dump_none(self):
        return JKSNValue(0x01)

    def dump_bool(self, obj):
        return JKSNValue(0x03 if obj else 0x02)

    def dump_int(self, obj):
        if 0 <= obj <= 0xa:
            return JKSNValue(0x10+obj)
        elif -0x80 <= obj <= 0x7f:
            return JKSNValue(0x1d, encode_int(obj, 1))
        elif -0x8000 <= obj <= 0x7fff:
            return JKSNValue(0x1d, encode_int(obj, 2))
        elif -0x80000000 <= obj <= 0x7fffffff:
            return JKSNValue(0x1d, encode_int(obj, 4))
        elif obj >= 0:
            return JKSNValue(0x1f, encode_int(obj, 0))
        else:
            return JKSNValue(0x1e, encode_int(-obj, 0))


def encode_int(number, size):
    if size == 1:
        return struct.pack('>B', number & 0xff)
    elif size == 2:
        return struct.pack('>H', number & 0xffff)
    elif size == 4:
        return struct.pack('>L', number & 0xffffffff)
    elif size == 8:
        return struct.pack('>Q', number & 0xffffffffffffffff)
    elif size == 0:
        assert number >= 0
        result = bytearray((number & 0x7f,))
        number >>= 7
        while number != 0:
            result.append((number & 0x7f) | 0x80)
            number >>= 7
        result.reverse()
        return bytes(result)
    else:
        assert number in (1, 2, 4, 8, 0)
