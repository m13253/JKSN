# Copyright (c) 2014 StarBrilliant <m13253@hotmail.com>
#                and dantmnf       <dantmnf2@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that the above copyright notice and this paragraph are
# duplicated in all such forms and that any documentation,
# advertising materials, and other materials related to such
# distribution and use acknowledge that the software was originally 
# developed by StarBrilliant.
# The name of StarBrilliant may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#require 'jksn'

class Integer
  def __jksn_dump
    if (0x00..0x0A).cover? self
      #return (self + 0x10).chr
      return JKSN::JKSNProxy.new(self, self | 0x10)
    elsif (-128..127).cover? self
      #return [0x1d, self].pack('cc')
      return JKSN::JKSNProxy.new(self, 0x1d, [self].pack('c')[0])
    elsif (-32767..32768).cover? self
      #return [0x1c, self].pack('cs>')
      return JKSN::JKSNProxy.new(self, 0x1c, [self].pack('s>')[0])
    elsif (0x20000000..0x7FFFFFFF).cover? self
      return __jksn_dump_bignum
    elsif (-2147483648...0x20000000).cover? self
      #return [0x1b, self].pack('cl>')
      return JKSN::JKSNProxy.new(self, 0x1b, [self].pack('l>')[0])
    else
      return __jksn_dump_bignum
    end
  end

  def __jksn_dump_bignum
    raise unless self != 0
    minus = (self < 0)
    num = self.abs
    atoms = [num & 0x007F]
    num >>= 7
    while num != 0
      atoms.unshift((num & 0x007F) | 0x0080)
      num >>= 7
    end
    return JKSN::JKSNProxy.new(self, (minus ? 0x1e : 0x1f), atoms.pack('C*'))
  end
end