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
#include <cassert>
#include <cmath>
#include <sstream>

using namespace JKSN;

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

std::string JKSNValue::toString() const {
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
                res << i.toString();
            }
            return res.str();
        }
    case JKSN_OBJECT:
        return "[object Object]";
    default:
        throw JKSNTypeError();
    }
}

bool JKSNValue::operator<(const JKSNValue &that) const {
    jksn_data_type this_type = this->getType();
    jksn_data_type that_type = that.getType();
    if(this_type == that_type || (this->isNumber() && that.isNumber()))
        switch(this_type) {
        case JKSN_BOOL:
            return this->toBool() < that.toBool();
        case JKSN_INT:
            switch(that_type) {
            case JKSN_INT:
                return this->toInt() < that.toInt();
            case JKSN_FLOAT:
                return this->toFloat() < that.toFloat();
            case JKSN_DOUBLE:
                return this->toDouble() < that.toDouble();
            case JKSN_LONG_DOUBLE:
                return this->toLongDouble() < that.toLongDouble();
            default:
                assert(that.isNumber());
                throw std::logic_error("unknown error occured during value comparision");
            }
        case JKSN_FLOAT:
            switch(that_type) {
            case JKSN_INT:
            case JKSN_FLOAT:
                return this->toFloat() < that.toFloat();
            case JKSN_DOUBLE:
                return this->toDouble() < that.toDouble();
            case JKSN_LONG_DOUBLE:
                return this->toLongDouble() < that.toLongDouble();
            default:
                assert(that.isNumber());
                throw std::logic_error("unknown error occured during value comparision");
            }
        case JKSN_DOUBLE:
            switch(that_type) {
            case JKSN_INT:
            case JKSN_FLOAT:
            case JKSN_DOUBLE:
                return this->toDouble() < that.toDouble();
            case JKSN_LONG_DOUBLE:
                return this->toLongDouble() < that.toLongDouble();
            default:
                assert(that.isNumber());
                throw std::logic_error("unknown error occured during value comparision");
            }
        case JKSN_LONG_DOUBLE:
            switch(that_type) {
            case JKSN_INT:
            case JKSN_FLOAT:
            case JKSN_DOUBLE:
            case JKSN_LONG_DOUBLE:
                return this->toLongDouble() < that.toLongDouble();
            default:
                assert(that.isNumber());
                throw std::logic_error("unknown error occured during value comparision");
            }
        case JKSN_STRING:
        case JKSN_BLOB:
            return this->toString() < that.toString();
        case JKSN_ARRAY:
            {
                const std::vector<JKSNValue> &this_vector = this->toVector();
                const std::vector<JKSNValue> &that_vector = that.toVector();
                auto this_iter = this_vector.cbegin();
                auto that_iter = that_vector.cbegin();
                while(this_iter != this_vector.cend()) {
                    if(that_iter == that_vector.cend())
                        return false;
                    else if(*this_iter < *that_iter)
                        return true;
                    else if(*this_iter != *that_iter) /* > */
                        return false;
                    ++this_iter;
                    ++that_iter;
                }
                return that_iter != that_vector.cend();
            }
        case JKSN_OBJECT:
            {
                const std::map<JKSNValue, JKSNValue> &this_map = this->toMap();
                const std::map<JKSNValue, JKSNValue> &that_map = this->toMap();
                auto this_iter = this_map.cbegin();
                auto that_iter = that_map.cbegin();
                while(this_iter != this_map.cend()) {
                    if(that_iter == that_map.cend())
                        return false;
                    else if((*this_iter).first < (*that_iter).first)
                        return true;
                    else if((*this_iter).first != (*that_iter).first) /* > */
                        return false;
                    else if((*this_iter).second < (*that_iter).second)
                        return true;
                    else if((*this_iter).second != (*that_iter).second)
                        return false;
                    ++this_iter;
                    ++that_iter;
                }
                return that_iter != that_map.cend();
            }
        default:
            return false;
        }
    else
        return this_type < that_type;
}
