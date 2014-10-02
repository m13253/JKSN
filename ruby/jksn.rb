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


module JKSN

  # Exception class raised during JKSN encoding and decoding
  class Error
  end

  # Exception class raised during JKSN encoding
  class EncodeError < Error
  end

  # Exception class raised during JKSN decoding
  class DecodeError < Error
  end

  # Exception class raised during checksum verification when decoding
  class ChecksumError < DecodeError
  end

  class << self
    # Dump an object into a buffer
    def dumps(*args)
      JKSNEncoder.new.dumps(*args)
    end

    # Dump an object into a file object
    def dump(*args)
      JKSNEncoder.new.dump(*args)
    end

    # load an object from a buffer
    def loads(*args)
      JKSNDecoder.new.loads(*args)
    end

    # Dump an object into a file object
    def load(*args)
      JKSNDecoder.new.load(*args)
    end
  end

  class JKSNProxy
    def initialize(origin, control, data='', buf='')
      raise unless control.is_a?(Fixnum) && (0..255).cover?(control)
      raise unless data.is_a? String
      raise unless buf.is_a? String
      @origin = origin
      @control = control
      @data = data
      @buf = buf
      @children = []
      @hash = nil
    end

    def output(fp, recursive=true)
      fp.write @control.chr
      fp.write @data
      fp.write @buf
      @children.each {|i| i.output(fp) } if recursive
    end

    def to_s(recursive=true) # Python __bytes__
      result = [@control.chr, @data, @buf].join ''
      @children.each {|i| result << i.to_s } if recursive
      return result
    end

    def length(depth=0)
      result = @data.length + @buf.length + 1
      @children.each {|i| result += i.length(depth - 1) } if depth != 1
      return result
    end
    alias :size :length
  end

  class UnspecifiedValue
  end

end