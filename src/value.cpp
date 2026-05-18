#include "value.h"
#include "chunk.h"

ObjFunction::ObjFunction() : Obj(ObjType::FUNCTION), arity(0), upvalueCount(0), isVariadic(false), name(""), chunk(new Chunk()) {}
ObjFunction::~ObjFunction() { delete chunk; }

void ValueArray::writeValue(Value value) {
    values.push_back(value);
}

void ValueArray::freeValueArray() {
    values.clear();
}
