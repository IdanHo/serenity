/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/IntlObject.h>

namespace JS {

IntlObject::IntlObject(GlobalObject& global_object)
    : Object(*global_object.object_prototype())
{
}

void IntlObject::initialize(GlobalObject& global_object)
{
    auto& vm = this->vm();
    Object::initialize(global_object);

    u8 attr = Attribute::Writable | Attribute::Configurable;
    define_native_function(vm.names.getCanonicalLocales, get_canonical_locales, 1, attr);

    // 8.1.1 Intl[ @@toStringTag ], https://tc39.es/ecma402/#sec-Intl-toStringTag
    define_property(*vm.well_known_symbol_to_string_tag(), js_string(global_object.heap(), vm.names.Intl.as_string()), Attribute::Configurable);
}

IntlObject::~IntlObject()
{
}

// 6.2.3 CanonicalizeUnicodeLocaleId ( locale ), https://tc39.es/ecma402/#sec-canonicalizeunicodelocaleid
String canonicalize_unicode_locale_id(String const& locale)
{
    TODO();
}

// 6.2.2 IsStructurallyValidLanguageTag ( locale ), https://tc39.es/ecma402/#sec-isstructurallyvalidlanguagetag
bool is_structurally_valid_language_tag(String const& locale)
{
    TODO();
}

// 9.2.1 CanonicalizeLocaleList ( locales ), https://tc39.es/ecma402/#sec-canonicalizelocalelist
Vector<Value> canonicalize_locale_list(GlobalObject& global_object, Value locales)
{
    auto& vm = global_object.vm();

    if (locales.is_undefined())
        return {};

    Object* object;
    // FIXME: or Type(locales) is Object and locales has an [[InitializedLocale]] internal slot,
    if (locales.is_string()) {
        object = Array::create_from(global_object, { locales });
    } else {
        object = locales.to_object(global_object);
        if (vm.exception())
            return {};
    }

    auto length_value = object->get(vm.names.length).value_or(js_undefined());
    if (vm.exception())
        return {};
    auto length = length_value.to_length(global_object);
    if (vm.exception())
        return {};

    HashTable<String> seen;
    for (size_t k = 0; k < length; ++k) {
        auto pk = String::number(k);
        auto k_present = object->has_own_property(pk);
        if (vm.exception())
            return {};
        if (!k_present)
            continue;
        auto k_value = object->get(pk).value_or(js_undefined());
        if (vm.exception())
            return {};
        if (!k_value.is_string() && !k_value.is_object()) {
            vm.throw_exception<TypeError>(global_object, ErrorType::InvalidLanguageTagType);
            return {};
        }
        String tag;
        // FIXME: If Type(kValue) is Object and kValue has an [[InitializedLocale]] internal slot
        tag = k_value.to_string(global_object);
        if (vm.exception())
            return {};
        if (!is_structurally_valid_language_tag(tag)) {
            vm.throw_exception<RangeError>(global_object, ErrorType::InvalidLanguageTag);
            return {};
        }
        seen.set(canonicalize_unicode_locale_id(tag));
    }
    Vector<Value> result;
    result.ensure_capacity(seen.size());
    for (auto& entry : seen)
        result.append(js_string(vm, entry));
    return result;
}

// 9.2.12 CoerceOptionsToObject ( options ), https://tc39.es/ecma402/#sec-coerceoptionstoobject
Object* coerce_options_to_object(GlobalObject& global_object, Value options)
{
    auto& vm = global_object.vm();

    if (options.is_undefined())
        return Object::create(global_object, nullptr);

    auto object = options.to_object(global_object);
    if (vm.exception())
        return {};

    return object;
}

// 9.2.13 GetOption ( options, property, type, values, fallback ), https://tc39.es/ecma402/#sec-getoption
Value get_option(GlobalObject& global_object, Object& options, PropertyName const& property, OptionType type, Vector<String> values, Value fallback)
{
    auto& vm = global_object.vm();

    auto value = options.get(property).value_or(js_undefined());
    if (vm.exception())
        return {};

    if (value.is_undefined())
        return fallback;

    if (type == OptionType::Boolean) {
        VERIFY(values.is_empty());
        return Value(value.to_boolean());
    } else if (type == OptionType::String) {
        auto string = value.to_string(global_object);
        if (vm.exception())
            return {};
        if (!values.is_empty() && !values.contains_slow(string)) {
            vm.throw_exception<RangeError>(global_object, ErrorType::InvalidOptionsProperty, string, property.to_value(vm).to_string_without_side_effects());
            return {};
        }
        return js_string(vm, string);
    }
    VERIFY_NOT_REACHED();
}

// 8.3.1 Intl.getCanonicalLocales ( locales ), https://tc39.es/ecma402/#sec-intl.getcanonicallocales
JS_DEFINE_NATIVE_FUNCTION(IntlObject::get_canonical_locales)
{
    auto list = canonicalize_locale_list(global_object, vm.argument(0));
    if (vm.exception())
        return {};
    return Array::create_from(global_object, list);
}

}
