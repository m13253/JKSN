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

class Object
  def __jksn_dump(*args)
    self.inspect.__jksn_dump(*args)
  end

  protected
  def __jksn_check_circular_helper(obj, circular_idlist)
    if obj.respond_to?(:to_a) or obj.respond_to?(:to_h)
      if circular_idlist.include? obj.object_id
        raise JKSN::EncodeError.new('circular reference found')
      else
        circular_idlist << obj.object_id
      end
    end
  end
end


class Integer
  def __jksn_dump(*args)
    if (0x00..0x0A).cover? self
      #return (self + 0x10).chr
      return JKSN::JKSNProxy.new(self, self | 0x10)
    elsif (-128..127).cover? self
      #return [0x1d, self].pack('cc')
      return JKSN::JKSNProxy.new(self, 0x1d, __jksn_encode(1))
    elsif (-32767..32768).cover? self
      #return [0x1c, self].pack('cs>')
      return JKSN::JKSNProxy.new(self, 0x1c, __jksn_encode(2))
    elsif (0x20000000..0x7FFFFFFF).cover?(self) or (-0x80000000..-0x20000000).cover?(self)
      return __jksn_dump_bignum
    elsif (-2147483648...0x20000000).cover? self
      #return [0x1b, self].pack('cl>')
      return JKSN::JKSNProxy.new(self, 0x1b, __jksn_encode(4))
    else
      return __jksn_dump_bignum
    end
  end

  def __jksn_encode(length)
    case length
    when 0
      return __jksn_encode_bignum
    when 1
      return (self & 0x00FF).chr
    when 2
      return [self & 0x00FFFF].pack('S>')[0]
    when 4
      return [self & 0x00FFFFFFFF].pack('L>')[0]
    else
      raise ArgumentError.new
    end
  end

  private
  def __jksn_dump_bignum
    raise unless self != 0
    minus = (self < 0)
    return JKSN::JKSNProxy.new(self, (minus ? 0x1e : 0x1f), self.abs.__jksn_encode_bignum)
  end

  def __jksn_encode_bignum(num)
    num = num.clone
    atoms = [num & 0x007F]
    num >>= 7
    while num != 0
      atoms.unshift((num & 0x007F) | 0x0080)
      num >>= 7
    end
    atoms.pack('C*')
  end

end

class TrueClass
  @@jksn_proxy = JKSN::JKSNProxy.new(self, 0x03)
  @@jksn_proxy.freeze
  def __jksn_dump(*args)
    @@jksn_proxy
  end
end

class FalseClass
  @@jksn_proxy = JKSN::JKSNProxy.new(self, 0x02)
  @@jksn_proxy.freeze
  def __jksn_dump(*args)
    @@jksn_proxy
  end
end

class NilClass
  @@jksn_proxy = JKSN::JKSNProxy.new(self, 0x01)
  @@jksn_proxy.freeze
  def __jksn_dump(*args)
    @@jksn_proxy
  end
end

class Float
  def __jksn_dump(*args)
    return JKSN::JKSNProxy.new(self, 0x20) if self.nan?
    case self.infinite?
    when 1
      return JKSN::JKSNProxy.new(self, 0x2f)
    when -1
      return JKSN::JKSNProxy.new(self, 0x2e)
    else
      return JKSN::JKSNProxy.new(self, 0x2c, [self].pack('G')[0])
    end
  end
end

class String
  def __jksn_dump(*args)
    if self.encoding == Encoding::ASCII_8BIT
      return __jksn_dump_blob
    else
      return __jksn_dump_unicode
    end
  end

  def __jksn_djbhash(iv=0)
    result = iv
    self.each_byte do |i|
      result += (result << 5) + i
      result &= 0xFF
    end
    return result
  end

  #private

  def __jksn_dump_blob
    if length <= 0xB
      result = JKSN::JKSNProxy.new(self, 0x50 | length, '', self)
    elsif length <= 0xFF
      result = JKSN::JKSNProxy.new(self, 0x5e, length.__jksn_encode(1), self)
    elsif length <= 0xFFFF
      result = JKSN::JKSNProxy.new(self, 0x5d, length.__jksn_encode(2), self)
    else
      result = JKSN::JKSNProxy.new(self, 0x5f, length.__jksn_encode(0), self)
    end
    result.hash = __jksn_djbhash
    return result
  end

  def __jksn_dump_unicode
    u16str = self.encode(Encoding::UTF_16LE)
    u8str  = self.encode(Encoding::UTF_8)
    short, control, enclength = (u16str.length < u8str.length) ? [u16str, 0x30, u16str.length << 1] : [u8str, 0x40, u8str.length]
    if enclength <= (control == 0x40 ? 0xc : 0xb)
      result = JKSN::JKSNProxy.new(self, control | length, '', short)
    elsif enclength <= 0xFF
      result = JKSN::JKSNProxy.new(self, control | 0x0e, length.__jksn_encode(1), short)
    elsif enclength <= 0xFFFF
      result = JKSN::JKSNProxy.new(self, control | 0x0d, length.__jksn_encode(2), short)
    else
      result = JKSN::JKSNProxy.new(self, control | 0x0f, length.__jksn_encode(0), short)
    end
    result.hash = short.__jksn_djbhash
    return result
  end
end

class Hash
  def __jksn_dump(circular_idlist=[])
    if length <= 0xc
      result = JKSN::JKSNProxy.new(self, 0x90 | length)
    elsif length <= 0xff
      result = JKSN::JKSNProxy.new(self, 0x9e, length.__jksn_encode(1))
    elsif length <= 0xffff
      result = JKSN::JKSNProxy.new(self, 0x9d, length.__jksn_encode(2))
    else
      result = JKSN::JKSNProxy.new(self, 0x9f, length.__jksn_encode(0))
    end
    self.each do |key, value|
      __jksn_check_circular_helper(key, circular_idlist)
      __jksn_check_circular_helper(value, circular_idlist)

      result.children << key.__jksn_dump(circular_idlist)
      result.children << value.__jksn_dump(circular_idlist)

    end
    raise unless result.children.length == length * 2
    return result
  end
end


require 'set'
class Array
  def __jksn_dump(circular_idlist=[])
    result = __jksn_encode_straight(circular_idlist)
    if __jksn_can_swap?
      result_swapped = __jksn_encode_swapped(circular_idlist)
      result = result_swapped if result_swapped.length(3) < result.length(3)
    end
    return result
  end

  def __jksn_can_swap?
    columns = false
    self.each do |row|
      return false unless row.is_a? Hash
      columns = columns || (row.length != 0)
    end
    return columns
  end

  def __jksn_encode_straight(circular_idlist=[])
    if length <= 0xc
      result = JKSN::JKSNProxy.new(self, 0x80 | length)
    elsif length <= 0xff
      result = JKSN::JKSNProxy.new(self, 0x8e, length.__jksn_encode(1))
    elsif length <= 0xffff
      result = JKSN::JKSNProxy.new(self, 0x8d, length.__jksn_encode(2))
    else
      result = JKSN::JKSNProxy.new(self, 0x8f, length.__jksn_encode(0))
    end
    self.each do |i|
      __jksn_check_circular_helper(i, circular_idlist)
      result.children << i.__jksn_dump(circular_idlist)
    end
    raise unless result.children.length == length
    return result
  end

  def __jksn_encode_swapped(circular_idlist=[])
    # row is Hash
    columns = Set.new(self.map(&:keys).flatten)
    if length <= 0xc
      result = JKSN::JKSNProxy.new(self, 0xa0 | length)
    elsif length <= 0xff
      result = JKSN::JKSNProxy.new(self, 0xae, length.__jksn_encode(1))
    elsif length <= 0xffff
      result = JKSN::JKSNProxy.new(self, 0xad, length.__jksn_encode(2))
    else
      result = JKSN::JKSNProxy.new(self, 0xa8f, length.__jksn_encode(0))
    end
    columns.each do |column|
      __jksn_check_circular_helper(column, circular_idlist)
      result.children << column.__jksn_dump(circular_idlist)
      result.children << self.map{|row| row.fetch(column, JKSN::UnspecifiedValue)}.__jksn_dump(circular_idlist)
    end
    raise unless result.children.length == length * 2
    return result
  end
end
