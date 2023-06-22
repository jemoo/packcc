//single_line_comment

[AttributeName]
module ModuleName

    /*
    delimited_comment
*/   

import ModuleName
import ModuleName.SubModuleName as ModuleAliasName
import ModuleName.SubModuleName { SymbolName, SymbolName, SymbolName, SymbolName }

public struct StructName {
    var Field1: int
    let Field2: int = 0

    public const {
        Field3: int = 1
        Field4: string = "abc"
        Field5: bool = true
    }

    [if debug]
    DebugMethod() {
    }

    [inline]
    public InlineMethod() {
    }

    const ConstMethod() {
    }
}

class ClassName<TypeParam1, TypeParam2, TypeParam3> {
}

class ChildClass : ModuleName.ClassName<int, float, bool> {
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

[inline]
[export: C, deprecated, unsafe]
function GlobalFunction<T>(Param1: int?, Param2: StructName&, const Param3: Array<float*?>** ...): (Map<int, Array<float>>, bool) {
}

type MyInt = int
public type Integer = int8 | int16 | int32 | int64
public type Nullable<T> = T | null
