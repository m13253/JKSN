#!/usr/bin/env python3

import sys
if sys.version_info < (3,):
    chr, bytes, str = unichr, str, unicode
import io
import math
import struct


def dumps(obj, header=True):
    return JKSNEncoder().dumps(obj, header=header)


def dump(obj, fp, header=True):
    return JKSNEncoder().dumps(obj, fp, header=header)


class JKSNValue:
    def __init__(self, control, data=b'', buf=b'', extra=None):
        assert isinstance(control, int) and 0 <= control <= 255
        assert isinstance(data, bytes)
        assert isinstance(buf, bytes)
        self.control = control
        self.data = data
        self.buf = buf
        self.extra = extra
        self.children = []
        self.hash = None

    def __bytes__(self, recursive=True):
        result = b''.join((bytes((self.control,)), self.data, self.buf))
        if recursive:
            result += b''.join(map(JKSNValue.__bytes__, self.children))
        return result

    def __len__(self, depth=0):
        result = 1 + len(data) + len(buf)
        if depth != 1:
            result += sum((JKSNValue.__len__(i, depth-1) for i in self.children))
        return recursive


class JKSNEncoder:
    def __init__(self):
        self.hashtable = [None] * 256

    def dumps(self, obj, header=True):
        result = self.dumpobj(obj).__bytes__()
        if header:
            return b'jk!' + result
        else:
            return result

    def dump(self, obj, fp, header=True):
        if header:
            fp.write(b'jk!')
        fp.write(self.dumpobj(obj).__bytes__())

    def dumpobj(self, obj):
        return self.dump_value(obj)

    def dump_value(self, obj):
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
        elif isinstance(obj, (list, tuple)):
            return self.dump_list(obj)
        elif isinstance(obj, dict):
            return self.dump_dict(obj)
        elif isinstance(obj, set):
            return self.dump_list(list(obj))
        else:
            raise ValueError('can not encode JKSN from value: %r' % obj)

    def dump_none(self):
        return JKSNValue(0x01)

    def dump_bool(self, obj):
        return JKSNValue(0x03 if obj else 0x02)

    def dump_int(self, obj):
        if 0 <= obj <= 0xa:
            return JKSNValue(0x10 | obj)
        elif -0x80 <= obj <= 0x7f:
            return JKSNValue(0x1d, self._encode_int(obj, 1))
        elif -0x8000 <= obj <= 0x7fff:
            return JKSNValue(0x1d, self._encode_int(obj, 2))
        elif -0x80000000 <= obj <= 0x7fffffff:
            return JKSNValue(0x1d, self._encode_int(obj, 4))
        elif obj >= 0:
            return JKSNValue(0x1f, self._encode_int(obj, 0))
        else:
            return JKSNValue(0x1e, self._encode_int(-obj, 0))

    def dump_float(self, obj):
        if math.isnan(obj):
            return JKSNValue(0x20)
        elif math.isinf(obj):
            return JKSNValue(0x2f if obj >= 0 else 0x2e)
        else:
            return JKSNValue(0x2c, struct.pack('>d', obj))

    def dump_str(self, obj):
        obj_utf16 = str.encode(obj, 'utf-16-le')
        obj_utf8 = str.encode(obj, 'utf-8')
        obj_short, control = (obj_utf16, 0x30) if len(obj_utf16) < len(obj_utf8) else (obj_utf8, 0x40)
        if len(obj_short) <= (0xc if control == 0x40 else 0xb):
            result = JKSNValue(control | len(obj_short), b'', obj_short)
        elif len(obj_short) <= 0xff:
            result = JKSNValue(control | 0xe, self._encode_int(len(obj_short), 1), obj_short)
        elif len(obj_short) <= 0xffff:
            result = JKSNValue(control | 0xd, self._encode_int(len(obj_short), 2), obj_short)
        else:
            result = JKSNValue(control | 0xf, self._encode_int(len(obj_short), 0), obj_short)
        result.hash = _djb_hash(obj_short)
        return result

    def dump_bytes(self, obj):
        if len(obj) <= 0xb:
            result = JKSNValue(0x50 | len(obj), b'', obj)
        elif len(obj) <= 0xff:
            result = JKSNValue(0x5e, self._encode_int(len(obj), 1), obj)
        elif len(obj) <= 0xffff:
            result = JKSNValue(0x5d, self._encode_int(len(obj), 2), obj)
        else:
            result = JKSNValue(0x5f, self._encode_int(len(obj), 0), obj)
        result.hash = _djb_hash(obj)
        return result

    def dump_dict(self, obj):
        length = len(obj)
        if length <= 0xc:
            result = JKSNValue(0x90 | length)
        elif length <= 0xff:
            result = JKSNValue(0x9e, self._encode_int(length, 1))
        elif length <= 0xffff:
            result = JKSNValue(0x9d, self._encode_int(length, 2))
        else:
            result = JKSNValue(0x9d, self._encode_int(length, 0))
        for key, value in dict.items(obj):
            result.children.append(self.dump_value(key))
            result.children.append(self.dump_value(value))
        assert len(result.children) == length * 2
        return result

    @staticmethod
    def _encode_int(number, size):
        if size == 1:
            return struct.pack('>B', number & 0xff)
        elif size == 2:
            return struct.pack('>H', number & 0xffff)
        elif size == 4:
            return struct.pack('>L', number & 0xffffffff)
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
            assert number in (1, 2, 4, 0)


def _djb_hash(obj):
    result = 0
    if sys.version_info < (3,):
        for i in obj:
            result += (result << 5) + ord(i)
    else:
        for i in obj:
            result += (result << 5) + i
    return result & 0xff
