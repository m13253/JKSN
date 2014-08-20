#!/usr/bin/env python3

# Copyright (c) 2014 StarBrilliant <m13253@hotmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that the above copyright notice and this paragraph are
# duplicated in all such forms and that any documentation,
# advertising materials, and other materials related to such
# distribution and use acknowledge that the software was developed by
# StarBrilliant.
# The name of StarBrilliant may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

import sys
import collections
import hashlib
import io
import json
import math
import struct
import zlib


if sys.version_info < (3,):
    bytes, str = str, unicode
    bytechr = chr
else:
    bytechr = lambda i: bytes((i,))


class JKSNError(ValueError):
    '''Exception class raised during JKSN encoding and decoding'''
    pass


class JKSNEncodeError(JKSNError):
    '''Exception class raised during JKSN encoding'''
    pass


class JKSNDecodeError(JKSNError):
    '''Exception class raised during JKSN decoding'''
    pass


class JKSNChecksumError(JKSNDecodeError):
    '''This version of JKSN decoder does not verify checksum'''
    pass


def dumps(obj, header=True, check_circular=True):
    '''Dump an object into a buffer'''
    return JKSNEncoder().dumps(obj, header=header, check_circular=check_circular)


def dump(obj, fp, header=True, check_circular=True):
    '''Dump an object into a file object'''
    return JKSNEncoder().dump(obj, fp, header=header, check_circular=check_circular)


def loads(s, header=True, ordered_dict=False):
    '''Load an object from a buffer'''
    return JKSNDecoder().loads(s, header=header, ordered_dict=ordered_dict)


def load(fp, header=True, ordered_dict=False):
    '''Load an object from a file object'''
    return JKSNDecoder().load(fp, header=header, ordered_dict=ordered_dict)


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
        result = b''.join((bytechr(self.control), self.data, self.buf))
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


_unspecified_value = _unspecified_value()


class JKSNEncoder:
    '''JKSN encoder

    Note: With a certain JKSN encoder, the hashtable is preserved during each dump'''
    def __init__(self):
        self.lastint = None
        self.texthash = [None] * 256
        self.blobhash = [None] * 256

    def dumps(self, obj, header=True, check_circular=True):
        '''Dump an object into a buffer'''
        result = self.dumpobj(obj, check_circular=check_circular).__bytes__()
        if header:
            return b'jk!' + result
        else:
            return result

    def dump(self, obj, fp, header=True, check_circular=True):
        '''Dump an object into a file object'''
        result = self.dumpobj(obj, check_circular=check_circular).__bytes__()
        if header:
            fp.write(b'jk!')
        fp.write(result)

    def dumpobj(self, obj, check_circular=True):
        try:
            self.circular = set() if check_circular else None
            return self._optimize(self._dump_value(obj))
        finally:
            self.circular = None

    def _dump_value(self, obj):
        if obj is None:
            return self._dump_none(obj)
        elif obj is _unspecified_value:
            return self._dump_unspecified(obj)
        elif isinstance(obj, bool):
            return self._dump_bool(obj)
        elif isinstance(obj, int):
            return self._dump_int(obj)
        elif isinstance(obj, float):
            return self._dump_float(obj)
        elif isinstance(obj, str):
            return self._dump_str(obj)
        elif isinstance(obj, bytes):
            return self._dump_bytes(obj)
        elif isinstance(obj, (list, tuple, set)):
            if self.circular is not None:
                if id(obj) in self.circular:
                    raise JKSNEncodeError('circular reference detected during JKSN encoding')
                self.circular.add(id(obj))
            try:
                result = self._dump_list(obj)
            finally:
                if self.circular is not None:
                    self.circular.remove(id(obj))
            return result
        elif isinstance(obj, dict):
            if self.circular is not None:
                if id(obj) in self.circular:
                    raise JKSNEncodeError('circular reference detected during JKSN encoding')
                self.circular.add(id(obj))
            try:
                result = self._dump_dict(obj)
            finally:
                if self.circular is not None:
                    self.circular.remove(id(obj))
            return result
        else:
            raise JKSNEncodeError('cannot encode JKSN from value: %r' % obj)

    def _dump_none(self, obj):
        return JKSNValue(0x01, origin=obj)

    def _dump_unspecified(self, obj):
        return JKSNValue(0xa0, origin=obj)

    def _dump_bool(self, obj):
        return JKSNValue(0x03 if obj else 0x02, origin=obj)

    def _dump_int(self, obj):
        if 0 <= obj <= 0xa:
            return JKSNValue(0x10 | obj, origin=obj)
        elif -0x80 <= obj <= 0x7f:
            return JKSNValue(0x1d, self._encode_int(obj, 1), origin=obj)
        elif -0x8000 <= obj <= 0x7fff:
            return JKSNValue(0x1c, self._encode_int(obj, 2), origin=obj)
        elif -0x80000000 <= obj <= -0x200000 or 0x200000 <= obj <= 0x7fffffff:
            return JKSNValue(0x1b, self._encode_int(obj, 4), origin=obj)
        elif obj >= 0:
            return JKSNValue(0x1f, self._encode_int(obj, 0), origin=obj)
        else:
            return JKSNValue(0x1e, self._encode_int(-obj, 0), origin=obj)

    def _dump_float(self, obj):
        if math.isnan(obj):
            return JKSNValue(0x20, origin=obj)
        elif math.isinf(obj):
            return JKSNValue(0x2f if obj >= 0 else 0x2e, origin=obj)
        else:
            return JKSNValue(0x2c, struct.pack('>d', obj), origin=obj)

    def _dump_str(self, obj):
        obj_utf16 = str.encode(obj, 'utf-16-le')
        obj_utf8 = str.encode(obj, 'utf-8')
        obj_short, control, length = (obj_utf16, 0x30, len(obj_utf16) >> 1) if len(obj_utf16) < len(obj_utf8) else (obj_utf8, 0x40, len(obj_utf8))
        if length <= (0xc if control == 0x40 else 0xb):
            result = JKSNValue(control | length, b'', obj_short, origin=obj)
        elif length <= 0xff:
            result = JKSNValue(control | 0xe, self._encode_int(length, 1), obj_short, origin=obj)
        elif length <= 0xffff:
            result = JKSNValue(control | 0xd, self._encode_int(length, 2), obj_short, origin=obj)
        else:
            result = JKSNValue(control | 0xf, self._encode_int(length, 0), obj_short, origin=obj)
        result.hash = _djb_hash(obj_short)
        return result

    def _dump_bytes(self, obj):
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

    def _dump_list(self, obj):

        def test_swap_availability(obj):
            columns = False
            for row in obj:
                if not isinstance(row, dict):
                    return False
                else:
                    columns = columns or len(row) != 0
            return columns

        def encode_straight_list(obj):
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
                result.children.append(self._dump_value(item))
            assert len(result.children) == length
            return result

        def encode_swapped_list(obj):
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
                result.children.append(self._dump_value(column))
                result.children.append(self._dump_list([row.get(column, _unspecified_value) for row in obj]))
            assert len(result.children) == len(columns) * 2
            return result

        result = encode_straight_list(obj)
        if test_swap_availability(obj):
            result_swapped = encode_swapped_list(obj)
            if result_swapped.__len__(depth=3) < result.__len__(depth=3):
                result = result_swapped
        return result

    def _dump_dict(self, obj):
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
            result.children.append(self._dump_value(key))
            result.children.append(self._dump_value(value))
        assert len(result.children) == length * 2
        return result

    def _optimize(self, obj):
        control = obj.control & 0xf0
        if control == 0x10:
            if self.lastint is not None:
                delta = obj.origin - self.lastint
                if abs(delta) < abs(obj.origin):
                    if 0 <= delta <= 0x5:
                        new_control, new_data = 0xb0 | delta, b''
                    elif -0x5 <= delta <= -0x1:
                        new_control, new_data = 0xb0 | (delta + 11), b''
                    elif -0x80 <= delta <= 0x7f:
                        new_control, new_data = 0xbd, self._encode_int(delta, 1)
                    elif -0x8000 <= delta <= 0x7fff:
                        new_control, new_data = 0xbc, self._encode_int(delta, 2)
                    elif -0x80000000 <= delta <= -0x200000 or 0x200000 <= delta <= 0x7fffffff:
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
                if self.texthash[obj.hash] == obj.buf:
                    obj.control, obj.data, obj.buf = 0x3c, self._encode_int(obj.hash, 1), b''
                else:
                    self.texthash[obj.hash] = obj.buf
        elif control == 0x50:
            if len(obj.buf) > 1:
                if self.blobhash[obj.hash] == obj.buf:
                    obj.control, obj.data, obj.buf = 0x5c, self._encode_int(obj.hash, 1), b''
                else:
                    self.blobhash[obj.hash] = obj.buf
        else:
            for child in obj.children:
                self._optimize(child)
        return obj

    @staticmethod
    def _encode_int(number, size):
        if size == 1:
            return bytechr(number & 0xff)
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
            raise AssertionError


class JKSNDecoder:
    '''JKSN decoder

    Note: With a certain JKSN decoder, the hashtable is preserved during each load'''
    def __init__(self):
        self.lastint = None
        self.texthash = [None] * 256
        self.blobhash = [None] * 256

    def loads(self, s, header=True, ordered_dict=False):
        '''Load an object from a buffer'''
        return self.load(io.BytesIO(s), header=header, ordered_dict=ordered_dict)

    def load(self, fp, header=True, ordered_dict=False):
        '''Load an object from a file object'''
        typetest = fp.read(0)
        if not isinstance(typetest, bytes):
            raise TypeError('%r does not support the buffer interface' % typetest.__name__)
        if header:
            header = fp.read(3)
            if header != b'jk!':
                fp.seek(-len(header), 1)
        self._ordered_dict = collections.OrderedDict if ordered_dict else dict
        return self._load_value(_file_check_eof(fp))

    def _load_value(self, fp):
        while True:
            control = ord(fp.read(1))
            ctrlhi = control & 0xf0
            # Special values
            if ctrlhi == 0x00:
                if control in (0x00, 0x01):
                    return None
                elif control == 0x02:
                    return False
                elif control == 0x03:
                    return True
                elif control == 0x0f:
                    s = self._load_value(fp)
                    if not isinstance(s, str):
                        raise JKSNDecodeError('JKSN value 0x0f requires a string but found: %r' % s)
                    return json.loads(s)
            # Integers
            elif ctrlhi == 0x10:
                if control <= 0x1a:
                    self.lastint = control & 0xf
                elif control == 0x1b:
                    self.lastint = self._unsigned_to_signed(self._decode_int(fp, 4), 32)
                elif control == 0x1c:
                    self.lastint = self._unsigned_to_signed(self._decode_int(fp, 2), 16)
                elif control == 0x1d:
                    self.lastint = self._unsigned_to_signed(self._decode_int(fp, 1), 8)
                elif control == 0x1e:
                    self.lastint = -self._decode_int(fp, 0)
                elif control == 0x1f:
                    self.lastint = self._decode_int(fp, 0)
                return self.lastint
            # Float point numbers
            elif ctrlhi == 0x20:
                if control == 0x20:
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
            # UTF-16 strings
            elif ctrlhi == 0x30:
                if control <= 0x3b:
                    return self._load_str(fp, (control & 0xf) << 1, 'utf-16-le')
                elif control == 0x3c:
                    hashvalue = ord(fp.read(1))
                    if self.texthash[hashvalue] is not None:
                        return self.texthash[hashvalue]
                    else:
                        raise JKSNDecodeError('JKSN stream requires a non-existing hash: 0x%02x' % hashvalue)
                elif control == 0x3d:
                    return self._load_str(fp, self._decode_int(fp, 2) << 1, 'utf-16-le')
                elif control == 0x3e:
                    return self._load_str(fp, self._decode_int(fp, 1) << 1, 'utf-16-le')
                elif control == 0x3f:
                    return self._load_str(fp, self._decode_int(fp, 0) << 1, 'utf-16-le')
            # UTF-8 strings
            elif ctrlhi == 0x40:
                if control <= 0x4c:
                    return self._load_str(fp, control & 0xf, 'utf-8')
                elif control == 0x4d:
                    return self._load_str(fp, self._decode_int(fp, 2), 'utf-8')
                elif control == 0x4e:
                    return self._load_str(fp, self._decode_int(fp, 1), 'utf-8')
                elif control == 0x4f:
                    return self._load_str(fp, self._decode_int(fp, 0), 'utf-8')
            # Blob strings
            elif ctrlhi == 0x50:
                if control <= 0x5b:
                    return self._load_str(fp, control & 0xf)
                elif control == 0x5c:
                    hashvalue = ord(fp.read(1))
                    if self.blobhash[hashvalue] is not None:
                        return self.blobhash[hashvalue]
                    else:
                        raise JKSNDecodeError('JKSN stream requires a non-existing hash: 0x%02x' % hashvalue)
                elif control == 0x5d:
                    return self._load_str(fp, self._decode_int(fp, 2))
                elif control == 0x5e:
                    return self._load_str(fp, self._decode_int(fp, 1))
                elif control == 0x5f:
                    return self._load_str(fp, self._decode_int(fp, 0))
            # Hashtable refreshers
            elif ctrlhi == 0x70:
                if control == 0x70:
                    self.texthash = [None] * 256
                    self.blobhash = [None] * 256
                elif control <= 0x7c:
                    for i in range(control & 0xf):
                        self._load_value(fp)
                elif control == 0x7d:
                    for i in range(self._decode_int(fp, 2)):
                        self._load_value(fp)
                elif control == 0x7e:
                    for i in range(self._decode_int(fp, 1)):
                        self._load_value(fp)
                elif control == 0x7f:
                    for i in range(self._decode_int(fp, 0)):
                        self._load_value(fp)
                continue
            # Arrays
            elif ctrlhi == 0x80:
                if control <= 0x8c:
                    return [self._load_value(fp) for i in range(control & 0xf)]
                elif control == 0x8d:
                    return [self._load_value(fp) for i in range(self._decode_int(fp, 2))]
                elif control == 0x8e:
                    return [self._load_value(fp) for i in range(self._decode_int(fp, 1))]
                elif control == 0x8f:
                    return [self._load_value(fp) for i in range(self._decode_int(fp, 0))]
            # Objects
            elif ctrlhi == 0x90:
                if control <= 0x9c:
                    return self._ordered_dict((self._load_value(fp), self._load_value(fp)) for i in range(control & 0xf))
                elif control == 0x9d:
                    return self._ordered_dict((self._load_value(fp), self._load_value(fp)) for i in range(self._decode_int(fp, 2)))
                elif control == 0x9e:
                    return self._ordered_dict((self._load_value(fp), self._load_value(fp)) for i in range(self._decode_int(fp, 1)))
                elif control == 0x9f:
                    return self._ordered_dict((self._load_value(fp), self._load_value(fp)) for i in range(self._decode_int(fp, 0)))
            # Row-col swapped arrays
            elif ctrlhi == 0xa0:
                if control == 0xa0:
                    return _unspecified_value
                elif control <= 0xac:
                    return self._load_swapped_list(fp, control & 0xf)
                elif control == 0xad:
                    return self._load_swapped_list(fp, self._decode_int(fp, 2))
                elif control == 0xae:
                    return self._load_swapped_list(fp, self._decode_int(fp, 1))
                elif control == 0xaf:
                    return self._load_swapped_list(fp, self._decode_int(fp, 0))
            # Delta encoded integers
            elif ctrlhi == 0xb0:
                if control <= 0xb5:
                    delta = control & 0xf
                elif control <= 0xba:
                    delta = (control & 0xf) - 11
                elif control == 0xbb:
                    delta = self._decode_int(fp, 4)
                elif control == 0xbc:
                    delta = self._decode_int(fp, 2)
                elif control == 0xbd:
                    delta = self._decode_int(fp, 1)
                elif control == 0xbe:
                    delta = -self._decode_int(fp, 0)
                elif control == 0xbf:
                    delta = self._decode_int(fp, 0)
                if self.lastint is not None:
                    self.lastint += delta
                    return self.lastint
                else:
                    raise JKSNDecodeError('JKSN stream contains an invalid delta encoded integer')
            elif ctrlhi == 0xf0:
                # Checksums
                if control <= 0xf4:
                    if control == 0xf0:
                        checksum = fp.read(4)
                        fp = _hashed_file(fp, _crc32_hasher)
                    elif control == 0xf1:
                        checksum = fp.read(16)
                        fp = _hashed_file(fp, hashlib.md5)
                    elif control == 0xf2:
                        checksum = fp.read(20)
                        fp = _hashed_file(fp, hashlib.sha1)
                    elif control == 0xf3:
                        checksum = fp.read(32)
                        fp = _hashed_file(fp, hashlib.sha256)
                    elif control == 0xf4:
                        checksum = fp.read(64)
                        fp = _hashed_file(fp, hashlib.sha512)
                    result = self._load_value(fp)
                    if fp.digest() == checksum:
                        return result
                    else:
                        return JKSNDecodeError('checksum mismatch')
                elif control in range(0xf8, 0xfd):
                    if control == 0xf8:
                        hashed_fp = _hashed_file(fp, _crc32_hasher)
                        result = self._load_value(hashed_fp)
                        checksum = fp.read(4)
                    elif control == 0xf9:
                        hashed_fp = _hashed_file(fp, hashlib.md5)
                        result = self._load_value(hashed_fp)
                        checksum = fp.read(16)
                    elif control == 0xfa:
                        hashed_fp = _hashed_file(fp, hashlib.sha1)
                        result = self._load_value(hashed_fp)
                        checksum = fp.read(20)
                    elif control == 0xfb:
                        hashed_fp = _hashed_file(fp, hashlib.sha256)
                        result = self._load_value(hashed_fp)
                        checksum = fp.read(32)
                    elif control == 0xfc:
                        hashed_fp = _hashed_file(fp, hashlib.sha512)
                        result = self._load_value(hashed_fp)
                        checksum = fp.read(64)
                    if hashed_fp.digest() == checksum:
                        return result
                    else:
                        return JKSNDecodeError('checksum mismatch')
                # Ignore pragmas
                elif control == 0xff:
                    self._load_value(fp)
                    continue
            raise JKSNDecodeError('cannot decode JKSN from byte 0x%02x' % control)

    def _load_str(self, fp, length, encoding=None):
        buf = fp.read(length)
        if encoding is not None:
            result = buf.decode(encoding)
            self.texthash[_djb_hash(buf)] = result
        else:
            result = buf
            self.blobhash[_djb_hash(buf)] = result
        return result

    def _load_swapped_list(self, fp, column_length):
        result = []
        for i in range(column_length):
            column_name = self._load_value(fp)
            column_values = self._load_value(fp)
            if not isinstance(column_values, list):
                raise JKSNDecodeError('JKSN row-col swapped array requires a array but found: %r' % column_values)
            for idx, value in enumerate(column_values):
                if idx == len(result):
                    result.append([])
                if value is not _unspecified_value:
                    result[idx].append((column_name, value))
        return list(map(self._ordered_dict, result))

    @staticmethod
    def _decode_int(fp, size):
        if size == 1:
            return ord(fp.read(size))
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


class _crc32_hasher:
    def __init__(self, data=b''):
        self.value = zlib.crc32(data)

    def update(self, data=b''):
        self.value = zlib.crc32(data, self.value)

    def digest(self):
        return struct.pack('>L', self.value & 0xffffffff)


class _file_check_eof:
    def __init__(self, fp):
        self.fp = fp

    def read(self, size=None):
        result = self.fp.read(size)
        if size is not None and len(result) < size:
            raise EOFError
        return result


class _hashed_file:
    def __init__(self, fp, hasher):
        self.fp = fp
        self.hasher = hasher()

    def read(self, size=None):
        result = self.fp.read(size)
        if result:
            self.hasher.update(result)
        return result

    def digest(self):
        return self.hasher.digest()


if __name__ == '__main__':
    if '--help' in sys.argv:
        sys.stderr.write('Usage: %s [-d] <input >output\n\nOptions:\n\t-d\tDecode JKSN instead of encoding\n\n' % sys.argv[0])
    elif '-d' in sys.argv:
        json.dump(loads(sys.stdin.buffer.read(), ordered_dict=True), sys.stdout, indent=2)
        sys.stdout.write('\n')
    else:
        dump(json.load(sys.stdin), sys.stdout.buffer)
