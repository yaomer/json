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
    return operator=(string(val));
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
    invalid_escape,
    invalid_unicode,
    invalid_unicode_surrogate,
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
    void parse_escape(string& s)
    {
        switch (nextchar()) {
            case '"': s.push_back('\"'); break;
            case '\\': s.push_back('\\'); break;
            case '/': s.push_back('/'); break;
            case 'b': s.push_back('\b'); break;
            case 'f': s.push_back('\f'); break;
            case 'n': s.push_back('\n'); break;
            case 'r': s.push_back('\r'); break;
            case 't': s.push_back('\t'); break;
            case 'u': encode_unicode(s); break;
            default: throw invalid_escape;
        }
    }
    string parse_string()
    {
        string s;
        while (true) {
            switch (nextchar()) {
            case '\\': parse_escape(s); break;
            case '\"': return s;
            default: s.push_back(c); break;
            }
        }
    }
    void nextsave(std::string& s)
    {
        s.push_back(nextchar());
    }
    void skipnumber(std::string& s)
    {
        while (!eof() && isnumber(c)) {
            nextsave(s);
        }
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
            skipnumber(s);
        }
        if (c == '.') {
            nextsave(s);
            if (!isnumber(c)) throw invalid_number;
            skipnumber(s);
        }
        if (c == 'e' || c == 'E') {
            nextsave(s);
            if (c == '+' || c == '-') nextsave(s);
            if (!isnumber(c)) throw invalid_number;
            skipnumber(s);
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

    unsigned fromhex()
    {
        nextchar();
        if (isnumber(c)) return (c - '0');
        else if (ishexnumber(c)) return (toupper(c) - 'A') + 10;
        else throw invalid_unicode;
    }
    unsigned parse_hex4()
    {
        return (fromhex() << 12) | (fromhex() << 8) | (fromhex() << 4) | (fromhex());
    }
    // Only support UTF-8
    // +----------------------------------------------------------------+
    // |       range        |  byte-1  |  byte-2  |  byte-3  |  byte-4  |
    // +----------------------------------------------------------------|
    // |  U+0000 ~ U+007F   | 0xxxxxxx |          |          |          |
    // |----------------------------------------------------------------|
    // |  U+0080 ~ U+07FF   | 110xxxxx | 10xxxxxx |          |          |
    // |----------------------------------------------------------------|
    // |  U+0800 ~ U+FFFF   | 1110xxxx | 10xxxxxx | 10xxxxxx |          |
    // |----------------------------------------------------------------|
    // | U+10000 ~ U+10FFFF | 11110xxx | 10xxxxxx | 10xxxxxx | 10xxxxxx |
    // +----------------------------------------------------------------+
    void encode_unicode(std::string& s)
    {
        unsigned u = parse_hex4();
        if (u >= 0xD800 && u <= 0xDBFF) { // SURROGATE-PAIR
            if (nextchar() != '\\') throw invalid_unicode_surrogate;
            if (nextchar() != 'u') throw invalid_unicode_surrogate;
            unsigned u2 = parse_hex4();
            if (u2 < 0xDC00 || u2 > 0xDFFF) throw invalid_unicode_surrogate;
            u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
        }
        if (u <= 0x7F) {
            s.push_back(u & 0xFF);
        } else if (u >= 0x80 && u <= 0x7FF) {
            s.push_back(0xC0 | ((u >> 6) & 0xFF));
            s.push_back(0x80 | ( u       & 0x3F));
        } else if (u >= 0x800 && u <= 0xFFFF) {
            s.push_back(0xE0 | ((u >> 12) & 0xFF));
            s.push_back(0x80 | ((u >> 6)  & 0x3F));
            s.push_back(0x80 | ( u        & 0x3F));
        } else {
            assert(u <= 0x10FFFF);
            s.push_back(0xF0 | ((u >> 18) & 0xFF));
            s.push_back(0x80 | ((u >> 12) & 0x3F));
            s.push_back(0x80 | ((u >> 6)  & 0x3F));
            s.push_back(0x80 | ( u        & 0x3F));
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
        "invalid_escape",
        "invalid_unicode",
        "invalid_unicode_surrogate",
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
        buf.push_back('\"');
        decode_unicode(s);
        buf.push_back('\"');
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

    void put_unicode(int c1, int c2, int c3, int c4)
    {
        static const char *tohex = "0123456789ABCDEF";
        buf.push_back('\\');
        buf.push_back('u');
        buf.push_back(tohex[c1]);
        buf.push_back(tohex[c2]);
        buf.push_back(tohex[c3]);
        buf.push_back(tohex[c4]);
    }
    void decode_unicode(const string& s)
    {
        for (size_t i = 0; i < s.size(); i++) {
            char c = s[i];
            if (!(c & 0x80)) {
                // 1 bit is treated as an ASCII character,
                // in which case UTF-8 is compatible with ASCII.
                buf.push_back(c);
            } else if (!(c & 0x20)) {
                if (i + 1 >= s.size()) throw invalid_unicode;
                char c2 = s[++i];
                // xxx|xx xx|xxxx
                put_unicode(0,
                            (c & 0x1C) >> 2,
                            ((c & 0x03) << 2) | ((c2 & 0x30) >> 4),
                            (c2 & 0x0F));
            } else if (!(c & 0x10)) {
                if (i + 2 >= s.size()) throw invalid_unicode;
                char c2 = s[++i], c3 = s[++i];
                // xxxx |xxxx|xx xx|xxxx
                put_unicode((c & 0x0F),
                            (c2 & 0x3C) >> 2,
                            ((c2 & 0x03) << 2) | ((c3 & 0x30) >> 4),
                            (c3 & 0x0F));
            } else { // SURROGATE-PAIR
                assert(!(c & 0x08));
                if (i + 3 >= s.size()) throw invalid_unicode;
                char c2 = s[++i], c3 = s[++i], c4 = s[++i];
                unsigned u = 0;
                u = (u | (c  & 0x07)) << 6;
                u = (u | (c2 & 0x3F)) << 6;
                u = (u | (c3 & 0x3F)) << 6;
                u = (u | (c4 & 0x3F));
                u -= 0x10000;
                unsigned u1 = (u >> 10) + 0xD800;
                unsigned u2 = (u & 0x3FF) + 0xDC00;
                put_unicode((u1 >> 12) & 0x0F, (u1 >> 8) & 0x0F, (u1 >> 4) & 0x0F, (u1 & 0x0F));
                put_unicode((u2 >> 12) & 0x0F, (u2 >> 8) & 0x0F, (u2 >> 4) & 0x0F, (u2 & 0x0F));
            }
        }
    }

    std::string buf;
    bool visual;
    int indent_spaces; // A few spaces to indent
    int cur_level; // Current indent level
};

}

#endif // _JSON_H
