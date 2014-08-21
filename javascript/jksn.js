(function(window) {

function JKSNEncoder() {
    var lastint = null;
    var texthash = new Array(256);
    var blobhash = new Array(256);
    function JKSNValue(origin, control, data, buf) {
        if(!(control >= 0 && control <= 255))
            throw "Assertion failed: control >= 0 && control <= 255";
        return {
            "origin": origin,
            "control": control,
            "data": data || "",
            "buf": buf || "",
            "children": [],
            "toString": function (recursive) {
                var result = [String.fromCharCode(this.control), this.data, this.buf];
                result.push.apply(result, this.children);
                return result.join("");
            },
            "getSize": function (depth) {
                var result = 1 + this.data.length + this.buf.length;
                if(depth === undefined)
                    depth = 0;
                if(depth != 1)
                    result = this.children.reduce(function (a, b) { return a + b.getSize(depth-1); }, result);
                return result;
            }
        };
    }
    function unspecifiedValue() {
    }
    function dumpToObject(obj) {
        return optimize(dumpValue(obj));
    }
    function dumpValue(obj) {
        if(obj === undefined)
            return JKSNValue(obj, 0x00);
        else if(obj === null)
            return JKSNValue(obj, 0x01);
        else if(obj === false)
            return JKSNValue(obj, 0x02);
        else if(obj === true)
            return JKSNValue(obj, 0x03);
        else if(obj instanceof unspecifiedValue)
            return JKSNValue(obj, 0xa0);
        else if(typeof obj === "number")
            return dumpNumber(obj);
        else if(typeof obj === "string")
            return dumpString(obj);
        else if(obj instanceof ArrayBuffer)
            return dumpBuffer(obj);
        else if(typeof obj !== "function" && obj.length !== undefined)
            return dumpArray(obj);
        else
            return dumpObject(obj);
    }
    function dumpNumber(obj) {
        if(isNaN(obj))
            return JKSNValue(obj, 0x20);
        else if(!isFinite(obj))
            return JKSNValue(obj, obj >= 0 ? 0x2f : 0x2e);
        else if((obj | 0) === obj)
            if(obj >= 0 && obj <= 0xa)
                return JKSNValue(obj, 0x10 | obj);
            else if(obj >= -0x80 && obj <= 0x7f)
                return JKSNValue(obj, 0x1d, encodeInt(obj, 1));
            else if(obj >= -0x8000 && obj <= 0x7fff)
                return JKSNValue(obj, 0x1c, encodeInt(obj, 2));
            else if((obj >= -0x80000000 && obj <= -0x200000) || (obj >= 0x200000 && obj <= 0x7fffffff))
                return JKSNValue(obj, 0x1b, encodeInt(obj, 4));
            else if(obj >= 0)
                return JKSNValue(obj, 0x1f, encodeInt(obj, 0));
            else
                return JKSNValue(obj, 0x1e, -encodeInt(obj, 0));
        else {
            var f64buf = new DataView(new ArrayBuffer(8));
            f64buf.setFloat64(0, obj, false);
            return JKSNValue(obj, 0x2c, encodeInt(f64buf.getUint32(0, false), 4)+encodeInt(f64buf.getUint32(4, false), 4));
        }
    }
    function dumpString(obj) {
        var obj_utf8 = unescape(encodeURIComponent(obj));
        var obj_utf16 = new Uint8Array(obj.length << 1);
        for(var i = 0, j = 0, k = 1; i < obj.length; i++, j += 2, k += 2) {
            var charCodeI = obj.charCodeAt(i);
            obj_utf16[j] = charCodeI;
            obj_utf16[k] = charCodeI >>> 8;
        }
        obj_utf16 = String.fromCharCode.apply(null, obj_utf16);
        if(obj_utf16.length < obj_utf8.length) {
            var obj_short = obj_utf16;
            var control = 0x30;
            var strlen = obj_utf16.length >>> 1;
        } else {
            var obj_short = obj_utf8;
            var control = 0x40;
            var strlen = obj_utf8.length;
        }
        if(strlen <= (control == 0x40 ? 0xc : 0xb))
            var result = JKSNValue(obj, control | strlen, "", obj_short);
        else if(strlen <= 0xff)
            var result = JKSNValue(obj, control | 0xe, encodeInt(strlen, 1), obj_short);
        else if(strlen <= 0xffff)
            var result = JKSNValue(obj, control | 0xd, encodeInt(strlen, 2), obj_short);
        else
            var result = JKSNValue(obj, control | 0xf, encodeInt(strlen, 0), obj_short);
        result.hash = DJBHash(obj_short);
        return result;
    }
    function dumpBuffer(obj) {
        var strbuf = new Uint8Array(obj);
        var str = String.fromCharCode.apply(null, strbuf);
        var strlen = str.length;
        if(strlen <= 0xb)
            var result = JKSNValue(obj, 0x50 | strlen, "", str);
        else if(strlen <= 0xff)
            var result = JKSNValue(obj, 0x5e, encodeInt(strlen, 1), str);
        else if(strlen <= 0xffff)
            var result = JKSNValue(obj, 0x5e, encodeInt(strlen, 2), str);
        else
            var result = JKSNValue(obj, 0x5e, encodeInt(strlen, 0), str);
        result.hash = DJBHash(strbuf);
        return result;
    }
    function dumpArray(obj) {
        function testSwapAvailability(obj) {
            columns = false;
            for(var row = 0; row < obj.length; row++)
                if(typeof obj[row] !== "object" || obj[row] === undefined || obj[row] === null || obj[row] instanceof unspecifiedValue)
                    return false;
                else
                    for(var key in obj[row]) {
                        columns = true;
                        break;
                    }
            return columns;
        }
        function encodeStraightArray(obj) {
            objlen = obj.length;
            if(objlen <= 0xc)
                var result = JKSNValue(obj, 0x80 | objlen);
            else if(objlen <= 0xff)
                var result = JKSNValue(obj, 0x8e, encodeInt(objlen, 1));
            else if(objlen <= 0xffff)
                var result = JKSNValue(obj, 0x8d, encodeInt(objlen, 2));
            else
                var result = JKSNValue(obj, 0x8f, encodeInt(objlen, 0));
            result.children = obj.map(dumpValue);
            if(result.children.length != objlen)
                throw "Assertion failed: result.children.length == objlen";
            return result;
        }
        function encodeSwappedArray(obj) {
            var columns = [];
            var columns_set = {};
            for(var row = 0; row < obj.length; row++)
                for(var column in obj[row])
                    if(!columns_set[column]) {
                        columns.push(column)
                        columns_set[column] = true;
                    }
            var collen = columns.length;
            if(collen <= 0xc)
                var result = JKSNValue(obj, 0xa0 | collen);
            else if(collen <= 0xff)
                var result = JKSNValue(obj, 0xae, encodeInt(collen, 1));
            else if(collen <= 0xffff)
                var result = JKSNValue(obj, 0xad, encodeInt(collen, 2));
            else
                var result = JKSNValue(obj, 0xaf, encodeInt(collen, 0));
            for(var column = 0; column < collen; column++) {
                var columns_value = new Array(obj.length);
                for(var row = 0; row < obj.length; row++)
                    columns_value[row] = (obj[row][columns[column]] !== undefined ? obj[row][columns[column]] : new unspecifiedValue())
                result.children.push(dumpValue(columns[column]), dumpArray(columns_value));
            }
            if(result.children.length != collen * 2)
                throw "Assertion failed: result.children.length == columns.length * 2";
            return result;
        }
        var result = encodeStraightArray(obj);
        if(testSwapAvailability(obj)) {
            var resultSwapped = encodeSwappedArray(obj);
            if(resultSwapped.getSize(3) < result.getSize(3))
                result = resultSwapped;
        }
        return result;
    }
    function dumpObject(obj) {
        var objlen = 0;
        var children = [];
        for(var key in obj) {
            objlen++;
            children.push(dumpValue(key), dumpValue(obj[key]));
        }
        if(objlen <= 0xc)
            var result = JKSNValue(obj, 0x90 | objlen);
        else if(objlen <= 0xff)
            var result = JKSNValue(obj, 0x9e, encodeInt(objlen, 1));
        else if(objlen <= 0xffff)
            var result = JKSNValue(obj, 0x9d, encodeInt(objlen, 2));
        else
            var result = JKSNValue(obj, 0x9f, encodeInt(objlen, 0));
        result.children = children;
        return result;
    }
    function optimize(obj) {
        var control = obj.control & 0xf0;
        switch(control) {
        case 0x10:
            if(lastint !== null) {
                var delta = obj.origin - lastint;
                if(Math.abs(delta) < Math.abs(obj.origin)) {
                    if(delta >= 0 && delta <= 0x5) {
                        newControl = 0xb0 | delta;
                        newData = "";
                    } else if(delta >= -0x5 && delta <= -0x1) {
                        newControl = 0xb0 | (delta+11);
                        newData = "";
                    } else if(delta >= -0x80 && delta <= 0x7f) {
                        newControl = 0xbd;
                        newData = encodeInt(delta, 1);
                    } else if(delta >= -0x8000 && delta <= 0x7fff) {
                        newControl = 0xbc;
                        newData = encodeInt(delta, 2);
                    } else if((delta >= -0x80000000 && delta <= -0x200000) || (delta >= 0x200000 && delta <= 0x7fffffff)) {
                        newControl = 0xbb;
                        newData = encodeInt(delta, 4);
                    } else if(delta >= 0) {
                        newControl = 0xbf;
                        newData = encodeInt(delta, 0);
                    } else {
                        newControl = 0xbe;
                        newData = encodeInt(-delta, 0);
                    }
                    if(newData.length < obj.data.length) {
                        obj.control = newControl;
                        obj.data = newData;
                    }
                }
            }
            lastint = obj.origin;
            break;
        case 0x30:
        case 0x40:
            if(obj.buf.length > 1)
                if(texthash[obj.hash] == obj.buf) {
                    obj.control = 0x3c;
                    obj.data = encodeInt(obj.hash, 1);
                    obj.buf = "";
                } else
                    texthash[obj.hash] = obj.buf;
            break;
        case 0x50:
            if(obj.buf.length > 1)
                if(blobhash[obj.hash] == obj.buf) {
                    obj.control = 0x5c;
                    obj.data = encodeInt(obj.hash, 1);
                    obj.buf = "";
                } else
                    blobhash[obj.hash] = obj.buf;
            break;
        default:
            for(var i = 0; i < obj.children.length; i++)
                optimize(obj.children[i]);
        }
        return obj;
    }
    function encodeInt(number, size) {
        switch(size) {
        case 1:
            return String.fromCharCode(number & 0xff);
        case 2:
            return String.fromCharCode((number >>> 8) & 0xff, number & 0xff);
        case 4:
            return String.fromCharCode((number >>> 24) & 0xff, (number >>> 16) & 0xff, (number >>> 8) & 0xff, number & 0xff);
        case 0:
            if(!(number >= 0))
                throw "Assertion failed: number >= 0";
            result = [number & 0x7f];
            number >>>= 7;
            while(number != 0) {
                result.unshift((number & 0x7f) | 0x80);
                number >>>= 7;
            }
            return String.fromCharCode.apply(null, result);
        default:
            throw "Assertion failed: size in [1, 2, 4, 0]";
        }
    }
    return {
        "stringifyToArrayBuffer": function (obj, header) {
            var str = this.stringifyToString(obj, header);
            var buf = new ArrayBuffer(str.length);
            var bufview = new Uint8Array(buf);
            for(var i = 0; i < str.length; i++)
                bufview[i] = str.charCodeAt(i);
            return buf;
        },
        "stringifyToString": function (obj, header) {
            var result = dumpToObject(obj);
            if(header || header === undefined)
                return "jk!"+result;
            else
                return result.toString();
        }
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
                    if(column_values.length === undefined)
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
        var result = new Uint16Array(arr.length >>> 1);
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
    if(arr.charCodeAt)
        for(var i = 0; i < arr.length; i++)
            result += (result << 5) + arr.charCodeAt(i);
    else
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
