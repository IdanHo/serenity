/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/IntlObject.h>
#include <LibJS/Runtime/NumberFormat.h>

namespace JS {

NumberFormat* NumberFormat::create(GlobalObject& global_object)
{
    return global_object.heap().allocate<NumberFormat>(global_object, *global_object.number_format_prototype());
}

NumberFormat::NumberFormat(Object& prototype)
    : Object(prototype)
{
}

NumberFormat::~NumberFormat()
{
}

}
