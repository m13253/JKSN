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
require 'jksn/types'

module JKSN

  # Exception class raised during JKSN encoding and decoding
  class JKSNError < RuntimeError
  end

  # Exception class raised during JKSN encoding
  class EncodeError < JKSNError
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
  end

  class JKSNProxy
    attr_accessor :origin
    attr_accessor :control
    attr_accessor :data
    attr_accessor :buf
    attr_accessor :hash
    attr_accessor :children
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
      result = [@control.chr, @data, @buf].join('').b
      @children.each {|i| result << i.to_s.b } if recursive
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
    @@jksn_proxy = JKSNProxy.new(nil, 0xa0)
    @@jksn_proxy.freeze
    def self.__jksn_dump(*args)
      @@jksn_proxy
    end
  end

  class JKSNEncoder
    def initialize
      @lastint = nil
      @texthash = [nil] * 256
      @blobhash = [nil] * 256
    end

    def dumps(obj, header=true)
      result = dump_to_proxy(obj)
      return header ? ('jk!'.b + result.to_s) : result.to_s
    end

    def dump(fd, obj, header=true)
      fd.write(dumps(obj, header))
    end

    def dump_to_proxy(obj)
      optimize(obj.__jksn_dump)
    end

    def optimize(proxyobj)
      # TODO
      control = proxyobj.control & 0xF0
      case control
      when 0x10
        if !@lastint.nil?
          delta = proxyobj.origin - @lastint
          if delta.abs < proxyobj.origin.abs
            if (0..0x5).cover? delta
              new_control, new_data = 0xb0 | delta, ''.b
            elsif (-0x05..-0x01).cover? delta
              new_control, new_data = 0xb0 | (delta + 11), ''.b
            elsif (-0x80..0x7F).cover? delta
              new_control, new_data = 0xbd, delta.__jksn_encode(1)
            elsif (-0x8000..0x7FFF).cover? delta
              new_control, new_data = 0xbc, delta.__jksn_encode(2)
            elsif (-0x80000000..-0x200000).cover?(delta) || (0x200000..0x7FFFFFFF).cover?(delta)
              new_control, new_data = 0xbb, delta.__jksn_encode(4)
            elsif delta >= 0
              new_control, new_data = 0xbf, delta.__jksn_encode(0)
            else
              new_control, new_data = 0xbe, (-delta).__jksn_encode(0)
            end
            if new_data.length < proxyobj.data.length
              proxyobj.control, proxyobj.data = new_control, new_data
            end
            @lastint = proxyobj.origin
          end
        end
      when 0x30, 0x40
        if proxyobj.buf.length > 1
          if @texthash[proxyobj.hash] == proxyobj.buf
            proxyobj.control, proxyobj.data, proxyobj.buf = 0x3c, proxyobj.hash.__jksn_encode(1), ''.b
          else
            @texthash[proxyobj.hash] = proxyobj.buf
          end
        end
      when 0x50
        if proxyobj.buf.length > 1
          if @blobhash[proxyobj.hash] == proxyobj.buf
            proxyobj.control, proxyobj.data, proxyobj.buf = 0x5c, proxyobj.hash.__jksn_encode(1), ''.b
          else
            @blobhash[proxyobj.hash] = proxyobj.buf
          end
        end
      else
        proxyobj.children.each { |child| optimize(child) }
      end

      return proxyobj
    end

  end

end

