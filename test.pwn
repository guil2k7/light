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
        case LIGHT_ERROR_EXPECT_BUT_FOUND:
            printf("ERROR: expected: `%s`, but found: `%s`.", TOKEN_NAME[getarg(1)], TOKEN_NAME[getarg(2)]);

        case LIGHT_ERROR_INVALID_EXPR:
            printf("ERROR: invalid expression.");

        case LIGHT_ERROR_INVALID_STMT:
            printf("ERROR: invalid statement.");

        case LIGHT_ERROR_LIGHT_MAX_VARS:
            printf("ERROR: max number of variables reached, cannot create new one.");

        case LIGHT_ERROR_VAR_NOT_FOUND:
            printf("ERROR: variable not found.");

        case LIGHT_ERROR_NATIVE_NOT_FOUND:
            printf("ERROR: native function not found.");

        case LIGHT_ERROR_CALL_NUM_ARGS:
            printf("ERROR: number of arguments does not match definition.");

        default: {}
    }
}

native_SetPlayerPos(const parms[]) {
    new playerid = parms[0];
    new Float:x = float(parms[1]);
    new Float:y = float(parms[2]);
    new Float:z = float(parms[3]);

    printf(
        "SetPlayerPos(%d, %f, %f, %f);",
        playerid, x, y, z
    );

    // SetPlayerPos(playerid, x, y, z);

    return 0;
}

main() {
    new byteCode[32];

    LightRegisterNative("SetPlayerPos", __addressof(native_SetPlayerPos), 4);

    if (LightCompile("$x = 1; $y = 2; $z = 3; @SetPlayerPos(0, $x, $y, $z);", byteCode))
        printf("Program result: %d", LightExecute(byteCode));

    if (LightCompile("$x = $foo;", byteCode))
        printf("Program result: %d", LightExecute(byteCode));

    if (LightCompile("1 + 2 * 3;", byteCode))
        printf("Program result: %d", LightExecute(byteCode));
}
