// Copyright 2024 Maicol Castro (maicolcastro.abc@gmail.com).
// Distributed under the MIT License.
// See LICENSE.txt in the root directory of this project
// or at https://opensource.org/license/mit.

#include <a_samp>
#include "light"

static const TOKEN_NAME[][] = {
    "end of file",
    "identifier",
    "number",

    "+",
    "-",
    "*",
    "/",
    "@",
    "$",
    ";",
    "=",
    "(",
    ")",
    ","
};

stock LightThrowError(E_LIGHT_ERROR:error, {Float,_}:...) {
    switch (error) {
        case LIGHT_ERROR_EXPECT_IDENT:
            printf("expected a identifier, but found: %s", TOKEN_NAME[getarg(2)]);

        case LIGHT_ERROR_EXPECT_BUT_FOUND:
            printf("expected: %s, but found: %s", TOKEN_NAME[getarg(2)], TOKEN_NAME[getarg(3)]);

        case LIGHT_ERROR_INVALID_EXPR:
            printf("invalid expression");

        case LIGHT_ERROR_INVALID_STMT:
            printf("invalid statement");

        case LIGHT_ERROR_LIGHT_MAX_VARS:
            printf("max number of variables reached, cannot create new one");

        case LIGHT_ERROR_VAR_NOT_FOUND:
            printf("variable not found");

        case LIGHT_ERROR_NATIVE_NOT_FOUND:
            printf("native function not found");

        case LIGHT_ERROR_CALL_NUM_ARGS:
            printf("number of arguments does not match definition");

        default: {}
    }
}

native_addi(const parms[]) {
    new result = parms[0] + parms[1];

    printf("addi: %d + %d = %d", parms[0], parms[1], result);

    return result;
}

main() {
    new byteCode[32];

    LightRegisterNative("addi", __addressof(native_addi), 2);
    
    if (LightCompile("@addi(10, 5);", byteCode))
        printf("Program result: %d", LightExecute(byteCode));
}
