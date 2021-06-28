/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibJS/Runtime/NumberFormat.h>

namespace JS {

class NumberFormatPrototype final : public Object {
    JS_OBJECT(NumberFormatPrototype, Object);

public:
    NumberFormatPrototype(GlobalObject&);
    virtual void initialize(GlobalObject&) override;
    virtual ~NumberFormatPrototype() override;

private:
    static NumberFormatPrototype* typed_this(VM&, GlobalObject&);
};

}
