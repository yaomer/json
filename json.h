#ifndef _JSON_H
#define _JSON_H

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <fstream>

#include <iostream>

namespace json {

enum ValueType {
    String,
    Number,
    Object,
    Array,
    Boolean,
    Null,
};

// JSON String
typedef std::string string;
// JSON Number
typedef int64_t number;
// JSON Value
struct value;
typedef std::unique_ptr<value> value_ptr;
// JSON Object
typedef std::unordered_map<string, value_ptr> object;
typedef std::unique_ptr<object> object_ptr;
// JSON Array
typedef std::vector<value_ptr> array;
typedef std::unique_ptr<array> array_ptr;

//////////////////// JSON value ////////////////////

class value {
public:
    typedef std::variant<string, number, object_ptr, array_ptr, bool> union_value;
    value() : type(Null) {  }
    value(value&& val) : type(val.type), uv(std::move(val.uv)) {  }
    value& operator=(value&& val);
    // For String
    value(const char *val) : type(String), uv(string(val)) {  }
    value(const string& val) : type(String), uv(val) {  }
    value(string&& val) : type(String), uv(std::move(val)) {  }
    value& operator=(const char *val);
    value& operator=(const string& val);
    value& operator=(string&& val);
    // For Number
    template <typename T,
              typename = std::enable_if_t<std::is_integral_v<T>>>
    value(T val) : type(Number), uv(static_cast<number>(val)) {  }
    template <typename T,
              typename = std::enable_if_t<std::is_integral_v<T>>>
    value& operator=(T val)
    {
        type = Number;
        uv = static_cast<number>(val);
        return *this;
    }
    // For Boolean
    value(bool val) : type(Boolean), uv(val) {  }
    value& operator=(bool val);
    // For Object
    value(object&& val) : type(Object), uv(object_ptr(new object(std::move(val)))) {  }
    value& operator=(object&& val);
    // For Array
    value(array&& val) : type(Array), uv(array_ptr(new array(std::move(val)))) {  }
    value& operator=(array&& val);
    // Init an Array by list
    template <typename T>
    value(std::initializer_list<T> l);
    template <typename T>
    value& operator=(std::initializer_list<T> l);
    // The value can be any other ValueType,
    // which we override as Object.
    value& operator[](const string& key);
    value& at(const string& key);
    // The value can be any other ValueType,
    // which we override as Array;
    template <typename T>
    value& append(T&& val);
    template <typename T>
    value& append(std::initializer_list<T> l);
    // The value must be an Array, and ensure that
    // the subscript access does not exceed the bounds.
    value& operator[](size_t i);
    value& at(size_t i);
    // Judge value type
    ValueType get_type() { return type; }
    bool is_string() { return type == String; }
    bool is_number() { return type == Number; }
    bool is_object() { return type == Object; }
    bool is_array() { return type == Array; }
    bool is_boolean() { return type == Boolean; }
    bool is_null() { return type == Null; }
    // Take out value
    string& as_string() { return std::get<string>(uv); }
    number as_number() { return std::get<number>(uv); }
    object& as_object() { return *std::get<object_ptr>(uv); }
    array& as_array() { return *std::get<array_ptr>(uv); }
    bool as_boolean() { return std::get<bool>(uv); }
private:
    ValueType type;
    union_value uv;
    friend class parser;
};

value& value::operator=(value&& val)
{
    type = val.type;
    uv = std::move(val.uv);
    return *this;
}

value& value::operator=(const char *val)
{
    return operator=(std::string(val));
}

value& value::operator=(const string& val)
{
    type = String;
    uv = val;
    return *this;
}

value& value::operator=(string&& val)
{
    type = String;
    uv = std::move(val);
    return *this;
}

value& value::operator=(bool val)
{
    type = Boolean;
    uv = val;
    return *this;
}

value& value::operator=(object&& val)
{
    type = Object;
    uv = object_ptr(new object(std::move(val)));
    return *this;
}

value& value::operator=(array&& val)
{
    type = Array;
    uv = array_ptr(new array(std::move(val)));
    return *this;
}

template <typename T>
value::value(std::initializer_list<T> l) : type(Array), uv(array_ptr(new array()))
{
    for (auto& e : l)
        as_array().emplace_back(new value(e));
}

template <typename T>
value& value::operator=(std::initializer_list<T> l)
{
    *this = std::move(array());
    for (auto& e : l)
        as_array().emplace_back(new value(e));
    return *this;
}

value& value::operator[](const string& key)
{
    if (type != Object) {
        *this = std::move(object());
    }
    auto& o = as_object();
    if (!o.count(key)) {
        o.emplace(key, new value());
    }
    return *o[key];
}

value& value::at(const string& key)
{
    return operator[](key);
}

template <typename T>
value& value::append(T&& val)
{
    if (type != Array) {
        *this = std::move(array());
    }
    as_array().emplace_back(new value(val));
    return *this;
}

template <typename T>
value& value::append(std::initializer_list<T> l)
{
    if (type != Array) {
        *this = std::move(array());
    }
    array a;
    for (auto& e : l)
        a.emplace_back(new value(e));
    as_array().emplace_back(new value(std::move(a)));
    return *this;
}

value& value::operator[](size_t i)
{
    return *as_array()[i];
}

value& value::at(size_t i)
{
    return *as_array().at(i);
}

//////////////////// Parser ////////////////////

class parser {
public:
    bool parse(value& value, const std::string& s)
    {
        charstream.reset(new string_stream(this, s));
        return parse(value);
    }
    bool parsefile(value& value, const std::string& filename)
    {
        charstream.reset(new file_stream(this, filename));
        return parse(value);
    }
    const char *error() { return errmsg.c_str(); }
private:
    // Convenient to deal parse error
    struct parse_error {
        parse_error(const std::string& msg) : msg(msg) {  }
        std::string msg;
    };

    // Abstract the input to a stream of characters
    struct char_stream {
        char_stream(parser *psr) : parser(psr) {  }
        virtual ~char_stream() {  }
        // Extracts a character
        virtual int nextchar() = 0;
        // Unextracts a character
        virtual void backward() = 0;
        // Reaches the next non-whitespace character
        int skipspace()
        {
            while (isspace(nextchar())) ;
            return parser->c;
        }
        // Set parser.c to cur character
        parser *parser;
    };

    bool parse(value& value)
    {
        try {
            value = std::move(parse_value());
        } catch (parse_error& e) {
            errmsg = e.msg;
            return false;
        }
        return true;
    }
    value parse_value()
    {
        charstream->skipspace();
        switch (c) {
        case '{': return parse_object();
        case '[': return parse_array();
        case '\"': return parse_string();
        case 't': case 'f': return parse_boolean();
        case 'n': return parse_null();
        default: return parse_number();
        }
    }
    object parse_object()
    {
        object o;
        while (true) {
            charstream->skipspace();
            if (c == '}') break;
            if (c != '"') throw parse_error("expected string, missing the '\"'");
            auto key = parse_string();
            charstream->skipspace();
            if (c != ':') throw parse_error("expected pair<string : value>, missing the ':'");
            value *value = new class value(parse_value());
            auto it = o.emplace(key, value);
            // For the same key, the old value will be overwritten
            if (!it.second) o[key] = value_ptr(value);
            charstream->skipspace();
            if (c == '}') break;
            if (c != ',') throw parse_error("expected object<pair1, pair2, ...>, missing the ','");
        }
        return o;
    }
    array parse_array()
    {
        array a;
        charstream->skipspace();
        if (c == ']') return a;
        charstream->backward();
        while (true) {
            a.emplace_back(new value(parse_value()));
            charstream->skipspace();
            if (c == ']') break;
            if (c != ',') throw parse_error("expected array<value1, value2, ...>, missing the ','");
        }
        return a;
    }
    // TODO: Handles UTF-8
    std::string parse_string()
    {
        std::string s;
        while (true) {
            charstream->nextchar();
            if (c == '\\') {
                charstream->nextchar();
                switch (c) {
                    case '\"': case '\\': case '/': case '\b':
                    case '\f': case '\n': case '\r': case '\t':
                        charstream->nextchar();
                        break;
                    default:
                        throw parse_error("invalid escape character");
                }
            }
            if (c == '\"') break;
            s.push_back(c);
        }
        return s;
    }
    // TODO: Handles more complete integers and floating points.
    number parse_number()
    {
        number n = 0;
        if (!isnumber(c)) {
            throw parse_error("unknown value type");
        }
        while (isnumber(c)) {
            n = n * 10 + (c - '0');
            charstream->nextchar();
        }
        charstream->backward();
        return n;
    }
    bool parse_boolean()
    {
        int c0 = c;
        int c1 = charstream->nextchar();
        int c2 = charstream->nextchar();
        int c3 = charstream->nextchar();
        if (c0 == 't' && c1 == 'r' && c2 == 'u' && c3 == 'e')
            return true;
        else
            throw parse_error("expected 'true'");
        int c4 = charstream->nextchar();
        if (c0 == 'f' && c1 == 'a' && c2 == 'l' && c3 == 's' && c4 == 'e')
            return false;
        else
            throw parse_error("expected 'false'");
    }
    value parse_null()
    {
        int c0 = c;
        int c1 = charstream->nextchar();
        int c2 = charstream->nextchar();
        int c3 = charstream->nextchar();
        if (c0 == 'n' && c1 == 'u' && c2 == 'l' && c3 == 'l')
            return value();
        else
            throw parse_error("expected 'null'");
    }

    struct string_stream : char_stream {
        string_stream(class parser *psr, const std::string& s)
            : char_stream(psr), p(s.data()), end(s.data() + s.size()) {  }
        int nextchar() override
        {
            if (p >= end) throw parse_error("incomplete json object");
            return parser->c = *p++;
        }
        void backward() override { p--; }
        const char *p, *end;
    };

    struct file_stream : char_stream {
        file_stream(class parser *psr, const std::string& filename)
            : char_stream(psr), ifs(filename) {  }
        int nextchar() override
        {
            if (ifs.eof()) throw parse_error("incomplete json object");
            return parser->c = ifs.get();
        }
        void backward() override { ifs.unget(); }
        std::ifstream ifs;
    };

    int c; // cur valid character
    std::unique_ptr<char_stream> charstream;
    std::string errmsg;
};

//////////////////// Writer ////////////////////

class writer {
public:
    // The default format is compact.
    // Or you can visualize it in a 2, 4, or 8 space indentation format.
    void dump(value& value, std::string& s, int spaces = 0)
    {
        visual_init(spaces);
        buf.clear();
        dump_value(value);
        buf.swap(s);
    }
private:
    void visual_init(int spaces)
    {
        if (spaces > 8) {
            spaces = 8; // 8-space indent is enough!
        }
        if (spaces > 0) {
            visual = true;
            indent_spaces = spaces;
        } else {
            visual = false;
            indent_spaces = 0;
        }
        cur_level = 0;
    }
    void dump_value(value& value)
    {
        auto type = value.get_type();
        switch (type) {
        case String: dump_string(value); break;
        case Number: dump_number(value); break;
        case Object: dump_object(value); break;
        case Array: dump_array(value); break;
        case Boolean: dump_boolean(value); break;
        case Null: dump_null(value); break;
        }
    }
    void dump_string(const string& s)
    {
        buf.append("\"").append(s).append("\"");
    }
    void dump_string(value& value)
    {
        dump_string(value.as_string());
    }
    void dump_number(value& value)
    {
        buf.append(std::to_string(value.as_number()));
    }
    void dump_object(value& value)
    {
        int level = cur_level;
        cur_level += indent_spaces;
        buf.push_back('{');
        add_newline();
        for (auto& [k, v] : value.as_object()) {
            add_spaces();
            dump_string(k);
            buf.append(":");
            dump_value(*v);
            buf.append(",");
            add_newline();
        }
        cur_level = level;
        handle_right_indent('}');
    }
    void dump_array(value& value)
    {
        int level = cur_level;
        cur_level += indent_spaces;
        buf.push_back('[');
        add_newline();
        for (auto& e : value.as_array()) {
            add_spaces();
            dump_value(*e);
            buf.append(",");
            add_newline();
        }
        cur_level = level;
        handle_right_indent(']');
    }
    void dump_boolean(value& value)
    {
        if (value.as_boolean()) buf.append("true");
        else buf.append("false");
    }
    void dump_null(value& value)
    {
        buf.append("null");
    }
    void add_newline()
    {
        if (visual) buf.push_back('\n');
    }
    void add_spaces()
    {
        if (visual) buf.append(cur_level, ' ');
    }
    void handle_right_indent(int c)
    {
        if (visual) {
            buf.pop_back();
            buf.back() = '\n';
            add_spaces();
            buf.push_back(c);
        } else {
            buf.back() = c;
        }
    }

    std::string buf;
    bool visual;
    int indent_spaces; // A few spaces to indent
    int cur_level; // Current indent level
};

}

#endif // _JSON_H
