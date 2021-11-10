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
typedef double number;
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
              typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    value(T val) : type(Number), uv(static_cast<number>(val)) {  }
    template <typename T,
              typename = std::enable_if_t<std::is_arithmetic_v<T>>>
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

enum error_code {
    invalid_value_type,
    invalid_object,
    invalid_array,
    invalid_escape_character,
    invalid_number,
    number_out_of_range,
    invalid_constant,
    incomplete,
    extra,
};

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
    error_code get_error_code() { return errcode; }
    static const char *get_error_string(error_code code);
private:
    // Abstract the input to a stream of characters
    struct char_stream {
        char_stream(parser *psr) : parser(psr) {  }
        virtual ~char_stream() {  }
        // Extracts a character
        virtual int nextchar() = 0;
        // Unextracts a character
        virtual void backward() = 0;
        // At the end of the stream ?
        virtual bool eof() = 0;
        // Reaches the next non-whitespace character
        int skipspace()
        {
            while (isspace(nextchar())) ;
            return parser->c;
        }
        // Set parser.c to cur character
        parser *parser;
    };

    // This is where we catch the exception,
    // and provide the error code to user.
    bool parse(value& value)
    {
        try {
            value = std::move(parse_value());
            while (!eof() && isspace(nextchar())) ;
            if (!eof()) throw extra;
        } catch (error_code code) {
            errcode = code;
            return false;
        }
        return true;
    }
    value parse_value()
    {
        skipspace();
        switch (c) {
        case '{': return parse_object();
        case '[': return parse_array();
        case '"': return parse_string();
        case 't': case 'f': case 'n': return parse_constant();
        default: return parse_number();
        }
    }
    object parse_object()
    {
        object o;
        while (true) {
            skipspace();
            if (c == '}') break;
            if (c != '"') throw invalid_object;
            auto key = parse_string();
            skipspace();
            if (c != ':') throw invalid_object;
            value *value = new class value(parse_value());
            auto it = o.emplace(key, value);
            // For the same key, the old value will be overwritten
            if (!it.second) o[key] = value_ptr(value);
            skipspace();
            if (c == '}') break;
            if (c != ',') throw invalid_object;
        }
        return o;
    }
    array parse_array()
    {
        array a;
        skipspace();
        if (c == ']') return a;
        backward();
        while (true) {
            a.emplace_back(new value(parse_value()));
            skipspace();
            if (c == ']') break;
            if (c != ',') throw invalid_array;
        }
        return a;
    }
    // TODO: Handles UTF-8
    std::string parse_string()
    {
        std::string s;
        while (true) {
            nextchar();
            if (c == '\\') {
                nextchar();
                switch (c) {
                    case '\"': case '\\': case '/': case '\b':
                    case '\f': case '\n': case '\r': case '\t':
                        nextchar();
                        break;
                    default:
                        throw invalid_escape_character;
                }
            }
            if (c == '\"') break;
            s.push_back(c);
        }
        return s;
    }
    void nextsave(std::string& s)
    {
        s.push_back(nextchar());
    }
    // We just do the number format check,
    // then call stod() to convert.
    number parse_number()
    {
        std::string s;
        if (!isnumber(c) && c != '-') {
            throw invalid_value_type;
        }
        s.push_back(c);
        if (c == '-') nextsave(s);
        if (c == '0' && !eof()) {
            nextsave(s);
            if (isnumber(c)) throw invalid_number;
        } else if (isnumber(c)) {
            while (!eof() && isnumber(c)) {
                nextsave(s);
            }
        }
        if (c == '.') {
            nextsave(s);
            if (!isnumber(c)) throw invalid_number;
            while (!eof() && isnumber(c)) {
                nextsave(s);
            }
        }
        if (c == 'e' || c == 'E') {
            nextsave(s);
            if (c == '+' || c == '-') nextsave(s);
            if (!isnumber(c)) throw invalid_number;
            while (!eof() && isnumber(c)) {
                nextsave(s);
            }
        }
        if (!isnumber(c)) backward();
        try {
            return std::stod(s);
        } catch (std::out_of_range& e) {
            throw number_out_of_range;
        } catch (std::invalid_argument& e) {
            throw invalid_number;
        }
    }
    value parse_constant()
    {
        const char *s, *p;
        switch (c) {
        case 't': s = "true"; break;
        case 'f': s = "false"; break;
        case 'n': s = "null"; break;
        }
        for (p = s + 1; *p != '\0'; p++) {
            if (*p != nextchar()) {
                throw invalid_constant;
            }
        }
        switch (*s) {
        case 't': return value(true);
        case 'f': return value(false);
        case 'n': return value();
        default: assert(0);
        }
    }

    int nextchar() { return charstream->nextchar(); }
    int skipspace() { return charstream->skipspace(); }
    void backward() { charstream->backward(); }
    bool eof() { return charstream->eof(); }

    struct string_stream : char_stream {
        string_stream(class parser *psr, const std::string& s)
            : char_stream(psr), p(s.data()), end(s.data() + s.size()) {  }
        int nextchar() override
        {
            if (p == end) throw incomplete;
            return parser->c = *p++;
        }
        void backward() override { p--; }
        bool eof() override { return p == end; }
        const char *p, *end;
    };

    struct file_stream : char_stream {
        file_stream(class parser *psr, const std::string& filename)
            : char_stream(psr), ifs(filename) {  }
        int nextchar() override
        {
            if (ifs.eof()) throw incomplete;
            return parser->c = ifs.get();
        }
        void backward() override { ifs.unget(); }
        bool eof() override { return ifs.eof(); }
        std::ifstream ifs;
    };

    int c; // cur valid character
    std::unique_ptr<char_stream> charstream;
    error_code errcode;
};

const char *parser::get_error_string(error_code code)
{
    static constexpr const char *error_code_map[] = {
        "invalid_value_type",
        "invalid_object",
        "invalid_array",
        "invalid_escape_character",
        "invalid_number",
        "number_out_of_range",
        "invalid_constant",
        "incomplete",
        "extra",
    };
    return error_code_map[code];
}

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
