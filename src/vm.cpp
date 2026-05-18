#include "vm.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "serializer.h"
#include <iostream>
#include <cstdarg>
#include <chrono>
#include <sstream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <unordered_set>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define close_socket(s) closesocket(s)
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    typedef int socket_t;
    #define close_socket(s) close(s)
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif


thread_local VM* VM::currentVM = nullptr;
#define allocateObject VM::currentVM->allocateObject

Value clockNative(int argCount, Value* args) {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return Value((double)std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0);
}

Value arrayLengthNative(int argCount, Value* args) {
    if (argCount < 1 || !args[0].isArray()) {
        return Value(0.0);
    }
    return Value((double)args[0].asArray()->elements.size());
}

Value arrayPushNative(int argCount, Value* args) {
    if (argCount < 2 || !args[0].isArray()) {
        return Value(0.0);
    }
    args[0].asArray()->elements.push_back(args[1]);
    return Value((double)args[0].asArray()->elements.size());
}

Value arrayPopNative(int argCount, Value* args) {
    if (argCount < 1 || !args[0].isArray()) {
        return Value(0.0);
    }
    auto& elements = args[0].asArray()->elements;
    if (elements.empty()) {
        return Value(0.0);
    }
    Value popped = elements.back();
    elements.pop_back();
    return popped;
}

Value arrayClearNative(int argCount, Value* args) {
    if (argCount < 1 || !args[0].isArray()) {
        return Value(0.0);
    }
    args[0].asArray()->elements.clear();
    return Value(0.0);
}

Value stringLengthNative(int argCount, Value* args) {
    if (argCount < 1 || !args[0].isString()) {
        return Value(0.0);
    }
    return Value((double)args[0].asString().length());
}

Value arraySliceNative(int argCount, Value* args) {
    if (argCount < 1 || !args[0].isArray()) {
        return Value(0.0);
    }
    auto& elements = args[0].asArray()->elements;
    int length = (int)elements.size();
    
    int start = 0;
    if (argCount >= 2 && args[1].isNumber()) {
        start = (int)args[1].asNumber();
    }
    if (start < 0) {
        start = length + start;
    }
    if (start < 0) start = 0;
    if (start > length) start = length;
    
    int end = length;
    if (argCount >= 3 && args[2].isNumber()) {
        end = (int)args[2].asNumber();
    }
    if (end < 0) {
        end = length + end;
    }
    if (end < 0) end = 0;
    if (end > length) end = length;
    
    ObjArray* sliceArray = allocateObject<ObjArray>();
    if (start < end) {
        for (int i = start; i < end; i++) {
            sliceArray->elements.push_back(elements[i]);
        }
    }
    return Value(sliceArray);
}

Value arrayIndexOfNative(int argCount, Value* args) {
    if (argCount < 2 || !args[0].isArray()) {
        return Value(-1.0);
    }
    auto& elements = args[0].asArray()->elements;
    Value searchVal = args[1];
    for (size_t i = 0; i < elements.size(); i++) {
        if (elements[i] == searchVal) {
            return Value((double)i);
        }
    }
    return Value(-1.0);
}

Value arrayJoinNative(int argCount, Value* args) {
    if (argCount < 1 || !args[0].isArray()) {
        return Value("");
    }
    auto& elements = args[0].asArray()->elements;
    std::string delim = ",";
    if (argCount >= 2 && args[1].isString()) {
        delim = args[1].asString();
    }
    std::stringstream ss;
    for (size_t i = 0; i < elements.size(); i++) {
        ss << elements[i];
        if (i < elements.size() - 1) {
            ss << delim;
        }
    }
    return Value(ss.str());
}

Value stringSplitNative(int argCount, Value* args) {
    if (argCount < 1 || !args[0].isString()) {
        return Value(0.0);
    }
    std::string str = args[0].asString();
    std::string delim = "";
    if (argCount >= 2 && args[1].isString()) {
        delim = args[1].asString();
    }
    ObjArray* splitArray = allocateObject<ObjArray>();
    if (delim.empty()) {
        for (char c : str) {
            splitArray->elements.push_back(Value(std::string(1, c)));
        }
    } else {
        size_t start = 0;
        size_t end = str.find(delim);
        while (end != std::string::npos) {
            splitArray->elements.push_back(Value(str.substr(start, end - start)));
            start = end + delim.length();
            end = str.find(delim, start);
        }
        splitArray->elements.push_back(Value(str.substr(start)));
    }
    return Value(splitArray);
}

Value stringSubstringNative(int argCount, Value* args) {
    if (argCount < 1 || !args[0].isString()) {
        return Value("");
    }
    std::string str = args[0].asString();
    int length = (int)str.length();
    
    int start = 0;
    if (argCount >= 2 && args[1].isNumber()) {
        start = (int)args[1].asNumber();
    }
    if (start < 0) {
        start = length + start;
    }
    if (start < 0) start = 0;
    if (start > length) start = length;
    
    int end = length;
    if (argCount >= 3 && args[2].isNumber()) {
        end = (int)args[2].asNumber();
    }
    if (end < 0) {
        end = length + end;
    }
    if (end < 0) end = 0;
    if (end > length) end = length;
    
    if (start >= end) {
        return Value("");
    }
    return Value(str.substr(start, end - start));
}

Value stringFindNative(int argCount, Value* args) {
    if (argCount < 2 || !args[0].isString() || !args[1].isString()) {
        return Value(-1.0);
    }
    std::string str = args[0].asString();
    std::string sub = args[1].asString();
    size_t index = str.find(sub);
    if (index == std::string::npos) {
        return Value(-1.0);
    }
    return Value((double)index);
}

Value stringContainsNative(int argCount, Value* args) {
    if (argCount < 2 || !args[0].isString() || !args[1].isString()) {
        return Value(false);
    }
    std::string str = args[0].asString();
    std::string sub = args[1].asString();
    return Value(str.find(sub) != std::string::npos);
}

Value mapLengthNative(int argCount, Value* args) {
    if (argCount < 1 || !args[0].isMap()) {
        return Value(0.0);
    }
    return Value((double)args[0].asMap()->entries.size());
}

Value mapHasNative(int argCount, Value* args) {
    if (argCount < 2 || !args[0].isMap()) {
        return Value(false);
    }
    ObjMap* map = args[0].asMap();
    Value key = args[1];
    std::string keyStr;
    if (key.isString()) {
        keyStr = key.asString();
    } else {
        std::stringstream ss;
        ss << key;
        keyStr = ss.str();
    }
    return Value(map->entries.find(keyStr) != map->entries.end());
}

Value mapRemoveNative(int argCount, Value* args) {
    if (argCount < 2 || !args[0].isMap()) {
        return Value(false);
    }
    ObjMap* map = args[0].asMap();
    Value key = args[1];
    std::string keyStr;
    if (key.isString()) {
        keyStr = key.asString();
    } else {
        std::stringstream ss;
        ss << key;
        keyStr = ss.str();
    }
    auto it = map->entries.find(keyStr);
    if (it != map->entries.end()) {
        map->entries.erase(it);
        return Value(true);
    }
    return Value(false);
}

Value mapKeysNative(int argCount, Value* args) {
    if (argCount < 1 || !args[0].isMap()) {
        return Value(allocateObject<ObjArray>());
    }
    ObjMap* map = args[0].asMap();
    ObjArray* array = allocateObject<ObjArray>();
    for (const auto& entry : map->entries) {
        array->elements.push_back(entry.second.first);
    }
    return Value(array);
}

Value mapValuesNative(int argCount, Value* args) {
    if (argCount < 1 || !args[0].isMap()) {
        return Value(allocateObject<ObjArray>());
    }
    ObjMap* map = args[0].asMap();
    ObjArray* array = allocateObject<ObjArray>();
    for (const auto& entry : map->entries) {
        array->elements.push_back(entry.second.second);
    }
    return Value(array);
}

Value mapClearNative(int argCount, Value* args) {
    if (argCount < 1 || !args[0].isMap()) {
        return Value(0.0);
    }
    args[0].asMap()->entries.clear();
    return Value(0.0);
}

Value mathSqrtNative(int argCount, Value* args) {
    if (argCount < 1 || !args[0].isNumber()) {
        return Value(0.0);
    }
    return Value(std::sqrt(args[0].asNumber()));
}

Value mathAbsNative(int argCount, Value* args) {
    if (argCount < 1 || !args[0].isNumber()) {
        return Value(0.0);
    }
    return Value(std::abs(args[0].asNumber()));
}

Value mathPowNative(int argCount, Value* args) {
    if (argCount < 2 || !args[0].isNumber() || !args[1].isNumber()) {
        return Value(0.0);
    }
    return Value(std::pow(args[0].asNumber(), args[1].asNumber()));
}

Value mathRandomNative(int argCount, Value* args) {
    double r = (double)std::rand() / ((double)RAND_MAX + 1.0);
    return Value(r);
}

Value fileReadNative(int argCount, Value* args) {
    if (argCount < 1 || !args[0].isString()) {
        return Value("");
    }
    std::string path = args[0].asString();
    std::ifstream file(path);
    if (!file.is_open()) {
        return Value("");
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return Value(ss.str());
}

Value fileWriteNative(int argCount, Value* args) {
    if (argCount < 2 || !args[0].isString() || !args[1].isString()) {
        return Value(false);
    }
    std::string path = args[0].asString();
    std::string content = args[1].asString();
    std::ofstream file(path);
    if (!file.is_open()) return Value(false);
    file << content;
    return Value(true);
}

// ── Type & conversion natives ─────────────────────────────────────────────────
Value typeofNative(int argCount, Value* args) {
    if (argCount < 1) return Value(std::string("nil"));
    Value v = args[0];
    if (v.isNumber())   return Value(std::string("number"));
    if (v.isBool())     return Value(std::string("boolean"));
    if (v.isString())   return Value(std::string("string"));
    if (v.isArray())    return Value(std::string("array"));
    if (v.isMap())      return Value(std::string("map"));
    if (v.isClosure() || v.isNative() || v.isFunction()) return Value(std::string("function"));
    if (v.isClass())    return Value(std::string("class"));
    if (v.isInstance()) return Value(std::string("instance"));
    return Value(std::string("unknown"));
}

Value strNative(int argCount, Value* args) {
    if (argCount < 1) return Value(std::string(""));
    std::ostringstream ss;
    ss << args[0];
    return Value(ss.str());
}

Value numNative(int argCount, Value* args) {
    if (argCount < 1) return Value(0.0);
    if (args[0].isNumber()) return args[0];
    if (args[0].isString()) {
        try { return Value(std::stod(args[0].asString())); }
        catch (...) { return Value(0.0); }
    }
    return Value(0.0);
}

Value floorNative(int argc, Value* args) {
    if (argc < 1 || !args[0].isNumber()) return Value(0.0);
    return Value(std::floor(args[0].asNumber()));
}
Value ceilNative(int argc, Value* args) {
    if (argc < 1 || !args[0].isNumber()) return Value(0.0);
    return Value(std::ceil(args[0].asNumber()));
}
Value roundNative(int argc, Value* args) {
    if (argc < 1 || !args[0].isNumber()) return Value(0.0);
    return Value(std::round(args[0].asNumber()));
}
Value maxNative(int argc, Value* args) {
    if (argc < 2 || !args[0].isNumber() || !args[1].isNumber()) return Value(0.0);
    return Value(args[0].asNumber() >= args[1].asNumber() ? args[0].asNumber() : args[1].asNumber());
}
Value minNative(int argc, Value* args) {
    if (argc < 2 || !args[0].isNumber() || !args[1].isNumber()) return Value(0.0);
    return Value(args[0].asNumber() <= args[1].asNumber() ? args[0].asNumber() : args[1].asNumber());
}

// ── String method natives (bound) ─────────────────────────────────────────────
Value stringUpperNative(int argc, Value* args) {
    if (argc < 1 || !args[0].isString()) return Value(std::string(""));
    std::string s = args[0].asString();
    for (char& c : s) c = (char)toupper(c);
    return Value(s);
}
Value stringLowerNative(int argc, Value* args) {
    if (argc < 1 || !args[0].isString()) return Value(std::string(""));
    std::string s = args[0].asString();
    for (char& c : s) c = (char)tolower(c);
    return Value(s);
}
Value stringTrimNative(int argc, Value* args) {
    if (argc < 1 || !args[0].isString()) return Value(std::string(""));
    std::string s = args[0].asString();
    size_t b = s.find_first_not_of(" \t\r\n");
    size_t e = s.find_last_not_of(" \t\r\n");
    return Value(b == std::string::npos ? std::string("") : s.substr(b, e - b + 1));
}
Value stringReplaceNative(int argc, Value* args) {
    if (argc < 3 || !args[0].isString() || !args[1].isString() || !args[2].isString()) return Value(std::string(""));
    std::string s = args[0].asString(), from = args[1].asString(), to = args[2].asString();
    if (from.empty()) return Value(s);
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) { s.replace(pos, from.size(), to); pos += to.size(); }
    return Value(s);
}
Value stringStartsWithNative(int argc, Value* args) {
    if (argc < 2 || !args[0].isString() || !args[1].isString()) return Value(false);
    std::string s = args[0].asString(), p = args[1].asString();
    return Value(s.size() >= p.size() && s.substr(0, p.size()) == p);
}
Value stringEndsWithNative(int argc, Value* args) {
    if (argc < 2 || !args[0].isString() || !args[1].isString()) return Value(false);
    std::string s = args[0].asString(), p = args[1].asString();
    return Value(s.size() >= p.size() && s.substr(s.size() - p.size()) == p);
}
Value stringRepeatNative(int argc, Value* args) {
    if (argc < 2 || !args[0].isString() || !args[1].isNumber()) return Value(std::string(""));
    std::string s = args[0].asString();
    int n = (int)args[1].asNumber();
    std::string result;
    for (int i = 0; i < n; i++) result += s;
    return Value(result);
}

// ── JSON implementation ───────────────────────────────────────────────────────
class JsonParser {
public:
    JsonParser(const std::string& source) : source(source), index(0) {}

    Value parse() {
        skipWhitespace();
        if (index >= source.length()) {
            VM::currentVM->runtimeError("Empty JSON string.");
            return Value(0.0);
        }
        Value val = parseValue();
        skipWhitespace();
        if (index < source.length()) {
            VM::currentVM->runtimeError("Trailing characters after JSON value.");
        }
        return val;
    }

private:
    std::string source;
    size_t index;

    void skipWhitespace() {
        while (index < source.length() && (source[index] == ' ' || source[index] == '\t' || source[index] == '\r' || source[index] == '\n')) {
            index++;
        }
    }

    char peek() {
        if (index >= source.length()) return '\0';
        return source[index];
    }

    char advance() {
        if (index >= source.length()) return '\0';
        return source[index++];
    }

    Value parseValue() {
        skipWhitespace();
        char c = peek();
        if (c == '{') {
            return parseObject();
        } else if (c == '[') {
            return parseArray();
        } else if (c == '"' || c == '\'') {
            return parseString();
        } else if (c == 't' || c == 'f') {
            return parseBool();
        } else if (c == 'n') {
            return parseNull();
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            return parseNumber();
        }
        VM::currentVM->runtimeError("Invalid JSON value.");
        return Value(0.0);
    }

    Value parseObject() {
        advance(); // '{'
        ObjMap* map = VM::currentVM->allocateObject<ObjMap>();
        skipWhitespace();
        if (peek() == '}') {
            advance(); // '}'
            return Value(map);
        }

        while (true) {
            skipWhitespace();
            char c = peek();
            if (c != '"' && c != '\'') {
                VM::currentVM->runtimeError("Expected string key in JSON object, got '%c' (code %d).", c, (int)c);
                return Value(0.0);
            }
            Value keyVal = parseString();
            std::string key = keyVal.asString();
            skipWhitespace();
            if (advance() != ':') {
                VM::currentVM->runtimeError("Expected ':' after key in JSON object.");
                return Value(0.0);
            }
            Value val = parseValue();
            map->entries[key] = std::make_pair(keyVal, val);
            skipWhitespace();
            char next = advance();
            if (next == '}') break;
            if (next != ',') {
                VM::currentVM->runtimeError("Expected ',' or '}' in JSON object.");
                return Value(0.0);
            }
        }
        return Value(map);
    }

    Value parseArray() {
        advance(); // '['
        ObjArray* array = VM::currentVM->allocateObject<ObjArray>();
        skipWhitespace();
        if (peek() == ']') {
            advance(); // ']'
            return Value(array);
        }

        while (true) {
            Value val = parseValue();
            array->elements.push_back(val);
            skipWhitespace();
            char c = advance();
            if (c == ']') break;
            if (c != ',') {
                VM::currentVM->runtimeError("Expected ',' or ']' in JSON array.");
                return Value(0.0);
            }
        }
        return Value(array);
    }

    Value parseString() {
        char quote = advance(); // '"' or '\''
        std::string str = "";
        while (index < source.length() && source[index] != quote) {
            char c = advance();
            if (c == '\\') {
                if (index >= source.length()) {
                    VM::currentVM->runtimeError("Unterminated escape sequence in JSON string.");
                    return Value(0.0);
                }
                char next = advance();
                if (next == quote) str += quote;
                else if (next == '\\') str += '\\';
                else if (next == '/') str += '/';
                else if (next == 'b') str += '\b';
                else if (next == 'f') str += '\f';
                else if (next == 'n') str += '\n';
                else if (next == 'r') str += '\r';
                else if (next == 't') str += '\t';
                else if (next == 'u') {
                    if (index + 4 <= source.length()) {
                        index += 4;
                        str += '?';
                    }
                } else {
                    str += next;
                }
            } else {
                str += c;
            }
        }
        if (advance() != quote) {
            VM::currentVM->runtimeError("Unterminated JSON string.");
        }
        return Value(str);
    }

    Value parseBool() {
        if (source.substr(index, 4) == "true") {
            index += 4;
            return Value(true);
        } else if (source.substr(index, 5) == "false") {
            index += 5;
            return Value(false);
        }
        VM::currentVM->runtimeError("Invalid boolean literal in JSON.");
        return Value(false);
    }

    Value parseNull() {
        if (source.substr(index, 4) == "null") {
            index += 4;
            return Value(0.0);
        }
        VM::currentVM->runtimeError("Invalid null literal in JSON.");
        return Value(0.0);
    }

    Value parseNumber() {
        size_t start = index;
        if (peek() == '-') advance();
        while (peek() >= '0' && peek() <= '9') advance();
        if (peek() == '.') {
            advance();
            while (peek() >= '0' && peek() <= '9') advance();
        }
        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') advance();
            while (peek() >= '0' && peek() <= '9') advance();
        }
        std::string numStr = source.substr(start, index - start);
        try {
            double d = std::stod(numStr);
            return Value(d);
        } catch (...) {
            VM::currentVM->runtimeError("Invalid number literal in JSON.");
            return Value(0.0);
        }
    }
};

class JsonSerializer {
public:
    JsonSerializer() = default;

    std::string serialize(Value val) {
        std::unordered_set<void*> visited;
        return serializeInternal(val, visited);
    }

private:
    std::string serializeInternal(Value val, std::unordered_set<void*>& visited) {
        if (val.isNumber()) {
            double d = val.asNumber();
            if (d == std::floor(d)) {
                return std::to_string((long long)d);
            }
            std::string s = std::to_string(d);
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (s.back() == '.') {
                s.pop_back();
            }
            return s;
        } else if (val.isBool()) {
            return val.asBool() ? "true" : "false";
        } else if (val.isString()) {
            return escapeString(val.asString());
        } else if (val.isArray()) {
            ObjArray* array = val.asArray();
            if (visited.count(array)) {
                VM::currentVM->throwException(Value("Circular reference in JSON serialization."));
                return "null";
            }
            visited.insert(array);
            std::string s = "[";
            for (size_t i = 0; i < array->elements.size(); i++) {
                s += serializeInternal(array->elements[i], visited);
                if (i < array->elements.size() - 1) s += ",";
            }
            s += "]";
            visited.erase(array);
            return s;
        } else if (val.isMap()) {
            ObjMap* map = val.asMap();
            if (visited.count(map)) {
                VM::currentVM->throwException(Value("Circular reference in JSON serialization."));
                return "null";
            }
            visited.insert(map);
            std::string s = "{";
            size_t count = 0;
            for (const auto& entry : map->entries) {
                s += escapeString(entry.first) + ":" + serializeInternal(entry.second.second, visited);
                if (count < map->entries.size() - 1) s += ",";
                count++;
            }
            s += "}";
            visited.erase(map);
            return s;
        }
        return "null";
    }

    std::string escapeString(const std::string& str) {
        std::string s = "\"";
        for (char c : str) {
            if (c == '"') s += "\\\"";
            else if (c == '\\') s += "\\\\";
            else if (c == '\n') s += "\\n";
            else if (c == '\t') s += "\\t";
            else if (c == '\r') s += "\\r";
            else if (c == '\b') s += "\\b";
            else if (c == '\f') s += "\\f";
            else s += c;
        }
        s += "\"";
        return s;
    }
};

Value jsonParseNative(int argc, Value* args) {
    if (argc < 1 || !args[0].isString()) {
        VM::currentVM->runtimeError("json_parse expects 1 string argument.");
        return Value(0.0);
    }
    JsonParser parser(args[0].asString());
    return parser.parse();
}

Value jsonStringifyNative(int argc, Value* args) {
    if (argc < 1) {
        VM::currentVM->runtimeError("json_stringify expects 1 argument.");
        return Value("");
    }
    JsonSerializer serializer;
    return Value(serializer.serialize(args[0]));
}

// ── Networking Sockets Native API ────────────────────────────────────────────
Value socketCreateNative(int argc, Value* args) {
    socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return Value(-1.0);
    }
    return Value((double)sock);
}

Value socketBindNative(int argc, Value* args) {
    if (argc < 3 || !args[0].isNumber() || !args[1].isString() || !args[2].isNumber()) {
        VM::currentVM->runtimeError("socket_bind expects arguments: socketHandle (number), ipAddress (string), port (number).");
        return Value(false);
    }
    socket_t sock = (socket_t)args[0].asNumber();
    std::string ip = args[1].asString();
    int port = (int)args[2].asNumber();

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());

    int result = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    return Value(result != SOCKET_ERROR);
}

Value socketListenNative(int argc, Value* args) {
    if (argc < 2 || !args[0].isNumber() || !args[1].isNumber()) {
        VM::currentVM->runtimeError("socket_listen expects arguments: socketHandle (number), backlog (number).");
        return Value(false);
    }
    socket_t sock = (socket_t)args[0].asNumber();
    int backlog = (int)args[1].asNumber();

    int result = listen(sock, backlog);
    return Value(result != SOCKET_ERROR);
}

Value socketAcceptNative(int argc, Value* args) {
    if (argc < 1 || !args[0].isNumber()) {
        VM::currentVM->runtimeError("socket_accept expects 1 argument: socketHandle (number).");
        return Value(-1.0);
    }
    socket_t sock = (socket_t)args[0].asNumber();
    socket_t clientSock = accept(sock, nullptr, nullptr);
    if (clientSock == INVALID_SOCKET) {
        return Value(-1.0);
    }
    return Value((double)clientSock);
}

Value socketConnectNative(int argc, Value* args) {
    if (argc < 3 || !args[0].isNumber() || !args[1].isString() || !args[2].isNumber()) {
        VM::currentVM->runtimeError("socket_connect expects arguments: socketHandle (number), ipAddress (string), port (number).");
        return Value(false);
    }
    socket_t sock = (socket_t)args[0].asNumber();
    std::string ip = args[1].asString();
    int port = (int)args[2].asNumber();

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());

    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    return Value(result != SOCKET_ERROR);
}

Value socketSendNative(int argc, Value* args) {
    if (argc < 2 || !args[0].isNumber() || !args[1].isString()) {
        VM::currentVM->runtimeError("socket_send expects arguments: socketHandle (number), dataString (string).");
        return Value(-1.0);
    }
    socket_t sock = (socket_t)args[0].asNumber();
    std::string data = args[1].asString();

    int result = send(sock, data.c_str(), (int)data.length(), 0);
    return Value((double)result);
}

Value socketRecvNative(int argc, Value* args) {
    if (argc < 2 || !args[0].isNumber() || !args[1].isNumber()) {
        VM::currentVM->runtimeError("socket_recv expects arguments: socketHandle (number), bufferSize (number).");
        return Value("");
    }
    socket_t sock = (socket_t)args[0].asNumber();
    int bufferSize = (int)args[1].asNumber();
    if (bufferSize <= 0) bufferSize = 1024;

    std::vector<char> buffer(bufferSize);
    int bytesReceived = recv(sock, buffer.data(), bufferSize, 0);
    if (bytesReceived <= 0) {
        return Value("");
    }
    return Value(std::string(buffer.data(), bytesReceived));
}

Value socketCloseNative(int argc, Value* args) {
    if (argc < 1 || !args[0].isNumber()) {
        VM::currentVM->runtimeError("socket_close expects 1 argument: socketHandle (number).");
        return Value(false);
    }
    socket_t sock = (socket_t)args[0].asNumber();
    close_socket(sock);
    return Value(true);
}

Value exitNative(int argc, Value* args) {
    int code = 0;
    if (argc >= 1 && args[0].isNumber()) {
        code = (int)args[0].asNumber();
    }
    std::exit(code);
    return Value(0.0);
}

VM::VM() : objects(nullptr), bytesAllocated(0), nextGC(1024 * 1024), openUpvalues(nullptr), hasException(false) {
    currentVM = this;
    std::srand((unsigned int)std::time(nullptr));

#ifdef _WIN32
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        std::cerr << "[CVM++] WSAStartup failed: " << res << std::endl;
    }
#endif

    defineNative("clock",      clockNative);
    defineNative("sqrt",       mathSqrtNative);
    defineNative("abs",        mathAbsNative);
    defineNative("pow",        mathPowNative);
    defineNative("random",     mathRandomNative);
    defineNative("read_file",  fileReadNative);
    defineNative("write_file", fileWriteNative);
    defineNative("typeof",     typeofNative);
    defineNative("str",        strNative);
    defineNative("num",        numNative);
    defineNative("floor",      floorNative);
    defineNative("ceil",       ceilNative);
    defineNative("round",      roundNative);
    defineNative("max",        maxNative);
    defineNative("min",        minNative);
    defineNative("exit",       exitNative);

    defineNative("json_parse",      jsonParseNative);
    defineNative("json_stringify",  jsonStringifyNative);
    defineNative("socket_create",   socketCreateNative);
    defineNative("socket_bind",     socketBindNative);
    defineNative("socket_listen",   socketListenNative);
    defineNative("socket_accept",   socketAcceptNative);
    defineNative("socket_connect",  socketConnectNative);
    defineNative("socket_send",     socketSendNative);
    defineNative("socket_recv",     socketRecvNative);
    defineNative("socket_close",    socketCloseNative);
}

VM::~VM() {
    Obj* object = objects;
    while (object != nullptr) {
        Obj* nextObj = object->next;
        delete object;
        object = nextObj;
    }
}

void VM::throwException(Value value) {
    hasException = true;
    exceptionValue = value;
}

void VM::defineNative(const std::string& name, NativeFn function) {
    globals[name] = Value(function);
}

ObjUpvalue* VM::captureUpvalue(int stackIndex) {
    ObjUpvalue* prevUpvalue = nullptr;
    ObjUpvalue* upvalue = openUpvalues;

    while (upvalue != nullptr && upvalue->index > stackIndex) {
        prevUpvalue = upvalue;
        upvalue = upvalue->nextUpvalue;
    }

    if (upvalue != nullptr && upvalue->index == stackIndex) {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = allocateObject<ObjUpvalue>(stackIndex);
    createdUpvalue->nextUpvalue = upvalue;

    if (prevUpvalue == nullptr) {
        openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->nextUpvalue = createdUpvalue;
    }

    return createdUpvalue;
}

void VM::closeUpvalues(int lastSlotIndex) {
    while (openUpvalues != nullptr && openUpvalues->index >= lastSlotIndex) {
        ObjUpvalue* upvalue = openUpvalues;
        upvalue->closed = stack[upvalue->index];
        upvalue->index = -1;
        openUpvalues = upvalue->nextUpvalue;
    }
}

void VM::push(Value value) {
    stack.push_back(value);
}

Value VM::pop() {
    Value value = stack.back();
    stack.pop_back();
    return value;
}

Value VM::peek(int distance) {
    return stack[stack.size() - 1 - distance];
}

// ─────────────────────────────────────────────
// Module import resolution
// ─────────────────────────────────────────────

bool VM::importModule(const std::string& rawPath) {
    // Resolve path relative to the current script directory
    std::string resolved = rawPath;
    if (!currentDir.empty() && rawPath.find('/') == std::string::npos && rawPath.find('\\') == std::string::npos) {
        resolved = currentDir + rawPath;
    }

    // Try the exact path, then with .cvmc, then with .cvm suffix
    std::vector<std::string> candidates = { resolved };
    if (resolved.size() < 5 || (resolved.substr(resolved.size()-5) != ".cvmc" && resolved.substr(resolved.size()-4) != ".cvm")) {
        candidates.push_back(resolved + ".cvmc");
        candidates.push_back(resolved + ".cvm");
    }

    std::string foundPath;
    for (const auto& c : candidates) {
        std::ifstream test(c, std::ios::binary);
        if (test.is_open()) { foundPath = c; break; }
    }

    if (foundPath.empty()) {
        runtimeError("Module not found: '%s'", rawPath.c_str());
        return false;
    }

    // Guard against circular or duplicate imports
    if (importedModules.count(foundPath)) {
        push(Value(0.0));
        return true;
    }
    importedModules.insert(foundPath);

    // Allocate module function first and use its pre-allocated chunk
    auto fn = allocateObject<ObjFunction>();
    fn->name = foundPath;
    fn->arity = 0;
    fn->upvalueCount = 0;
    Chunk* moduleChunk = fn->chunk;

    bool isBytecode = (foundPath.size() >= 5 && foundPath.substr(foundPath.size()-5) == ".cvmc");
    if (isBytecode) {
        if (!Serializer::readFile(foundPath, *moduleChunk, *this)) {
            runtimeError("Failed to load bytecode module: '%s'", foundPath.c_str());
            return false;
        }
    } else {
        std::ifstream file(foundPath);
        std::stringstream buf; buf << file.rdbuf();
        std::string source = buf.str();

        Lexer lexer(source.c_str());
        Parser parser(lexer);
        auto stmts = parser.parse();
        if (parser.hasError()) {
            runtimeError("Parse error in module: '%s'", foundPath.c_str());
            return false;
        }
        Compiler compiler(*this);
        compiler.compile(stmts, *moduleChunk);
    }

    // Wrap module chunk in a function/closure and push a new call frame
    auto closure = allocateObject<ObjClosure>(fn);
    push(Value(closure));

    CallFrame frame;
    frame.closure = closure;
    frame.ip = moduleChunk->code.data();
    frame.slots = (int)stack.size() - 1;
    frames.push_back(frame);

    return true;
}

void VM::printStackTrace() {
    std::cerr << "Stack trace (most recent call first):" << std::endl;
    for (int i = (int)frames.size() - 1; i >= 0; i--) {
        const CallFrame& frame = frames[i];
        ObjFunction* function = frame.closure->function;
        size_t instruction = frame.ip - function->chunk->code.data();
        if (instruction > 0) instruction--;
        int line = function->chunk->lines[instruction];
        std::cerr << "  at " << (function->name.empty() ? "<script>" : function->name) 
                  << "() (line " << line << ")" << std::endl;
    }
}

void VM::runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    std::cerr << "\n";
    printStackTrace();
}

InterpretResult VM::interpret(Chunk* chunk) {
    currentVM = this;
    ObjFunction* script = allocateObject<ObjFunction>();
    *(script->chunk) = *chunk; // Copy to heap-allocated chunk owned by script
    script->name = "<script>";
    
    ObjClosure* closure = allocateObject<ObjClosure>(script);
    
    CallFrame frame;
    frame.closure = closure;
    frame.ip = chunk->code.data();
    frame.slots = 0;
    
    push(Value(closure)); // Push script closure onto stack
    frames.push_back(frame);
    
    openUpvalues = nullptr;
    
    return run();
}

InterpretResult VM::run() {
    CallFrame* frame = &frames.back();
    
    #define READ_BYTE() (*frame->ip++)
    #define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
    #define READ_CONSTANT() (frame->closure->function->chunk->constants.values[READ_BYTE()])
    #define READ_STRING() (frame->closure->function->chunk->strings[READ_BYTE()])
    #define BINARY_OP(op) \
        do { \
            if (!peek(0).isNumber() || !peek(1).isNumber()) { \
                runtimeError("Operands must be numbers."); \
                return INTERPRET_RUNTIME_ERROR; \
            } \
            double b = pop().asNumber(); \
            double a = pop().asNumber(); \
            push(Value(a op b)); \
        } while (false)

    for (;;) {
        uint8_t instruction = READ_BYTE();
        switch (instruction) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_TRUE:     push(Value(true)); break;
            case OP_FALSE:    push(Value(false)); break;
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(Value(a == b));
                break;
            }
            case OP_GREATER:  BINARY_OP(>); break;
            case OP_LESS:     BINARY_OP(<); break;
            
            case OP_ADD: {
                if (peek(0).isString() && peek(1).isString()) {
                    std::string b = pop().asString();
                    std::string a = pop().asString();
                    push(Value(a + b));
                } else if (peek(0).isNumber() && peek(1).isNumber()) {
                    double b = pop().asNumber();
                    double a = pop().asNumber();
                    push(Value(a + b));
                } else {
                    runtimeError("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE:   BINARY_OP(/); break;
            case OP_NEGATE: {
                if (!peek(0).isNumber()) {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(Value(-pop().asNumber()));
                break;
            }
            case OP_NOT: {
                push(Value(!pop().isTruthy()));
                break;
            }
            
            case OP_POP:
                pop();
                break;
                
            case OP_PRINT: {
                std::cout << pop() << std::endl;
                break;
            }
                
            case OP_DEFINE_GLOBAL: {
                std::string name = READ_STRING();
                globals[name] = peek(0);
                pop();
                break;
            }
                
            case OP_GET_GLOBAL: {
                std::string name = READ_STRING();
                if (globals.find(name) == globals.end()) {
                    runtimeError("Undefined variable '%s'.", name.c_str());
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(globals[name]);
                break;
            }
                
            case OP_SET_GLOBAL: {
                std::string name = READ_STRING();
                if (globals.find(name) == globals.end()) {
                    runtimeError("Undefined variable '%s'.", name.c_str());
                    return INTERPRET_RUNTIME_ERROR;
                }
                globals[name] = peek(0);
                break;
            }
            
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(stack[frame->slots + slot]);
                break;
            }
            
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                stack[frame->slots + slot] = peek(0);
                break;
            }

            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                ObjUpvalue* up = frame->closure->upvalues[slot];
                if (up->index != -1) {
                    push(stack[up->index]);
                } else {
                    push(up->closed);
                }
                break;
            }

            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                ObjUpvalue* up = frame->closure->upvalues[slot];
                if (up->index != -1) {
                    stack[up->index] = peek(0);
                } else {
                    up->closed = peek(0);
                }
                break;
            }

            case OP_CLOSURE: {
                ObjFunction* function = READ_CONSTANT().asFunction();
                ObjClosure* closure = allocateObject<ObjClosure>(function);
                push(Value(closure));
                
                for (int i = 0; i < function->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (isLocal) {
                        closure->upvalues.push_back(captureUpvalue(frame->slots + index));
                    } else {
                        closure->upvalues.push_back(frame->closure->upvalues[index]);
                    }
                }
                break;
            }

            case OP_CLOSE_UPVALUE: {
                closeUpvalues(stack.size() - 1);
                pop();
                break;
            }

            case OP_CLASS: {
                std::string name = READ_STRING();
                push(Value(allocateObject<ObjClass>(name)));
                break;
            }

            case OP_METHOD: {
                std::string name = READ_STRING();
                Value method = peek(0);
                ObjClass* klass = peek(1).asClass();
                klass->methods[name] = method;
                pop();
                break;
            }

            case OP_INHERIT: {
                Value superclass = peek(1);
                if (!superclass.isClass()) {
                    runtimeError("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ObjClass* subclass = peek(0).asClass();
                for (const auto& method : superclass.asClass()->methods) {
                    subclass->methods[method.first] = method.second;
                }
                pop(); // Pop the subclass (superclass remains on the stack as "super" local!)
                break;
            }

            case OP_GET_SUPER: {
                std::string name = READ_STRING();
                ObjClass* superclass = pop().asClass();
                ObjInstance* receiver = peek(0).asInstance();
                
                auto method = superclass->methods.find(name);
                if (method == superclass->methods.end()) {
                    runtimeError("Undefined property '%s'.", name.c_str());
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ObjBoundMethod* bound = allocateObject<ObjBoundMethod>(receiver, method->second.asClosure());
                pop(); // Pop receiver
                push(Value(bound));
                break;
            }

            case OP_GET_PROPERTY: {
                Value receiver = peek(0);
                std::string name = READ_STRING();
                
                if (receiver.isInstance()) {
                    ObjInstance* instance = receiver.asInstance();
                    auto field = instance->fields.find(name);
                    if (field != instance->fields.end()) {
                        pop(); // Pop the instance
                        push(field->second);
                        break;
                    }
                    
                    auto method = instance->klass->methods.find(name);
                    if (method != instance->klass->methods.end()) {
                        pop(); // Pop the instance
                        ObjBoundMethod* bound = allocateObject<ObjBoundMethod>(instance, method->second.asClosure());
                        push(Value(bound));
                        break;
                    }
                    
                    runtimeError("Undefined property '%s'.", name.c_str());
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (receiver.isArray()) {
                    if (name == "length") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, arrayLengthNative)));
                        break;
                    }
                    if (name == "push") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, arrayPushNative)));
                        break;
                    }
                    if (name == "pop") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, arrayPopNative)));
                        break;
                    }
                    if (name == "clear") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, arrayClearNative)));
                        break;
                    }
                    if (name == "slice") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, arraySliceNative)));
                        break;
                    }
                    if (name == "indexOf") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, arrayIndexOfNative)));
                        break;
                    }
                    if (name == "join") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, arrayJoinNative)));
                        break;
                    }
                    runtimeError("Undefined array property '%s'.", name.c_str());
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (receiver.isString()) {
                    if (name == "length") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, stringLengthNative)));
                        break;
                    }
                    if (name == "split") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, stringSplitNative)));
                        break;
                    }
                    if (name == "substring") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, stringSubstringNative)));
                        break;
                    }
                    if (name == "find") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, stringFindNative)));
                        break;
                    }
                    if (name == "contains") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, stringContainsNative)));
                        break;
                    }
                    if (name == "upper") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, stringUpperNative)));
                        break;
                    }
                    if (name == "lower") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, stringLowerNative)));
                        break;
                    }
                    if (name == "trim") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, stringTrimNative)));
                        break;
                    }
                    if (name == "replace") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, stringReplaceNative)));
                        break;
                    }
                    if (name == "startsWith") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, stringStartsWithNative)));
                        break;
                    }
                    if (name == "endsWith") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, stringEndsWithNative)));
                        break;
                    }
                    if (name == "repeat") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, stringRepeatNative)));
                        break;
                    }
                    runtimeError("Undefined string property '%s'.", name.c_str());
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (receiver.isMap()) {
                    if (name == "length" || name == "size") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, mapLengthNative)));
                        break;
                    }
                    if (name == "has") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, mapHasNative)));
                        break;
                    }
                    if (name == "remove") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, mapRemoveNative)));
                        break;
                    }
                    if (name == "keys") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, mapKeysNative)));
                        break;
                    }
                    if (name == "values") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, mapValuesNative)));
                        break;
                    }
                    if (name == "clear") {
                        pop();
                        push(Value(allocateObject<ObjBoundNativeMethod>(receiver, mapClearNative)));
                        break;
                    }
                    runtimeError("Undefined map property '%s'.", name.c_str());
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                runtimeError("Only instances, arrays, strings, and maps have properties.");
                return INTERPRET_RUNTIME_ERROR;
            }

            case OP_SET_PROPERTY: {
                if (!peek(1).isInstance()) {
                    runtimeError("Only instances have fields.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ObjInstance* instance = peek(1).asInstance();
                std::string name = READ_STRING();
                
                instance->fields[name] = peek(0);
                Value value = pop(); // Pop value
                pop(); // Pop instance
                push(value); // Push value back
                break;
            }
                
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
                
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (!peek(0).isTruthy()) frame->ip += offset;
                break;
            }
                
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            
            case OP_CALL: {
                uint8_t argCount = READ_BYTE();
                Value callee = peek(argCount);
                
                if (callee.isNative()) {
                    NativeFn native = callee.asNative();
                    Value result = native(argCount, &stack[stack.size() - argCount]);
                    stack.resize(stack.size() - argCount - 1);
                    push(result);
                    if (VM::currentVM->hasException) {
                        VM::currentVM->hasException = false;
                        Value exceptionVal = VM::currentVM->exceptionValue;
                        if (!exceptionHandlers.empty()) {
                            ExceptionHandler handler = exceptionHandlers.back();
                            exceptionHandlers.pop_back();
                            closeUpvalues(handler.stackDepth);
                            while (frames.size() > handler.frameDepth) {
                                frames.pop_back();
                            }
                            frame = &frames.back();
                            stack.resize(handler.stackDepth);
                            push(exceptionVal);
                            frame->ip = handler.catchIp;
                            break;
                        } else {
                            std::cerr << "Unhandled Exception: " << exceptionVal << "\n";
                            printStackTrace();
                            return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                    break;
                }
                
                if (callee.isClass()) {
                    ObjClass* klass = callee.asClass();
                    ObjInstance* instance = allocateObject<ObjInstance>(klass);
                    stack[stack.size() - argCount - 1] = Value(instance);
                    
                    auto initializer = klass->methods.find("init");
                    if (initializer != klass->methods.end()) {
                        ObjClosure* initClosure = initializer->second.asClosure();
                        
                        if (argCount != initClosure->function->arity) {
                            runtimeError("Expected %d arguments but got %d.", initClosure->function->arity, argCount);
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        if (frames.size() == 256) {
                            runtimeError("Stack overflow.");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        CallFrame newFrame;
                        newFrame.closure = initClosure;
                        newFrame.ip = initClosure->function->chunk->code.data();
                        newFrame.slots = stack.size() - argCount - 1;
                        frames.push_back(newFrame);
                        frame = &frames.back();
                    } else if (argCount != 0) {
                        runtimeError("Expected 0 arguments but got %d.", argCount);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                }
                
                if (callee.isBoundMethod()) {
                    ObjBoundMethod* bound = callee.asBoundMethod();
                    stack[stack.size() - argCount - 1] = Value(bound->receiver);
                    
                    ObjClosure* methodClosure = bound->method;
                    if (argCount != methodClosure->function->arity) {
                        runtimeError("Expected %d arguments but got %d.", methodClosure->function->arity, argCount);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    if (frames.size() == 256) {
                        runtimeError("Stack overflow.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    CallFrame newFrame;
                    newFrame.closure = methodClosure;
                    newFrame.ip = methodClosure->function->chunk->code.data();
                    newFrame.slots = stack.size() - argCount - 1;
                    frames.push_back(newFrame);
                    frame = &frames.back();
                    break;
                }

                if (callee.isBoundNativeMethod()) {
                    ObjBoundNativeMethod* bound = callee.asBoundNativeMethod();
                    stack[stack.size() - argCount - 1] = bound->receiver;
                    NativeFn native = bound->method;
                    Value result = native(argCount + 1, &stack[stack.size() - argCount - 1]);
                    stack.resize(stack.size() - argCount - 1);
                    push(result);
                    if (VM::currentVM->hasException) {
                        VM::currentVM->hasException = false;
                        Value exceptionVal = VM::currentVM->exceptionValue;
                        if (!exceptionHandlers.empty()) {
                            ExceptionHandler handler = exceptionHandlers.back();
                            exceptionHandlers.pop_back();
                            closeUpvalues(handler.stackDepth);
                            while (frames.size() > handler.frameDepth) {
                                frames.pop_back();
                            }
                            frame = &frames.back();
                            stack.resize(handler.stackDepth);
                            push(exceptionVal);
                            frame->ip = handler.catchIp;
                            break;
                        } else {
                            std::cerr << "Unhandled Exception: " << exceptionVal << "\n";
                            printStackTrace();
                            return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                    break;
                }
                
                if (!callee.isClosure()) {
                    runtimeError("Can only call functions and classes.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                ObjClosure* closure = callee.asClosure();

                if (closure->function->isVariadic) {
                    // Collect extra args (beyond arity regular params) into an array
                    int fixedArity = closure->function->arity;
                    if (argCount < fixedArity) {
                        runtimeError("Expected at least %d arguments but got %d.", fixedArity, argCount);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    int extraCount = argCount - fixedArity;
                    auto varArray = allocateObject<ObjArray>();
                    // extra args are at stack[size-extraCount .. size-1]
                    for (int ei = 0; ei < extraCount; ei++) {
                        varArray->elements.push_back(stack[stack.size() - extraCount + ei]);
                    }
                    // Shrink to fixedArity args then push the array
                    stack.resize(stack.size() - extraCount);
                    push(Value(varArray));
                    argCount = fixedArity + 1; // closure slot + fixed + varArray
                } else if (argCount != closure->function->arity) {
                    runtimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (frames.size() == 256) {
                    runtimeError("Stack overflow.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                CallFrame newFrame;
                newFrame.closure = closure;
                newFrame.ip = closure->function->chunk->code.data();
                newFrame.slots = stack.size() - argCount - 1;
                frames.push_back(newFrame);
                frame = &frames.back();
                break;
            }
                
            case OP_BUILD_ARRAY: {
                uint8_t itemCount = READ_BYTE();
                ObjArray* array = allocateObject<ObjArray>();
                for (int i = itemCount - 1; i >= 0; i--) {
                    array->elements.push_back(peek(i));
                }
                for (int i = 0; i < itemCount; i++) {
                    pop();
                }
                push(Value(array));
                break;
            }
            case OP_BUILD_MAP: {
                uint8_t pairCount = READ_BYTE();
                auto objMap = allocateObject<ObjMap>();
                
                std::vector<std::pair<Value, Value>> tempPairs(pairCount);
                for (int i = pairCount - 1; i >= 0; i--) {
                    Value val = pop();
                    Value key = pop();
                    tempPairs[i] = std::make_pair(key, val);
                }
                
                for (int i = 0; i < pairCount; i++) {
                    Value key = tempPairs[i].first;
                    Value val = tempPairs[i].second;
                    
                    std::string keyStr;
                    if (key.isString()) {
                        keyStr = key.asString();
                    } else {
                        std::stringstream ss;
                        ss << key;
                        keyStr = ss.str();
                    }
                    objMap->entries[keyStr] = std::make_pair(key, val);
                }
                push(Value(objMap));
                break;
            }
            case OP_INDEX_GET: {
                Value index = pop();
                Value receiver = pop();
                if (receiver.isArray()) {
                    if (!index.isNumber()) {
                        runtimeError("Array index must be a number.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    ObjArray* array = receiver.asArray();
                    int idx = (int)index.asNumber();
                    if (idx < 0 || idx >= array->elements.size()) {
                        runtimeError("Array index out of bounds.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(array->elements[idx]);
                } else if (receiver.isMap()) {
                    ObjMap* map = receiver.asMap();
                    std::string keyStr;
                    if (index.isString()) {
                        keyStr = index.asString();
                    } else {
                        std::stringstream ss;
                        ss << index;
                        keyStr = ss.str();
                    }
                    
                    auto it = map->entries.find(keyStr);
                    if (it == map->entries.end()) {
                        runtimeError("Key '%s' not found in map.", keyStr.c_str());
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(it->second.second);
                } else {
                    runtimeError("Only arrays and maps can be indexed.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_INDEX_SET: {
                Value value = pop();
                Value index = pop();
                Value receiver = pop();
                if (receiver.isArray()) {
                    if (!index.isNumber()) {
                        runtimeError("Array index must be a number.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    ObjArray* array = receiver.asArray();
                    int idx = (int)index.asNumber();
                    if (idx < 0 || idx >= array->elements.size()) {
                        runtimeError("Array index out of bounds.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    array->elements[idx] = value;
                    push(value);
                } else if (receiver.isMap()) {
                    ObjMap* map = receiver.asMap();
                    std::string keyStr;
                    if (index.isString()) {
                        keyStr = index.asString();
                    } else {
                        std::stringstream ss;
                        ss << index;
                        keyStr = ss.str();
                    }
                    map->entries[keyStr] = std::make_pair(index, value);
                    push(value);
                } else {
                    runtimeError("Only arrays and maps can be indexed.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_RETURN: {
                Value result = pop();
                CallFrame poppedFrame = frames.back();
                
                closeUpvalues(poppedFrame.slots);
                
                frames.pop_back();
                if (frames.empty()) {
                    pop();
                    return INTERPRET_OK;
                }
                
                stack.resize(poppedFrame.slots);
                push(result);
                frame = &frames.back();
                break;
            }
            case OP_TRY: {
                uint16_t offset = READ_SHORT();
                ExceptionHandler handler;
                handler.catchIp = frame->ip + offset;
                handler.stackDepth = (int)stack.size();
                handler.frameDepth = frames.size();
                exceptionHandlers.push_back(handler);
                break;
            }
            case OP_END_TRY: {
                if (!exceptionHandlers.empty()) {
                    exceptionHandlers.pop_back();
                }
                break;
            }
            case OP_THROW: {
                Value exceptionValue = pop();
                if (!exceptionHandlers.empty()) {
                    ExceptionHandler handler = exceptionHandlers.back();
                    exceptionHandlers.pop_back();
                    
                    closeUpvalues(handler.stackDepth);
                    
                    while (frames.size() > handler.frameDepth) {
                        frames.pop_back();
                    }
                    frame = &frames.back();
                    
                    stack.resize(handler.stackDepth);
                    push(exceptionValue);
                    
                    frame->ip = handler.catchIp;
                } else {
                    std::cerr << "Unhandled Exception: " << exceptionValue << "\n";
                    printStackTrace();
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_IMPORT: {
                std::string path = READ_STRING();
                // Resolve candidates
                std::string resolved = path;
                if (!currentDir.empty() && path.find('/') == std::string::npos && path.find('\\') == std::string::npos) {
                    resolved = currentDir + path;
                }
                std::vector<std::string> candidates = { resolved };
                if (resolved.size() < 5 || (resolved.substr(resolved.size()-5) != ".cvmc" && resolved.substr(resolved.size()-4) != ".cvm")) {
                    candidates.push_back(resolved + ".cvmc");
                    candidates.push_back(resolved + ".cvm");
                }
                std::string foundPath;
                for (const auto& c : candidates) {
                    std::ifstream test(c, std::ios::binary);
                    if (test.is_open()) { foundPath = c; break; }
                }
                if (foundPath.empty()) {
                    std::string errMsg = std::string("Module not found: '") + path + "'";
                    if (!exceptionHandlers.empty()) {
                        ExceptionHandler handler = exceptionHandlers.back();
                        exceptionHandlers.pop_back();
                        closeUpvalues(handler.stackDepth);
                        while (frames.size() > handler.frameDepth) frames.pop_back();
                        frame = &frames.back();
                        stack.resize(handler.stackDepth);
                        push(Value(errMsg));
                        frame->ip = handler.catchIp;
                    } else {
                        runtimeError("Module not found: '%s'", path.c_str());
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                }
                if (!importModule(path)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                // importModule pushed a new frame; re-sync our local frame pointer
                frame = &frames.back();
                break;
            }
            case OP_COLLECTION_SIZE: {
                uint8_t slot = READ_BYTE();
                Value coll = stack[frame->slots + slot];
                double sz = 0.0;
                if (coll.isArray()) sz = (double)coll.asArray()->elements.size();
                else if (coll.isMap()) sz = (double)coll.asMap()->entries.size();
                push(Value(sz));
                break;
            }
            case OP_ITER_GET: {
                uint8_t collSlot = READ_BYTE();
                uint8_t idxSlot  = READ_BYTE();
                Value coll = stack[frame->slots + collSlot];
                Value idx  = stack[frame->slots + idxSlot];
                int i = (int)idx.asNumber();
                if (coll.isArray()) {
                    auto& elems = coll.asArray()->elements;
                    if (i >= 0 && i < (int)elems.size()) push(elems[i]);
                    else push(Value(0.0));
                } else if (coll.isMap()) {
                    // Iterate over original key Values (not the lookup string key)
                    int count = 0;
                    bool found = false;
                    for (const auto& entry : coll.asMap()->entries) {
                        if (count == i) { push(entry.second.first); found = true; break; }
                        count++;
                    }
                    if (!found) push(Value(0.0));
                } else {
                    push(Value(0.0));
                }
                break;
            }
        }
    }
    
    #undef READ_BYTE
    #undef READ_CONSTANT
    #undef READ_SHORT
    #undef READ_STRING
    #undef BINARY_OP
}

void VM::markObject(Obj* object) {
    if (object == nullptr) return;
    if (object->isMarked) return;
    object->isMarked = true;
    grayStack.push_back(object);
}

void VM::markValue(Value value) {
    if (value.isString()) return; 
    if (value.isFunction()) markObject(value.asFunction());
    else if (value.isArray()) markObject(value.asArray());
    else if (value.isClosure()) markObject(value.asClosure());
    else if (value.isClass()) markObject(value.asClass());
    else if (value.isInstance()) markObject(value.asInstance());
    else if (value.isBoundMethod()) markObject(value.asBoundMethod());
    else if (value.isBoundNativeMethod()) markObject(value.asBoundNativeMethod());
    else if (value.isMap()) markObject(value.asMap());
}

void VM::markRoots() {
    for (auto& value : stack) markValue(value);
    for (auto& frame : frames) markObject(frame.closure);
    for (ObjUpvalue* upvalue = openUpvalues; upvalue != nullptr; upvalue = upvalue->nextUpvalue) {
        markObject(upvalue);
    }
    for (auto& entry : globals) markValue(entry.second);
}

void VM::traceReferences() {
    while (!grayStack.empty()) {
        Obj* object = grayStack.back();
        grayStack.pop_back();

        switch (object->objType) {
            case ObjType::FUNCTION: {
                ObjFunction* func = (ObjFunction*)object;
                if (func->chunk) {
                    for (auto& value : func->chunk->constants.values) {
                        markValue(value);
                    }
                }
                break;
            }
            case ObjType::CLOSURE: {
                ObjClosure* closure = (ObjClosure*)object;
                markObject(closure->function);
                for (auto* upvalue : closure->upvalues) markObject(upvalue);
                break;
            }
            case ObjType::UPVALUE: {
                markValue(((ObjUpvalue*)object)->closed);
                break;
            }
            case ObjType::CLASS: {
                ObjClass* klass = (ObjClass*)object;
                for (auto& entry : klass->methods) markValue(entry.second);
                break;
            }
            case ObjType::INSTANCE: {
                ObjInstance* instance = (ObjInstance*)object;
                markObject(instance->klass);
                for (auto& entry : instance->fields) markValue(entry.second);
                break;
            }
            case ObjType::BOUND_METHOD: {
                ObjBoundMethod* bound = (ObjBoundMethod*)object;
                markObject(bound->receiver);
                markObject(bound->method);
                break;
            }
            case ObjType::BOUND_NATIVE_METHOD: {
                ObjBoundNativeMethod* bound = (ObjBoundNativeMethod*)object;
                markValue(bound->receiver);
                break;
            }
            case ObjType::ARRAY: {
                ObjArray* array = (ObjArray*)object;
                for (auto& value : array->elements) markValue(value);
                break;
            }
            case ObjType::MAP: {
                ObjMap* map = (ObjMap*)object;
                for (auto& entry : map->entries) {
                    markValue(entry.second.first);
                    markValue(entry.second.second);
                }
                break;
            }
            default: break;
        }
    }
}

void VM::sweep() {
    Obj* previous = nullptr;
    Obj* object = objects;
    while (object != nullptr) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous != nullptr) previous->next = object;
            else objects = object;
            delete unreached;
        }
    }
}

void VM::collectGarbage() {
    markRoots();
    traceReferences();
    sweep();
    nextGC = bytesAllocated * 2;
}
