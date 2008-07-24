#include "bcoding.h"
#include <sstream>

void Value::clear()
{
    integer = 0;
    string.clear();
    list.clear();
    dict.clear();
}

std::ostream &operator<< (std::ostream &os, const Value &value)
{
    switch(value.type)
    {
    case integer:
        {
            os << value.integer;
        } break;
    
    case string:
        {
            os << '"' << value.string << '"';
            // FIXME: string escaping
        } break;
    
    case list:
        {
            os << "[ ";
            for( List::const_iterator i = value.list.begin();
                 i != value.list.end(); ++i )
            {
                if(i != value.list.begin())
                    os << ", ";
                os << *i;
            }
            os << " ]";
        } break;
    
    case dict:
        {
            os << "{ ";
            for( Dict::const_iterator i = value.dict.begin();
                 i != value.dict.end(); ++i )
            {
                if(i != value.dict.begin())
                    os << ", ";
                os << '"' << i->first << "\": " << i->second;
                // FIXME: string escaping (for key)
            }
            os << " }";
        } break;
    }
    return os;
}

/*
bool parse_int( const std::string &str,
    std::string::size_type begin, std::string::size_type end, long long &i )
{
    i = 0;
    bool negative = false;
    if(str[begin] == '-')
    {
        negative = true;
        ++begin;
    }
    while(begin != end)
    {
        if(str[begin] >= '0' && str[begin] <= '9')
        {
            i = 10*i + (str[begin] - '0');
            ++begin;
        }
        else
            return false;
    }
    i = negative ? -i : +i;
    return true;
}

bool bdecode(const std::string &str, Value &value, std::string::size_type &pos)
{
    value.clear();

    switch(str[pos])
    {
    case 'i':
        {
            // Parse integer
            ++pos;
            value.type = integer;
            std::string::size_type sep = str.find('e', pos);
            if(sep == std::string::npos || !parse_int(str, pos, sep, value.integer))
                return false;
            pos = sep + 1;
           return true;
        }

    case 'l':
        {
            // Parse list
            ++pos;
            value.type = list;
    
            while(1)
            {
                if(pos == str.size())
                    return false;
                if(str[pos] == 'e')
                    break;
    
                value.list.resize(value.list.size() + 1);
                if(!bdecode(str, value.list.back(), pos))
                    return false;
            }
            ++pos;
            return true;
        }

    case 'd':
        {
            // Parse dictionary
            ++pos;
            value.type = dict;
    
            while(1)
            {
                if(pos == str.size())
                    return false;
                if(str[pos] == 'e')
                    break;
    
                Value elem_key;
                if(!bdecode(str, elem_key, pos) || elem_key.type != string)
                    return false;
                if(pos == str.size())
                    return false;
                Value &elem_value = value.dict[elem_key.string];
                if(!bdecode(str, elem_value, pos))
                    return false;
            }
            ++pos;
            return true;
        }

    default:
        {
            // Parse string
            value.type = string;
            std::string::size_type sep = str.find(':', pos);
            if(sep == std::string::npos)
                return false;
            long long len;
            if( !parse_int(str, pos, sep, len) || len < 0 ||
                std::string::size_type(len) > str.size() - (sep + 1) )
                return false;
            value.string.assign(str, sep + 1, len);
            pos = sep + 1 + len;
            return true;
        }
    }

    return false;   // shouldn't get here
}
*/

bool bdecode(std::istream &is, Value &value)
{
    value.clear();

    switch(is.peek())
    {
    case 'i':
        {   // Parse integer
            is.get();
            value.type = integer;
            if(is >> value.integer && is.get() == 'e')
                return true;
            return false;
        }

    case 'l':
        {   // Parse list
            is.get();
            value.type = list;
            while(is.peek() != 'e')
            {
                value.list.resize(value.list.size() + 1);
                if(!bdecode(is, value.list.back()))
                    return false;
            }
            is.get();
            return true;
        }

    case 'd':
        {   // Parse dictionary
            is.get();
            value.type = dict;
            while(is.peek() != 'e')
            {
                Value elem_key;
                if(!bdecode(is, elem_key) || elem_key.type != string)
                    return false;
                Value &elem_value = value.dict[elem_key.string];
                if(!bdecode(is, elem_value))
                    return false;
            }
            is.get();
            return true;
        }

    default:
        {   // Parse string
            value.type = string;
            
            long long len;
            if(!(is >> len) || is.get() != ':')
                return false;
            value.string.resize(len);   // FIXME: could throw bad_alloc on malformed input
            is.read(const_cast<char*>(value.string.data()), len);   // FIXME: hack!
            return is.good();
        }
    }

    return false;   // shouldn't get here
}

bool bdecode(const std::string &str, Value &value, bool allow_extra_data)
{
    std::istringstream iss(str);
    return bdecode(iss, value) && (allow_extra_data || iss.peek() == EOF);
}

void bencode(std::ostream &os, const Value &value)
{
    switch(value.type)
    {
    case integer:
        {
            os << 'i' << value.integer << 'e';
        } break;

    case string:
        {
            os << value.string.size() << ':' << value.string;
        } break;

    case list:
        {
            os << 'l';
            for( List::const_iterator i = value.list.begin();
                 i != value.list.end(); ++i )
            {
                bencode(os, *i);
            }
            os << 'e';
        } break;

    case dict:
        {
            os << 'd';
            for( Dict::const_iterator i = value.dict.begin();
                 i != value.dict.end(); ++i )
            {
                os << i->first.size() << ':' << i->first;
                bencode(os, i->second);
            }
            os << 'e';
        } break;
    }
}

std::string bencode(const Value &value)
{
    std::ostringstream oss;
    bencode(oss, value);
    return oss.str();
}
/*
int main()
{
    std::string test = "d3:barl4:spam4:eggse3:fooi42ee";
    Value value;
    if(bdecode(test, value))
    {
        std::cout << value << std::endl;
        std::cout << "Verification " << (test == bencode(value) ? "passed." : "FAILED!") << std::endl;
    }
    else
        std::cerr << "bdecoding failed!" << std::endl;
    return 0;
}
*/
