#ifndef cvm_value_h
#define cvm_value_h

#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>

class Chunk;

enum class ObjType { FUNCTION, CLOSURE, UPVALUE, CLASS, INSTANCE, BOUND_METHOD, BOUND_NATIVE_METHOD, ARRAY, MAP };

struct Obj {
    ObjType objType;
    bool isMarked;
    Obj* next;

    Obj(ObjType type) : objType(type), isMarked(false), next(nullptr) {}
    virtual ~Obj() = default;
};

struct ObjFunction : public Obj {
    int arity;
    int upvalueCount;
    bool isVariadic;
    std::string name;
    Chunk* chunk;
    
    ObjFunction();
    virtual ~ObjFunction();
};

struct ObjArray;
struct ObjMap;
struct ObjUpvalue;
struct ObjClosure;
struct ObjClass;
struct ObjInstance;
struct ObjBoundMethod;
struct ObjBoundNativeMethod;

struct Value;
typedef Value (*NativeFn)(int argCount, Value* args);

enum class ValueType { NUMBER, BOOLEAN, STRING, FUNCTION, NATIVE, ARRAY, CLOSURE, CLASS, INSTANCE, BOUND_METHOD, BOUND_NATIVE_METHOD, MAP };

struct Value {
    ValueType type;
    double number;
    bool boolean;
    std::string string;
    ObjFunction* function;
    NativeFn native;
    ObjArray* array;
    ObjClosure* closure;
    ObjClass* klass;
    ObjInstance* instance;
    ObjBoundMethod* boundMethod;
    ObjBoundNativeMethod* boundNative;
    ObjMap* map;

    Value() : type(ValueType::NUMBER), number(0.0) {}
    Value(double d) : type(ValueType::NUMBER), number(d) {}
    Value(bool b) : type(ValueType::BOOLEAN), boolean(b) {}
    Value(const std::string& s) : type(ValueType::STRING), string(s) {}
    Value(const char* s) : type(ValueType::STRING) {
        if (s) string = s;
        else string = "";
    }
    Value(ObjFunction* f) : type(ValueType::FUNCTION), function(f) {}
    Value(NativeFn n) : type(ValueType::NATIVE), native(n) {}
    Value(ObjArray* a) : type(ValueType::ARRAY), array(a) {}
    Value(ObjClosure* c) : type(ValueType::CLOSURE), closure(c) {}
    Value(ObjClass* k) : type(ValueType::CLASS), klass(k) {}
    Value(ObjInstance* i) : type(ValueType::INSTANCE), instance(i) {}
    Value(ObjBoundMethod* bm) : type(ValueType::BOUND_METHOD), boundMethod(bm) {}
    Value(ObjBoundNativeMethod* bn) : type(ValueType::BOUND_NATIVE_METHOD), boundNative(bn) {}
    Value(ObjMap* m) : type(ValueType::MAP), map(m) {}

    bool isNumber() const { return type == ValueType::NUMBER; }
    bool isBool() const { return type == ValueType::BOOLEAN; }
    bool isString() const { return type == ValueType::STRING; }
    bool isFunction() const { return type == ValueType::FUNCTION; }
    bool isNative() const { return type == ValueType::NATIVE; }
    bool isArray() const { return type == ValueType::ARRAY; }
    bool isClosure() const { return type == ValueType::CLOSURE; }
    bool isClass() const { return type == ValueType::CLASS; }
    bool isInstance() const { return type == ValueType::INSTANCE; }
    bool isBoundMethod() const { return type == ValueType::BOUND_METHOD; }
    bool isBoundNativeMethod() const { return type == ValueType::BOUND_NATIVE_METHOD; }
    bool isMap() const { return type == ValueType::MAP; }

    double asNumber() const { return number; }
    bool asBool() const { return boolean; }
    const std::string& asString() const { return string; }
    ObjFunction* asFunction() const { return function; }
    NativeFn asNative() const { return native; }
    ObjArray* asArray() const { return array; }
    ObjClosure* asClosure() const { return closure; }
    ObjClass* asClass() const { return klass; }
    ObjInstance* asInstance() const { return instance; }
    ObjBoundMethod* asBoundMethod() const { return boundMethod; }
    ObjBoundNativeMethod* asBoundNativeMethod() const { return boundNative; }
    ObjMap* asMap() const { return map; }

    bool isTruthy() const {
        if (isBool()) return asBool();
        if (isNumber()) return asNumber() != 0.0;
        if (isString()) return !asString().empty();
        if (isFunction()) return true;
        if (isNative()) return true;
        if (isArray()) return true;
        if (isMap()) return true;
        if (isClosure()) return true;
        if (isClass()) return true;
        if (isInstance()) return true;
        if (isBoundMethod()) return true;
        if (isBoundNativeMethod()) return true;
        return false;
    }

    friend std::ostream& operator<<(std::ostream& os, const Value& val);
    
    bool operator==(const Value& other) const {
        if (type != other.type) return false;
        if (isNumber()) return number == other.number;
        if (isBool()) return boolean == other.boolean;
        if (isString()) return string == other.string;
        if (isFunction()) return function == other.function;
        if (isNative()) return native == other.native;
        if (isArray()) return array == other.array;
        if (isMap()) return map == other.map;
        if (isClosure()) return closure == other.closure;
        if (isClass()) return klass == other.klass;
        if (isInstance()) return instance == other.instance;
        if (isBoundMethod()) return boundMethod == other.boundMethod;
        if (isBoundNativeMethod()) return boundNative == other.boundNative;
        return false;
    }
};

struct ObjArray : public Obj {
    std::vector<Value> elements;
    ObjArray() : Obj(ObjType::ARRAY) {}
};

struct ObjMap : public Obj {
    std::unordered_map<std::string, std::pair<Value, Value>> entries;
    ObjMap() : Obj(ObjType::MAP) {}
};

struct ObjUpvalue : public Obj {
    int index;
    Value closed;
    ObjUpvalue* nextUpvalue;

    ObjUpvalue(int index) : Obj(ObjType::UPVALUE), index(index), nextUpvalue(nullptr) {}
};

struct ObjClosure : public Obj {
    ObjFunction* function;
    std::vector<ObjUpvalue*> upvalues;

    ObjClosure(ObjFunction* function) : Obj(ObjType::CLOSURE), function(function) {}
};

struct ObjClass : public Obj {
    std::string name;
    std::unordered_map<std::string, Value> methods;
    ObjClass(std::string name) : Obj(ObjType::CLASS), name(name) {}
};

struct ObjInstance : public Obj {
    ObjClass* klass;
    std::unordered_map<std::string, Value> fields;
    ObjInstance(ObjClass* klass) : Obj(ObjType::INSTANCE), klass(klass) {}
};

struct ObjBoundMethod : public Obj {
    ObjInstance* receiver;
    ObjClosure* method;
    ObjBoundMethod(ObjInstance* receiver, ObjClosure* method)
        : Obj(ObjType::BOUND_METHOD), receiver(receiver), method(method) {}
};

struct ObjBoundNativeMethod : public Obj {
    Value receiver;
    NativeFn method;
    ObjBoundNativeMethod(Value receiver, NativeFn method) 
        : Obj(ObjType::BOUND_NATIVE_METHOD), receiver(receiver), method(method) {}
};

inline std::ostream& operator<<(std::ostream& os, const Value& val) {
    if (val.isNumber()) os << val.asNumber();
    else if (val.isBool()) os << (val.asBool() ? "true" : "false");
    else if (val.isString()) os << val.asString();
    else if (val.isFunction()) {
        if (val.asFunction()->name.empty()) os << "<script>";
        else os << "<fn " << val.asFunction()->name << ">";
    }
    else if (val.isClosure()) {
        if (val.asClosure()->function->name.empty()) os << "<script>";
        else os << "<fn " << val.asClosure()->function->name << ">";
    }
    else if (val.isClass()) os << val.asClass()->name;
    else if (val.isInstance()) os << val.asInstance()->klass->name << " instance";
    else if (val.isBoundMethod()) {
        if (val.asBoundMethod()->method->function->name.empty()) os << "<method>";
        else os << "<method " << val.asBoundMethod()->method->function->name << ">";
    }
    else if (val.isBoundNativeMethod()) {
        os << "<bound native fn>";
    }
    else if (val.isNative()) os << "<native fn>";
    else if (val.isArray()) {
        os << "[";
        for (size_t i = 0; i < val.asArray()->elements.size(); i++) {
            os << val.asArray()->elements[i];
            if (i < val.asArray()->elements.size() - 1) os << ", ";
        }
        os << "]";
    }
    else if (val.isMap()) {
        os << "{";
        size_t count = 0;
        for (const auto& entry : val.asMap()->entries) {
            os << entry.second.first << ": " << entry.second.second;
            if (count < val.asMap()->entries.size() - 1) {
                os << ", ";
            }
            count++;
        }
        os << "}";
    }
    return os;
}

class ValueArray {
public:
    ValueArray() = default;
    
    void writeValue(Value value);
    void freeValueArray();

    std::vector<Value> values;
};

#endif
