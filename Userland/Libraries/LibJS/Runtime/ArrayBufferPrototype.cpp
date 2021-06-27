/*
 * Copyright (c) 2020, Linus Groh <linusg@serenityos.org>
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Function.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/ArrayBuffer.h>
#include <LibJS/Runtime/ArrayBufferConstructor.h>
#include <LibJS/Runtime/ArrayBufferPrototype.h>
#include <LibJS/Runtime/GlobalObject.h>

namespace JS {

ArrayBufferPrototype::ArrayBufferPrototype(GlobalObject& global_object)
    : Object(*global_object.object_prototype())
{
}

void ArrayBufferPrototype::initialize(GlobalObject& global_object)
{
    auto& vm = this->vm();
    Object::initialize(global_object);
    u8 attr = Attribute::Writable | Attribute::Configurable;
    define_native_function(vm.names.slice, slice, 2, attr);
    define_native_function(vm.names.resize, resize, 1, attr);
    define_native_function(vm.names.transfer, transfer, 0, attr);
    define_native_accessor(vm.names.byteLength, byte_length_getter, {}, Attribute::Configurable);
    define_native_accessor(vm.names.maxByteLength, max_byte_length_getter, {}, Attribute::Configurable);
    define_native_accessor(vm.names.resizable, resizable_getter, {}, Attribute::Configurable);

    // 25.1.5.4 ArrayBuffer.prototype [ @@toStringTag ], https://tc39.es/ecma262/#sec-arraybuffer.prototype-@@tostringtag
    define_property(*vm.well_known_symbol_to_string_tag(), js_string(vm.heap(), vm.names.ArrayBuffer.as_string()), Attribute::Configurable);
}

ArrayBufferPrototype::~ArrayBufferPrototype()
{
}

static ArrayBuffer* array_buffer_object_from(VM& vm, GlobalObject& global_object)
{
    // ArrayBuffer.prototype.* deliberately don't coerce |this| value to object.
    auto this_value = vm.this_value(global_object);
    if (!this_value.is_object() || !is<ArrayBuffer>(this_value.as_object())) {
        vm.throw_exception<TypeError>(global_object, ErrorType::NotAn, "ArrayBuffer");
        return nullptr;
    }
    return static_cast<ArrayBuffer*>(&this_value.as_object());
}

// 25.1.5.3 ArrayBuffer.prototype.slice ( start, end ), https://tc39.es/ecma262/#sec-arraybuffer.prototype.slice
JS_DEFINE_NATIVE_FUNCTION(ArrayBufferPrototype::slice)
{
    auto array_buffer_object = array_buffer_object_from(vm, global_object);
    if (!array_buffer_object)
        return {};

    // FIXME: Check for shared buffer
    if (array_buffer_object->is_detached()) {
        vm.throw_exception<TypeError>(global_object, ErrorType::DetachedArrayBuffer);
        return {};
    }

    auto length = array_buffer_object->byte_length();

    auto relative_start = vm.argument(0).to_integer_or_infinity(global_object);
    if (vm.exception())
        return {};

    double first;
    if (relative_start < 0)
        first = max(length + relative_start, 0.0);
    else
        first = min(relative_start, (double)length);

    auto relative_end = vm.argument(1).is_undefined() ? length : vm.argument(1).to_integer_or_infinity(global_object);
    if (vm.exception())
        return {};

    double final;
    if (relative_end < 0)
        final = max(length + relative_end, 0.0);
    else
        final = min(relative_end, (double)length);

    auto new_length = max(final - first, 0.0);

    auto constructor = species_constructor(global_object, *array_buffer_object, *global_object.array_buffer_constructor());
    if (vm.exception())
        return {};

    MarkedValueList arguments(vm.heap());
    arguments.append(Value(new_length));
    auto new_array_buffer = vm.construct(*constructor, *constructor, move(arguments));
    if (vm.exception())
        return {};

    if (!new_array_buffer.is_object() || !is<ArrayBuffer>(new_array_buffer.as_object())) {
        vm.throw_exception<TypeError>(global_object, ErrorType::SpeciesConstructorDidNotCreate, "an ArrayBuffer");
        return {};
    }
    auto* new_array_buffer_object = static_cast<ArrayBuffer*>(&new_array_buffer.as_object());

    // FIXME: Check for shared buffer
    if (new_array_buffer_object->is_detached()) {
        vm.throw_exception<TypeError>(global_object, ErrorType::SpeciesConstructorReturned, "a detached ArrayBuffer");
        return {};
    }
    if (same_value(new_array_buffer_object, array_buffer_object)) {
        vm.throw_exception<TypeError>(global_object, ErrorType::SpeciesConstructorReturned, "same ArrayBuffer instance");
        return {};
    }
    if (new_array_buffer_object->byte_length() < new_length) {
        vm.throw_exception<TypeError>(global_object, ErrorType::SpeciesConstructorReturned, "an ArrayBuffer smaller than requested");
        return {};
    }

    if (array_buffer_object->is_detached()) {
        vm.throw_exception<TypeError>(global_object, ErrorType::DetachedArrayBuffer);
        return {};
    }

    // This is ugly, is there a better way to do this?
    array_buffer_object->buffer().span().slice(first, new_length).copy_to(new_array_buffer_object->buffer().span());
    return new_array_buffer_object;
}

// 1.3.4 ArrayBuffer.prototype.resize ( newLength ), https://tc39.es/proposal-resizablearraybuffer/#sec-arraybuffer.prototype.resize
JS_DEFINE_NATIVE_FUNCTION(ArrayBufferPrototype::resize)
{
    auto array_buffer_object = array_buffer_object_from(vm, global_object);
    if (!array_buffer_object)
        return {};

    if (!array_buffer_object->is_resizable()) {
        vm.throw_exception<TypeError>(global_object, ErrorType::NotA, "resizable ArrayBuffer");
        return {};
    }

    // FIXME: Check for shared buffer
    if (array_buffer_object->is_detached()) {
        vm.throw_exception<TypeError>(global_object, ErrorType::DetachedArrayBuffer);
        return {};
    }

    auto new_byte_length = vm.argument(0).to_integer_or_infinity(global_object);
    if (vm.exception())
        return {};

    if (new_byte_length < 0 || new_byte_length > array_buffer_object->max_byte_length()) {
        vm.throw_exception<RangeError>(global_object, ErrorType::ArrayBufferInvalidByteLength);
        return {};
    }

    array_buffer_object->buffer().resize(new_byte_length);

    return js_undefined();
}

// 1.3.5 ArrayBuffer.prototype.transfer ( [ newLength ] ), https://tc39.es/proposal-resizablearraybuffer/#sec-arraybuffer.prototype.transfer
JS_DEFINE_NATIVE_FUNCTION(ArrayBufferPrototype::transfer)
{
    auto array_buffer_object = array_buffer_object_from(vm, global_object);
    if (!array_buffer_object)
        return {};

    // FIXME: Check for shared buffer
    if (array_buffer_object->is_detached()) {
        vm.throw_exception<TypeError>(global_object, ErrorType::DetachedArrayBuffer);
        return {};
    }

    size_t new_byte_length;
    if (vm.argument(0).is_undefined()) {
        new_byte_length = array_buffer_object->byte_length();
    } else {
        new_byte_length = vm.argument(0).to_integer_or_infinity(global_object);
        if (vm.exception())
            return {};
    }

    MarkedValueList arguments(vm.heap());
    arguments.empend(new_byte_length);
    auto new_array_buffer = vm.construct(*global_object.array_buffer_constructor(), *global_object.array_buffer_constructor(), move(arguments));
    if (vm.exception())
        return {};
    auto* new_array_buffer_object = static_cast<ArrayBuffer*>(&new_array_buffer.as_object());

    // This is ugly, is there a better way to do this?
    array_buffer_object->buffer().span().copy_trimmed_to(new_array_buffer_object->buffer().span());

    array_buffer_object->detach_buffer();

    return new_array_buffer_object;
}

// 25.1.5.1 get ArrayBuffer.prototype.byteLength, https://tc39.es/ecma262/#sec-get-arraybuffer.prototype.bytelength
JS_DEFINE_NATIVE_GETTER(ArrayBufferPrototype::byte_length_getter)
{
    auto array_buffer_object = array_buffer_object_from(vm, global_object);
    if (!array_buffer_object)
        return {};

    // FIXME: Check for shared buffer
    if (array_buffer_object->is_detached())
        return Value(0);

    return Value(array_buffer_object->byte_length());
}

// 1.3.2 get ArrayBuffer.prototype.maxByteLength, https://tc39.es/proposal-resizablearraybuffer/#sec-get-arraybuffer.prototype.maxbytelength
JS_DEFINE_NATIVE_GETTER(ArrayBufferPrototype::max_byte_length_getter)
{
    auto array_buffer_object = array_buffer_object_from(vm, global_object);
    if (!array_buffer_object)
        return {};

    // FIXME: Check for shared buffer
    if (array_buffer_object->is_detached())
        return Value(0);

    if (array_buffer_object->is_resizable())
        return Value(array_buffer_object->max_byte_length());
    else
        return Value(array_buffer_object->byte_length());
}

// 1.3.3 get ArrayBuffer.prototype.resizable, https://tc39.es/proposal-resizablearraybuffer/#sec-get-arraybuffer.prototype.resizable
JS_DEFINE_NATIVE_GETTER(ArrayBufferPrototype::resizable_getter)
{
    auto array_buffer_object = array_buffer_object_from(vm, global_object);
    if (!array_buffer_object)
        return {};

    // FIXME: Check for shared buffer

    return Value(array_buffer_object->is_resizable());
}

}
