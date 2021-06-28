/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/IntlObject.h>
#include <LibJS/Runtime/NumberFormat.h>
#include <LibJS/Runtime/NumberFormatConstructor.h>

namespace JS {

NumberFormatConstructor::NumberFormatConstructor(GlobalObject& global_object)
    : NativeFunction(vm().names.NumberFormat.as_string(), *global_object.function_prototype())
{
}

void NumberFormatConstructor::initialize(GlobalObject& global_object)
{
    auto& vm = this->vm();
    NativeFunction::initialize(global_object);

    // 15.3.1 Intl.NumberFormat.prototype, https://tc39.es/ecma402/#sec-intl.numberformat.prototype
    define_property(vm.names.prototype, global_object.number_format_prototype(), 0);

    define_property(vm.names.length, Value(0), Attribute::Configurable);
}

NumberFormatConstructor::~NumberFormatConstructor()
{
}

// 15.2.1 Intl.NumberFormat ( [ locales [ , options ] ] ), https://tc39.es/ecma402/#sec-intl.numberformat
Value NumberFormatConstructor::call()
{
    return NumberFormatConstructor::construct(*this);
}

// 15.1.2 InitializeNumberFormat ( numberFormat, locales, options ), https://tc39.es/ecma402/#sec-initializenumberformat
static void initialize_number_format(GlobalObject& global_object, NumberFormat& number_format, Value locales, Value options)
{
    auto& vm = global_object.vm();

    auto requested_locales = canonicalize_locale_list(global_object, locales);
    if (vm.exception())
        return;

    auto options_object = coerce_options_to_object(global_object, options);
    if (vm.exception())
        return;

    auto opt = Object::create(global_object, global_object.object_prototype());
    auto matcher = get_option(global_object, *options_object, "localeMatcher", OptionType::String, { "lookup", "best fit" }, js_string(vm, "best fit"));
    if (vm.exception())
        return;
    opt->define_property("localeMatcher", matcher);

    TODO();
}

// 15.2.1 Intl.NumberFormat ( [ locales [ , options ] ] ), https://tc39.es/ecma402/#sec-intl.numberformat
Value NumberFormatConstructor::construct(FunctionObject& new_target)
{
    auto& vm = this->vm();
    auto& global_object = this->global_object();

    auto number_format = ordinary_create_from_constructor<NumberFormat>(global_object, new_target, &GlobalObject::number_format_prototype);
    if (vm.exception())
        return {};

    initialize_number_format(global_object, *number_format, vm.argument(0), vm.argument(1));
    if (vm.exception())
        return {};

    return number_format;
}

}
