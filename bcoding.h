#ifndef BCODING_H_INCLUDED
#define BCODING_H_INCLUDED

#include <map>
#include <vector>
#include <string>
#include <iostream>

struct Value;
typedef std::map<std::string, Value> Dict;
typedef std::vector<Value> List;

enum Type { integer, string, list, dict  };

struct ValueError
{
};

struct TypeError : ValueError
{
    TypeError(Type type) : requested_type(type) { };

    Type requested_type;
};

struct KeyError : ValueError
{
    KeyError(const std::string &key) : requested_key(key) { };

    std::string requested_key;
};

struct Value
{
    Type type;
    long long   integer;
    std::string string;
    List        list;
    Dict        dict;

    void clear();

    inline void assign(long long i);
    inline void assign(const char *str);
    inline void assign(const char *str, size_t length);
    inline void assign(const std::string &str);

    inline long long &makeInteger();
    inline std::string &makeString();
    inline List &makeList();
    inline Dict &makeDict();

    inline const long long &asInteger() const throw (TypeError);
    inline const std::string &asString() const throw (TypeError);
    inline const List &asList() const throw (TypeError);
    inline const Dict &asDict() const throw (TypeError);

    inline List::size_type listSize() const throw (TypeError);
    inline const Value &listAt(List::size_type pos) const throw (TypeError);

    inline const Value &dictAt(const std::string &key) const throw (TypeError, KeyError);
    inline bool dictHasKey(const std::string &key) const throw (TypeError);
};

std::ostream &operator<< (std::ostream &os, const Value &value);

bool bdecode(std::istream &is, Value &value);
bool bdecode(const std::string &str, Value &value, bool allow_extra_data=true);

void bencode(std::ostream &os, const Value &value);
std::string bencode(const Value &value);


//
// Definition of inline methods
//

void Value::assign(long long i)
{
    type    = ::integer;
    integer = i;
}

void Value::assign(const char *str)
{
    type = ::string;
    string.assign(str);
}

void Value::assign(const char *str, size_t length)
{
    type = ::string;
    string.assign(str, length);
}

void Value::assign(const std::string &str)
{
    type = ::string;
    string.assign(str);
}

long long &Value::makeInteger()
{
    type = ::integer;
    return integer;
}

std::string &Value::makeString()
{
    type = ::string;
    return string;
}

List &Value::makeList()
{
    type = ::list;
    return list;
}

Dict &Value::makeDict()
{
    type = ::dict;
    return dict;
}

const long long &Value::asInteger() const throw (TypeError)
{
    if(type != ::integer)
        throw TypeError(::integer);
    return integer;
}

const std::string &Value::asString() const throw (TypeError)
{
    if(type != ::string)
        throw TypeError(::string);
    return string;
}

const List &Value::asList() const throw (TypeError)
{
    if(type != ::list)
        throw TypeError(::list);
    return list;
}

const Dict &Value::asDict() const throw (TypeError)
{
    if(type != ::dict)
        throw TypeError(::dict);
    return dict;
}

List::size_type Value::listSize() const throw (TypeError)
{
    return asList().size();
}

const Value &Value::listAt(List::size_type pos) const throw (TypeError)
{
    return asList().at(pos);
}

const Value &Value::dictAt(const std::string &key) const throw (TypeError, KeyError)
{
    const Dict &d = asDict();
    Dict::const_iterator i = d.find(key);
    if(i == d.end())
        throw KeyError(key);
    return i->second;
}

bool Value::dictHasKey(const std::string &key) const throw (TypeError)
{
    const Dict &d = asDict();
    Dict::const_iterator i = d.find(key);
    return i != d.end();
}

#endif /* ndef BCODING_H_INCLUDED */
