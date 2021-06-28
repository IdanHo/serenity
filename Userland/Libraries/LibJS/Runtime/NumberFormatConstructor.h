/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibJS/Runtime/NativeFunction.h>

namespace JS {

class NumberFormatConstructor final : public NativeFunction {
    JS_OBJECT(NumberFormatConstructor, NativeFunction);

public:
    explicit NumberFormatConstructor(GlobalObject&);
    virtual void initialize(GlobalObject&) override;
    virtual ~NumberFormatConstructor() override;

    virtual Value call() override;
    virtual Value construct(FunctionObject&) override;

private:
    virtual bool has_constructor() const override { return true; }
};

}
