/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/Object.h>

namespace JS {

class NumberFormat : public Object {
    JS_OBJECT(NumberFormat, Object);

public:
    static NumberFormat* create(GlobalObject&);

    explicit NumberFormat(Object& prototype);
    virtual ~NumberFormat() override;

private:
};

}
