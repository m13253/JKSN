/*
  Copyright (c) 2014 StarBrilliant <m13253@hotmail.com>
  All rights reserved.

  Redistribution and use in source and binary forms are permitted
  provided that the above copyright notice and this paragraph are
  duplicated in all such forms and that any documentation,
  advertising materials, and other materials related to such
  distribution and use acknowledge that the software was developed by
  StarBrilliant.
  The name of StarBrilliant may not be used to endorse or promote
  products derived from this software without specific prior written
  permission.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
*/

#include "jksn.hpp"
#include <cmath>
#include <sstream>

using namespace JKSN;

JKSNValue::~JKSNValue() {
    switch(this->getType()) {
    case JKSN_STRING:
        delete this->data_string;
        break;
    case JKSN_ARRAY:
        delete this->data_array;
        break;
    case JKSN_OBJECT:
        delete this->data_object;
        break;
    default:
        break;
    };
}

JKSNValue::operator bool() const {
    switch(this->getType()) {
    case JKSN_UNDEFINED:
    case JKSN_NULL:
        return false;
    case JKSN_BOOL:
        return this->data_bool;
    case JKSN_INT:
        return this->data_int != 0;
    case JKSN_FLOAT:
        return this->data_float != 0.0f;
    case JKSN_DOUBLE:
        return this->data_double != 0.0;
    case JKSN_LONG_DOUBLE:
        return this->data_long_double != 0.0L;
    case JKSN_STRING:
    case JKSN_BLOB:
        return this->data_string->size() != 0;
    case JKSN_ARRAY:
        return this->data_array->size() != 0;
    case JKSN_OBJECT:
        return this->data_object->size() != 0;
    default:
        throw JKSNTypeError();
    }
}

JKSNValue::operator intmax_t() const {
    switch(this->getType()) {
    case JKSN_NULL:
        return 0;
    case JKSN_BOOL:
        return this->data_bool;
    case JKSN_INT:
        return this->data_int;
    case JKSN_FLOAT:
        return this->data_float;
    case JKSN_DOUBLE:
        return this->data_double;
    case JKSN_LONG_DOUBLE:
        return this->data_long_double;
    case JKSN_STRING:
        try {
            return std::stoll(*this->data_string);
        } catch(std::invalid_argument) {
            throw JKSNTypeError();
        } catch(std::out_of_range) {
            throw JKSNTypeError();
        }
    default:
        throw JKSNTypeError();
    }
}

template<typename T> inline T JKSNValue::toNumber() const {
    switch(this->getType()) {
    case JKSN_NULL:
        return 0;
    case JKSN_BOOL:
        return this->data_bool;
    case JKSN_INT:
        return this->data_int;
    case JKSN_FLOAT:
        return this->data_float;
    case JKSN_DOUBLE:
        return this->data_double;
    case JKSN_LONG_DOUBLE:
        return this->data_long_double;
    case JKSN_STRING:
        try {
            return std::stoll(*this->data_string);
        } catch(std::invalid_argument) {
            return NAN;
        } catch(std::out_of_range) {
            return NAN;
        }
    default:
        return NAN;
    }
}

JKSNValue::operator std::string() const {
    switch(this->getType()) {
    case JKSN_UNDEFINED:
        return "undefined";
    case JKSN_NULL:
        return "null";
    case JKSN_BOOL:
        return this->data_bool ? "true" : "false";
    case JKSN_INT:
        return std::to_string(this->data_int);
    case JKSN_FLOAT:
        if(std::isnan(this->data_float))
            return "NaN";
        else if(std::isinf(this->data_float))
            return this->data_float >= 0 ? "Infinity" : "-Infinity";
        else
            return std::to_string(this->data_float);
    case JKSN_DOUBLE:
        if(std::isnan(this->data_double))
            return "NaN";
        else if(std::isinf(this->data_double))
            return this->data_double >= 0 ? "Infinity" : "-Infinity";
        else
            return std::to_string(this->data_double);
    case JKSN_LONG_DOUBLE:
        if(std::isnan(this->data_long_double))
            return "NaN";
        else if(std::isinf(this->data_long_double))
            return this->data_long_double >= 0 ? "Infinity" : "-Infinity";
        else
            return std::to_string(this->data_long_double);
    case JKSN_STRING:
    case JKSN_BLOB:
        return *this->data_string;
    case JKSN_ARRAY:
        {
            std::stringstream res;
            bool first = true;
            for(const JKSNValue &i : *this->data_array) {
                if(!first) {
                    res << ',';
                    first = false;
                }
                res << static_cast<std::string>(i);
            }
            return res.str();
        }
    case JKSN_OBJECT:
        return "[object Object]";
    default:
        throw JKSNTypeError();
    }
}

