#!/usr/bin/env python3

import sys
if sys.version_info < (3,):
    chr, bytes, str = unichr, str, unicode
import collections
import io
import json
import math
import struct


class JKSNError(ValueError):
    pass


class JKSNEncodeError(JKSNError):
    pass


class JKSNDecodeError(JKSNError):
    pass


def dumps(obj, header=True, check_circular=True):
    return JKSNEncoder().dumps(obj, header=header, check_circular=check_circular)


def dump(obj, fp, header=True, check_circular=True):
    return JKSNEncoder().dumps(obj, fp, header=header, check_circular=check_circular)


def loads(s):
    return JKSNDecoder().loads(s)

def load(fp):
    return JKSNDecoder().load(fp)

class JKSNValue:
    def __init__(self, control, data=b'', buf=b'', origin=None):
        assert isinstance(control, int) and 0 <= control <= 255
        assert isinstance(data, bytes)
        assert isinstance(buf, bytes)
        self.control = control
        self.data = data
        self.buf = buf
        self.origin = origin
        self.children = []
        self.hash = None

    def __bytes__(self, recursive=True):
        result = b''.join((bytes((self.control,)), self.data, self.buf))
        if recursive:
            result += b''.join(map(JKSNValue.__bytes__, self.children))
        return result

    def __len__(self, depth=0):
        result = 1 + len(self.data) + len(self.buf)
        if depth != 1:
            result += sum((JKSNValue.__len__(i, depth-1) for i in self.children))
        return result


class _unspecified_value:
    pass


class JKSNEncoder:
    def __init__(self):
        self.lastint = None
        self.texthash = [None] * 256
        self.blobhash = [None] * 256

    def dumps(self, obj, header=True, check_circular=True):
        result = self.dumpobj(obj, check_circular=check_circular).__bytes__()
        if header:
            return b'jk!' + result
        else:
            return result

    def dump(self, obj, fp, header=True, check_circular=True):
        result = self.dumpobj(obj, check_circular=check_circular).__bytes__()
        if header:
            fp.write(b'jk!')
        fp.write(result)

    def dumpobj(self, obj, check_circular=True):
        try:
            self.circular = set() if check_circular else None
            return self._optimize(self.dump_value(obj))
        finally:
            self.circular = None

    def dump_value(self, obj):
        if obj is None:
            return self.dump_none(obj)
        elif isinstance(obj, _unspecified_value):
            return self.dump_unspecified(obj)
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
        elif isinstance(obj, (list, tuple, set)):
            if self.circular is not None:
                if id(obj) in self.circular:
                    raise JKSNEncodeError('circular reference detected during JKSN encoding')
                self.circular.add(id(obj))
            return self.dump_list(obj)
        elif isinstance(obj, dict):
            if self.circular is not None:
                if id(obj) in self.circular:
                    raise JKSNEncodeError('circular reference detected during JKSN encoding')
                self.circular.add(id(obj))
            return self.dump_dict(obj)
        else:
            raise JKSNEncodeError('cannot encode JKSN from value: %r' % obj)

    def dump_none(self, obj):
        return JKSNValue(0x01, origin=obj)

    def dump_unspecified(self, obj):
        return JKSNValue(0xa0, origin=obj)

    def dump_bool(self, obj):
        return JKSNValue(0x03 if obj else 0x02, origin=obj)

    def dump_int(self, obj):
        if 0 <= obj <= 0xa:
            return JKSNValue(0x10 | obj, origin=obj)
        elif -0x80 <= obj <= 0x7f:
            return JKSNValue(0x1d, self._encode_int(obj, 1), origin=obj)
        elif -0x8000 <= obj <= 0x7fff:
            return JKSNValue(0x1c, self._encode_int(obj, 2), origin=obj)
        elif -0x80000000 <= obj <= 0x7fffffff:
            return JKSNValue(0x1b, self._encode_int(obj, 4), origin=obj)
        elif obj >= 0:
            return JKSNValue(0x1f, self._encode_int(obj, 0), origin=obj)
        else:
            return JKSNValue(0x1e, self._encode_int(-obj, 0), origin=obj)

    def dump_float(self, obj):
        if math.isnan(obj):
            return JKSNValue(0x20, origin=obj)
        elif math.isinf(obj):
            return JKSNValue(0x2f if obj >= 0 else 0x2e, origin=obj)
        else:
            return JKSNValue(0x2c, struct.pack('>d', obj), origin=obj)

    def dump_str(self, obj):
        obj_utf16 = str.encode(obj, 'utf-16-le')
        obj_utf8 = str.encode(obj, 'utf-8')
        obj_short, control = (obj_utf16, 0x30) if len(obj_utf16) < len(obj_utf8) else (obj_utf8, 0x40)
        if len(obj_short) <= (0xc if control == 0x40 else 0xb):
            result = JKSNValue(control | len(obj_short), b'', obj_short, origin=obj)
        elif len(obj_short) <= 0xff:
            result = JKSNValue(control | 0xe, self._encode_int(len(obj_short), 1), obj_short, origin=obj)
        elif len(obj_short) <= 0xffff:
            result = JKSNValue(control | 0xd, self._encode_int(len(obj_short), 2), obj_short, origin=obj)
        else:
            result = JKSNValue(control | 0xf, self._encode_int(len(obj_short), 0), obj_short, origin=obj)
        result.hash = _djb_hash(obj_short)
        return result

    def dump_bytes(self, obj):
        if len(obj) <= 0xb:
            result = JKSNValue(0x50 | len(obj), b'', obj, origin=obj)
        elif len(obj) <= 0xff:
            result = JKSNValue(0x5e, self._encode_int(len(obj), 1), obj, origin=obj)
        elif len(obj) <= 0xffff:
            result = JKSNValue(0x5d, self._encode_int(len(obj), 2), obj, origin=obj)
        else:
            result = JKSNValue(0x5f, self._encode_int(len(obj), 0), obj, origin=obj)
        result.hash = _djb_hash(obj)
        return result

    def dump_list(self, obj):

        def test_swap_availability(obj):
            columns = False
            for row in obj:
                if not isinstance(row, dict):
                    return False
                else:
                    columns = columns or len(row) != 0
            return columns

        def encode_list_straight(obj):
            length = len(obj)
            if length <= 0xc:
                result = JKSNValue(0x80 | length, origin=obj)
            elif length <= 0xff:
                result = JKSNValue(0x8e, self._encode_int(length, 1), origin=obj)
            elif length <= 0xffff:
                result = JKSNValue(0x8d, self._encode_int(length, 2), origin=obj)
            else:
                result = JKSNValue(0x8f, self._encode_int(length, 0), origin=obj)
            for item in obj:
                result.children.append(self.dump_value(item))
            assert len(result.children) == length
            return result

        def encode_list_swapped(obj):
            columns = collections.OrderedDict().fromkeys((column for row in obj for column in row))
            if len(columns) <= 0xc:
                result = JKSNValue(0xa0 | len(columns), origin=obj)
            elif len(columns) <= 0xff:
                result = JKSNValue(0xae, self._encode_int(length, 1), origin=obj)
            elif len(columns) <= 0xffff:
                result = JKSNValue(0xad, self._encode_int(length, 2), origin=obj)
            else:
                result = JKSNValue(0xaf, self._encode_int(length, 0), origin=obj)
            for column in columns:
                result.children.append(self.dump_value(column))
                result.children.append(self.dump_list([row.get(column, _unspecified_value()) for row in obj]))
            assert len(result.children) == len(columns) * 2
            return result

        result = encode_list_straight(obj)
        if test_swap_availability(obj):
            result_swapped = encode_list_swapped(obj)
            if result_swapped.__len__(3) < result.__len__(3):
                result = result_swapped
        return result

    def dump_dict(self, obj):
        length = len(obj)
        if length <= 0xc:
            result = JKSNValue(0x90 | length, origin=obj)
        elif length <= 0xff:
            result = JKSNValue(0x9e, self._encode_int(length, 1), origin=obj)
        elif length <= 0xffff:
            result = JKSNValue(0x9d, self._encode_int(length, 2), origin=obj)
        else:
            result = JKSNValue(0x9f, self._encode_int(length, 0), origin=obj)
        for key, value in obj.items():
            result.children.append(self.dump_value(key))
            result.children.append(self.dump_value(value))
        assert len(result.children) == length * 2
        return result

    def _optimize(self, obj):
        control = obj.control & 0xf0
        if control == 0x10:
            if self.lastint is not None:
                delta = obj.origin - self.lastint
                if abs(delta) < abs(obj.origin):
                    if delta <= 0xa:
                        new_control, new_data = 0xb0 | delta, b''
                    elif -0x80 <= delta <= 0x7f:
                        new_control, new_data = 0xbd, self._encode_int(delta, 1)
                    elif -0x8000 <= delta <= 0x7fff:
                        new_control, new_data = 0xbc, self._encode_int(delta, 2)
                    elif -0x80000000 <= delta <= 0x7fffffff:
                        new_control, new_data = 0xbb, self._encode_int(delta, 4)
                    elif delta >= 0:
                        new_control, new_data = 0xbf, self._encode_int(delta, 0)
                    else:
                        new_control, new_data = 0xbe, self._encode_int(-delta, 0)
                    if len(new_data) < len(obj.data):
                        obj.control, obj.data = new_control, new_data
            self.lastint = obj.origin
        elif control in (0x30, 0x40):
            if len(obj.buf) > 1:
                cached = self.texthash[obj.hash]
                if cached is not None and cached.buf == obj.buf:
                    obj.control, obj.data, obj.buf = 0x3c, self._encode_int(obj.hash, 1), b''
                else:
                    self.texthash[obj.hash] = obj
        elif control == 0x50:
            if len(obj.buf) > 1:
                cached = self.blobhash[obj.hash]
                if cached is not None and cached.buf == obj.buf:
                    obj.control, obj.data, obj.buf = 0x5c, self._encode_int(obj.hash, 1), b''
                else:
                    self.blobhash[obj.hash] = obj
        else:
            for child in obj.children:
                self._optimize(child)
        return obj

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
            assert size in (1, 2, 4, 0)


class JKSNDecoder:
    def __init__(self):
        self.lastint = None
        self.texthash = [None] * 256
        self.blobhash = [None] * 256

    def loads(self, s):
        return self.load(io.BytesIO(s))

    def load(self, fp):
        typetest = fp.read(0)
        if not isinstance(typetest, bytes):
            raise TypeError('%r does not support the buffer interface' % typetest.__name__)
        header = fp.read(3)
        if header != b'jk!':
            fp.seek(-3, 1)
        return self.loadobj(fp)

    def loadobj(self, fp):
        while True:
            control = ord(fp.read(1))
            if control in (0x00, 0x01):
                return None
            elif control == 0x02:
                return False
            elif control == 0x03:
                return True
            elif control == 0x0f:
                s = loadobj(fp)
                if not isinstance(s, str):
                    raise JKSNDecodeError('JKSN value 0x0f requires a string but found: %r' % s)
                return json.loads(s)
            elif control in range(0x10, 0x1b):
                return control & 0xf
            elif control == 0x1b:
                self.lastint = self._unsigned_to_signed(self._decode_int(fp, 4), 32)
                return self.lastint
            elif control == 0x1c:
                self.lastint = self._unsigned_to_signed(self._decode_int(fp, 2), 16)
                return self.lastint
            elif control == 0x1d:
                self.lastint = self._unsigned_to_signed(self._decode_int(fp, 1), 8)
                return self.lastint
            elif control == 0x1e:
                self.lastint = -self._decode_int(fp, 0)
                return self.lastint
            elif control == 0x1f:
                self.lastint = self._decode_int(fp, 0)
                return self.lastint
            elif control == 0x20:
                return float('nan')
            elif control == 0x2b:
                raise NotImplementedError('This JKSN decoder does not support long double numbers')
            elif control == 0x2c:
                return struct.unpack('>d', fp.read(8))[0]
            elif control == 0x2d:
                return struct.unpack('>f', fp.read(4))[0]
            elif control == 0x2e:
                return float('-inf')
            elif control == 0x2f:
                return float('inf')
            elif control in range(0x30, 0x3c):
                return self.load_str(fp, control & 0xf, 'utf-16-le')
            elif control == 0x3c:
                hashvalue = ord(fp.read(1))
                if self.texthash[hashvalue] is not None:
                    return self.texthash[hashvalue]
                else:
                    raise JKSNDecodeError('JKSN stream requires a non-existing hash: 0x%02x' % hashvalue)
            elif control == 0x3d:
                return self.load_str(fp, self._decode_int(fp, 2), 'utf-16-le')
            elif control == 0x3e:
                return self.load_str(fp, self._decode_int(fp, 1), 'utf-16-le')
            elif control == 0x3f:
                return self.load_str(fp, self._decode_int(fp, 0), 'utf-16-le')
            elif control in range(0x40, 0x4d):
                return self.load_str(fp, control & 0xf, 'utf-8')
            elif control == 0x4d:
                return self.load_str(fp, self._decode_int(fp, 2), 'utf-8')
            elif control == 0x4e:
                return self.load_str(fp, self._decode_int(fp, 1), 'utf-8')
            elif control == 0x4f:
                return self.load_str(fp, self._decode_int(fp, 0), 'utf-8')
            elif control in range(0x50, 0x5c):
                return self.load_str(fp, control & 0xf)
            elif control == 0x5c:
                hashvalue = ord(fp.read(1))
                if self.blobhash[hashvalue] is not None:
                    return self.blobhash[hashvalue]
                else:
                    raise JKSNDecodeError('JKSN stream requires a non-existing hash: 0x%02x' % hashvalue)
            elif control == 0x5d:
                return self.load_str(fp, self._decode_int(fp, 2))
            elif control == 0x5e:
                return self.load_str(fp, self._decode_int(fp, 1))
            elif control == 0x5f:
                return self.load_str(fp, self._decode_int(fp, 0))
            elif control == 0x70:
                self.texthash = [None] * 256
                self.blobhash = [None] * 256
            elif control in range(0x71, 0x7d):
                for i in range(control & 0xf):
                    self.loadobj(fp)
            elif control == 0x7d:
                for i in range(self._decode_int(fp, 2)):
                    self.loadobj(fp)
            elif control == 0x7e:
                for i in range(self._decode_int(fp, 1)):
                    self.loadobj(fp)
            elif control == 0x7f:
                for i in range(self._decode_int(fp, 0)):
                    self.loadobj(fp)
            elif control in range(0x80, 0x8d):
                return [self.loadobj(fp) for i in range(control & 0xf)]
            elif control == 0x8d:
                return [self.loadobj(fp) for i in range(self._decode_int(fp, 2))]
            elif control == 0x8e:
                return [self.loadobj(fp) for i in range(self._decode_int(fp, 1))]
            elif control == 0x8f:
                return [self.loadobj(fp) for i in range(self._decode_int(fp, 0))]
            elif control in range(0x90, 0x9d):
                return collections.OrderedDict((self.loadobj(fp), self.loadobj(fp)) for i in range(control & 0xf))
            elif control == 0x9d:
                return collections.OrderedDict((self.loadobj(fp), self.loadobj(fp)) for i in range(self._decode_int(fp, 2)))
            elif control == 0x9e:
                return collections.OrderedDict((self.loadobj(fp), self.loadobj(fp)) for i in range(self._decode_int(fp, 1)))
            elif control == 0x9f:
                return collections.OrderedDict((self.loadobj(fp), self.loadobj(fp)) for i in range(self._decode_int(fp, 0)))
            elif control in range(0xb0, 0xbb):
                if self.lastint is not None:
                    self.lastint += control & 0xf
                    return self.lastint
                else:
                    raise JKSNDecodeError('JKSN stream contains an invalid delta encoded integer')
            elif control == 0xbb:
                if self.lastint is not None:
                    self.lastint += self._decode_int(fp, 4)
                    return self.lastint
                else:
                    raise JKSNDecodeError('JKSN stream contains an invalid delta encoded integer')
            elif control == 0xbc:
                if self.lastint is not None:
                    self.lastint += self._decode_int(fp, 2)
                    return self.lastint
                else:
                    raise JKSNDecodeError('JKSN stream contains an invalid delta encoded integer')
            elif control == 0xbd:
                if self.lastint is not None:
                    self.lastint += self._decode_int(fp, 1)
                    return self.lastint
                else:
                    raise JKSNDecodeError('JKSN stream contains an invalid delta encoded integer')
            elif control == 0xbe:
                if self.lastint is not None:
                    self.lastint -= self._decode_int(fp, 0)
                    return self.lastint
                else:
                    raise JKSNDecodeError('JKSN stream contains an invalid delta encoded integer')
            elif control == 0xbf:
                if self.lastint is not None:
                    self.lastint += self._decode_int(fp, 0)
                    return self.lastint
                else:
                    raise JKSNDecodeError('JKSN stream contains an invalid delta encoded integer')
            # Ignore checksums
            elif control == 0xf0:
                fp.read(4)
            elif control == 0xf1:
                fp.read(16)
            elif control == 0xf2:
                fp.read(20)
            elif control == 0xf3:
                fp.read(32)
            elif control == 0xf4:
                fp.read(64)
            elif control == 0xf8:
                result = self.loadobj(fp)
                fp.read(4)
                return result
            elif control == 0xf9:
                result = self.loadobj(fp)
                fp.read(16)
                return result
            elif control == 0xfa:
                result = self.loadobj(fp)
                fp.read(20)
                return result
            elif control == 0xfb:
                result = self.loadobj(fp)
                fp.read(32)
                return result
            elif control == 0xfc:
                result = self.loadobj(fp)
                fp.read(64)
                return result
            elif control == 0xff:
                self.loadobj(fp)
            else:
                raise JKSNDecodeError('cannot decode JKSN from byte 0x%02x' % control)

    def load_str(self, fp, length, encoding=None):
        buf = fp.read(length)
        if encoding is not None:
            result = buf.decode(encoding)
            self.texthash[_djb_hash(buf)] = result
        else:
            result = buf
            self.blobhash[_djb_hash(buf)] = result
        return result

    @staticmethod
    def _decode_int(fp, size):
        if size == 1:
            return struct.unpack('>B', fp.read(size))[0]
        elif size == 2:
            return struct.unpack('>H', fp.read(size))[0]
        elif size == 4:
            return struct.unpack('>L', fp.read(size))[0]
        elif size == 0:
            result = 0
            thisbyte = -1
            while thisbyte & 0x80:
                thisbyte = ord(fp.read(1))
                result = (result << 7) | (thisbyte & 0x7f)
            return result
        else:
            assert size in (1, 2, 4, 0)

    @staticmethod
    def _unsigned_to_signed(x, bits):
        return x - ((x >> (bits - 1)) << bits)


def _djb_hash(obj):
    result = 0
    if sys.version_info < (3,):
        for i in obj:
            result += (result << 5) + ord(i)
    else:
        for i in obj:
            result += (result << 5) + i
    return result & 0xff
