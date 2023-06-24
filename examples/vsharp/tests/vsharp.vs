//single_line_comment

[AttributeName]
module ModuleName

    /*
    delimited_comment
*/   

import strconv
import v.ast as Ast
import readline { Readline }

public struct BaseType {
    var Field1: int
    let Field2: int& = 0
}

//Map<List<Map<int, Array<float>>>, Map<string, int>>
public struct DeriveType : Ast.File, BaseType {
    public const {
        Field3: int = 1
        Field4: string = "abc"
        Field5: bool? = true
        Field6: float& = 0
    }

    DebugMethod<TValue>(value: TValue?) {
    }

    [inline]
    public InlineMethod(const value: double&) {
    }

    const ConstMethod(value: bool&) {
    }
}

class GenericClass<TypeParam1, TypeParam2, TypeParam3> {
    Field1: TypeParam1
    Field2: TypeParam2
    Field3: TypeParam3
}

class ChildClass : GenericClass<int, float, bool> {
}

union UnionName {
}

interface InterfaceName {
    InterfaceField: Array<int>
    InterfaceMethod(input: int): bool
    InterfaceMethod2(input: float): bool
}

interface Any {
}

interface Iterator<T> {
    GetNext(): T?
}

enum EnumName {
}

[if debug]
[inline]
[export: C, deprecated, unsafe]
function GlobalFunction<T>(Param1: int?, Param2: StructName&, const Param3: Array<float*?>** ...): (Map<int, Array<float>>, bool) {
}

type MyInt = int
public type Integer = int8 | int16 | int32 | int64
public type Nullable<T> = T | null
