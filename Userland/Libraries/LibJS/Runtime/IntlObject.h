/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibJS/Runtime/Object.h>

namespace JS {

class IntlObject final : public Object {
    JS_OBJECT(IntlObject, Object);

public:
    explicit IntlObject(GlobalObject&);
    virtual void initialize(GlobalObject&) override;
    virtual ~IntlObject() override;

private:
    JS_DECLARE_NATIVE_FUNCTION(get_canonical_locales);
};

Vector<Value> canonicalize_locale_list(GlobalObject&, Value locales);
Object* coerce_options_to_object(GlobalObject&, Value options);
enum class OptionType {
    Boolean,
    String
};
Value get_option(GlobalObject&, Object& options, PropertyName const& property, OptionType, Vector<String> values, Value fallback);

}
