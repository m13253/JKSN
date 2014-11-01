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

require 'stringio'
require 'oj' rescue require 'json'

module JKSN

  # Exception class raised during JKSN decoding
  class DecodeError < JKSNError
  end

  # Exception class raised during checksum verification when decoding
  class ChecksumError < DecodeError
  end

  class << self
    # load an object from a buffer
    def loads(*args)
      JKSNDecoder.new.loads(*args)
    end

    # Dump an object into a file object
    def load(*args)
      JKSNDecoder.new.load(*args)
    end
  end

  class JKSNDecoder
    def initialize
      @lastint = nil
      @texthash = [nil] * 256
      @blobhash = [nil] * 256
    end

    def loads(s, header=true)
      load(StringIO.new(s.b), header)
    end

    def load(io, header=true)
      if header
        if (header_from_io = io.read(3)) != '!jk'.b
          io.seek(-header_from_io.length, :CUR)
        end
      end
      return load_value(IODieWhenEOFRead.from(io))
    end

    protected
    def load_value(io)
      loop do
        control = io.read(1).ord
        ctrlhi = control & 0xF0
        case ctrlhi
        when 0x00 # Special values
          case control
          when 0x00, 0x01
            return nil
          when 0x02
            return false
          when 0x03
            return true
          when 0x0F
            s = load_value(io)
            raise DecoderError.new unless s.is_a? String
            return JSON.parse s
          end
        when 0x10 # Integers
          @lastint = case control
            when 0x10..0x1A
              control & 0x0F
            when 0x1B
              unsigned_to_signed(decode_int(io, 4), 32)
            when 0x1C
              unsigned_to_signed(decode_int(io, 2), 16)
            when 0x1D
              unsigned_to_signed(decode_int(io, 1), 8)
            when 0x1E
              -decode_int(io, 0)
            when 0x1F
              decode_int(io, 0)
            end
          return @lastint

        when 0x20 # Float point numbers
          case control
          when 0x20
            return Float::NAN
          when 0x2B
            raise NotImplementedError.new('this JKSN decoder does not support long double numbers')
          when 0x2C
            return io.read(8).unpack('G').first
          when 0x2D
            return io.read(4).unpack('g').first
          when 0x2E
            return -Float::INFINITY
          when 0x2F
            return Float::INFINITY
          end
        when 0x30 # UTF-16 strings
          raise NotImplementedError.new
        when 0x40 # UTF-8 strings
          raise NotImplementedError.new
        when 0x50 # Blob strings
          raise NotImplementedError.new
        when 0x70 # Hashtable refreshers
          raise NotImplementedError.new
        when 0x80 # Arrays
          raise NotImplementedError.new
        when 0x90 # Objects
          raise NotImplementedError.new
        when 0xA0 # Row-col swapped arrays
          raise NotImplementedError.new
        when 0xB0 # Delta encoded integers
          raise NotImplementedError.new
        when 0xF0 # Checksums
          raise NotImplementedError.new
        else
          raise DecodeError.new('cannot decode JKSN from byte 0x%02x' % control)
        end
      end

    end

    def decode_int(io, length)
      case length
      when 1
        return io.read(1).ord
      when 2
        return io.read(2).unpack('S>').first
      when 4
        return io.read(4).unpack('L>').first
      when 0
        result = 0
        thisbyte = -1
        while thisbyte & 0x80 != 0
          thisbyte = io.read(1).ord
          result = (result << 7) | (thisbyte & 0x7F)
        end
        return result
      else
        raise DecoderError.new
      end
    end

    def unsigned_to_signed(x, bits)
      x - ((x >> (bits - 1)) << bits)
    end

  end

  class IODieWhenEOFRead
    def self.from(io)
      obj = self.new
      io.instance_methods.each do |name|
        obj.define_singleton_method(name) { |*args, &block| io.send(name, *args, &block) } 
      end
      obj.define_singleton_method :read do |length=nil, strbuf=nil|
        result = io.read(length, strbuf)
        if length != nil
          raise EOFError.new if result.length < length
        end
        return result
      end
    end
  end
end