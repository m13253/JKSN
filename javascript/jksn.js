(function(window) {

function JKSNEncoder() {
    var lastint = null;
    var texthash = new Array(256);
    var blobhash = new Array(256);
    return {
    };
}
function JKSNDecoder() {
    var lastint = null;
    var texthash = new Array(256);
    var blobhash = new Array(256);
    var offset = 0;
    function loadValue(buf) {
        for(;;) {
            var control = buf.getUint8(offset++);
            var ctrlhi = control & 0xf0;
            switch(ctrlhi) {
            case 0x00:
                switch(control) {
                case 0x00:
                    return undefined;
                case 0x01:
                    return null;
                case 0x02:
                    return false;
                case 0x03:
                    return true;
                case 0x0f:
                    var s = loadValue(fp);
                    if(typeof s !== "string")
                        throw "JKSNDecodeError: JKSN value 0x0f requires a string but found "+s;
                    return JSON.parse(s);
                }
                break;
            case 0x10:
                switch(control) {
                case 0x1b:
                    lastint = buf.getInt32(offset, false);
                    offset += 4;
                    break;
                case 0x1c:
                    lastint = buf.getInt16(offset, false);
                    offset += 2;
                    break;
                case 0x1d:
                    lastint = buf.getInt8(offset++);
                    break;
                case 0x1e:
                    lastint = -decodeInt(buf);
                    break;
                case 0x1f:
                    lastint = decodeInt(buf);
                    break;
                default:
                    lastint = control & 0xf;
                }
                return lastint;
            case 0x20:
                switch(control) {
                case 0x20:
                    return NaN;
                case 0x2b:
                    throw "JKSNDecodeError: This JKSN decoder does not support long double numbers.";
                case 0x2c:
                    var result = buf.getFloat64(offset, false);
                    offset += 8;
                    return result;
                case 0x2d:
                    var result = buf.getFloat32(offset, false);
                    offset += 4;
                    return result;
                case 0x2e:
                    return -Infinity;
                case 0x2f:
                    return Infinity;
                }
                break;
            case 0x30:
                var strlen;
                switch(control) {
                case 0x3c:
                    var hashvalue = buf.getUint8(offset++);
                    if(texthash[hashvalue] !== undefined)
                        return texthash[hashvalue];
                    else
                        throw "JKSNDecodeError: JKSN stream requires a non-existing hash: "+hashvalue;
                case 0x3d:
                    strlen = buf.getUint16(offset, false);
                    offset += 2;
                    break;
                case 0x3e:
                    strlen = buf.getUint8(offset++);
                    break;
                case 0x3f:
                    strlen = decodeInt(buf);
                    break;
                default:
                    strlen = control & 0xf;
                }
                strlen *= 2;
                var strbuf = new Uint8Array(buf.buffer, offset, strlen);
                var result = String.fromCharCode.apply(null, LittleEndianUint16FromUint8Array(strbuf));
                texthash[DJBHash(strbuf)] = result;
                offset += strlen;
                return result;
            case 0x40:
                var strlen;
                switch(control) {
                case 0x4d:
                    strlen = buf.getUint16(offset, false);
                    offset += 2;
                    break;
                case 0x4e:
                    strlen = buf.getUint8(offset++);
                    break;
                case 0x4f:
                    strlen = decodeInt(buf);
                    break;
                default:
                    strlen = control & 0xf;
                }
                var strbuf = new Uint8Array(buf.buffer, offset, strlen);
                var result = decodeURIComponent(escape(String.fromCharCode.apply(null, strbuf)));
                texthash[DJBHash(strbuf)] = result;
                offset += strlen;
                return result;
            case 0x50:
                var strlen;
                switch(control) {
                case 0x5d:
                    strlen = buf.getUint16(offset, false);
                    offset += 2;
                    break;
                case 0x5e:
                    strlen = buf.getUint8(offset++);
                    break;
                case 0x5f:
                    strlen = decodeInt(buf);
                    break;
                default:
                    strlen = control & 0xf;
                }
                var strbuf = new Uint8Array(new Uint8Array(buf.buffer, offset, strlen));
                var result = strbuf.buffer;
                blobhash[DJBHash(strbuf)] = result;
                offset += strlen;
                return result;
            case 0x70:
                var objlen;
                switch(control) {
                case 0x70:
                    self.texthash = new Array(256);
                    self.blobhash = new Array(256);
                    continue;
                case 0x7d:
                    objlen = buf.getUint16(offset, false);
                    offset += 2;
                    break;
                case 0x7e:
                    objlen = buf.getUint8(offset++);
                    break;
                case 0x7f:
                    objlen = decodeInt(buf);
                    break;
                default:
                    objlen = control & 0xf;
                }
                for(; objlen > 0; objlen--)
                    loadValue(buf);
                continue;
            case 0x80:
                var objlen;
                switch(control) {
                case 0x8d:
                    objlen = buf.getUint16(offset, false);
                    offset += 2;
                    break;
                case 0x8e:
                    objlen = buf.getUint8(offset++);
                    break;
                case 0x8f:
                    objlen = decodeInt(buf);
                    break;
                default:
                    objlen = control & 0xf;
                }
                var result = new Array(objlen);
                for(var i = 0; i < objlen; i++)
                    result[i] = loadValue(buf);
                return result;
            case 0x90:
                var objlen;
                switch(control) {
                case 0x9d:
                    objlen = buf.getUint16(offset, false);
                    offset += 2;
                    break;
                case 0x9e:
                    objlen = buf.getUint8(offset++);
                    break;
                case 0x9f:
                    objlen = decodeInt(buf);
                    break;
                default:
                    objlen = control & 0xf;
                }
                var result = {};
                for(; objlen > 0; objlen--) {
                    var key = loadValue(buf);
                    result[key] = loadValue(buf);
                }
                return result;
            case 0xa0:
                var collen;
                switch(control) {
                case 0xa0:
                    return undefined;
                case 0xad:
                    collen = buf.getUint16(offset, false);
                    offset += 2;
                    break;
                case 0xae:
                    collen = buf.getUint8(offset++);
                    break;
                case 0xaf:
                    collen = decodeInt(buf);
                    break;
                default:
                    collen = control & 0xf;
                }
                var result = [];
                for(var i = 0; i < collen; i++) {
                    var column_name = loadValue(buf);
                    var column_values = loadValue(buf);
                    if(column_values.slice === undefined)
                        throw "JKSNDecodeError: JKSN row-col swapped array requires an array but found "+column_values;
                    for(var row = 0; row < column_values.length; row++) {
                        if(row == result.length)
                            result.push({});
                        if(column_values[row] !== undefined)
                            result[row][column_name] = column_values[row];
                    }
                }
                return result;
            case 0xb0:
                var delta;
                switch(control) {
                case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5:
                    delta = control & 0xf;
                    break;
                case 0xb6: case 0xb7: case 0xb8: case 0xb9: case 0xba:
                    delta = (control & 0xf)-11;
                    break;
                case 0xbb:
                    delta = buf.getInt32(offset, false);
                    offset += 4;
                    break;
                case 0xbc:
                    delta = buf.getInt16(offset, false);
                    offset += 2;
                    break;
                case 0xbd:
                    delta = buf.getInt8(offset++);
                    break;
                case 0xbe:
                    delta = -decodeInt(buf);
                    break;
                case 0xbf:
                    delta = decodeInt(buf);
                    break;
                }
                if(lastint !== null)
                    return (lastint += delta);
                else
                    throw "JKSNDecodeError: JKSN stream contains an invalid delta encoded integer"
            case 0xf0:
                if(control <= 0xf4 || (control >= 0xf8 && control <= 0xfc)) {
                    console.warn("JKSNDecodeWarning: checksum is not supported")
                    switch(control) {
                    case 0xf0:
                        offset += 4;
                        continue;
                    case 0xf1:
                        offset += 16;
                        continue;
                    case 0xf2:
                        offset += 20;
                        continue;
                    case 0xf3:
                        offset += 32;
                        continue;
                    case 0xf4:
                        offset += 64;
                        continue;
                    }
                    continue;
                } else if(control == 0xff) {
                    loadValue(buf);
                    continue;
                }
                break;
            }
            throw "JKSNDecodeError: cannot decode JKSN from byte "+control;
        }
    }
    function decodeInt(buf) {
        var result = 0;
        var thisbyte;
        do {
            thisbyte = buf.getUint8(offset++);
            result = (result*128) + (thisbyte & 0x7f);
        } while(thisbyte & 0x80);
        return result;
    }
    function LittleEndianUint16FromUint8Array(arr) {
        var result = new Uint16Array(arr.length/2);
        for(var i = 0, j = 1, k = 0; j < arr.length; i += 2, j += 2, k++)
            result[k] = arr[i] | (arr[j] << 8);
        return result;
    }
    return {
        "parseFromArrayBuffer": function (buf) {
            var headerbuf = new Uint8Array(buf, 0, 3);
            if(headerbuf[0] == 106 && headerbuf[1] == 107 && headerbuf[2] == 33)
                offset = 3;
            return loadValue(new DataView(buf));
        },
        "parseFromString": function (str) {
            var buf = new ArrayBuffer(str.length);
            var bufview = new Uint8Array(buf);
            for(var i = 0; i < str.length; i++)
                bufview[i] = str.charCodeAt(i);
            return this.parseFromArrayBuffer(buf);
        }
    };
}
function DJBHash(arr) {
    var result = 0;
    for(var i = 0; i < arr.length; i++)
        result += (result << 5) + arr[i];
    return result;
}

JKSN = {
    "encoder": JKSNEncoder,
    "decoder": JKSNDecoder,
    "parseFromArrayBuffer": function parse(buf) {
        return new JKSN.decoder().parseFromArrayBuffer(buf);
    },
    "parseFromString": function parse(buf) {
        return new JKSN.decoder().parseFromString(buf);
    },
    "stringifyToArrayBuffer": function stringify(obj, header) {
        return new JKSN.encoder().stringifyToArrayBuffer(obj, header);
    },
    "stringifyToString": function stringify(obj, header) {
        return new JKSN.encoder().stringifyToString(obj, header);
    }
}

window.JKSN = JKSN;

}(this));
