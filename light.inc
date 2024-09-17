// Copyright 2024 Maicol Castro (maicolcastro.abc@gmail.com).
// Distributed under the MIT License.
// See LICENSE.txt in the root directory of this project
// or at https://opensource.org/license/mit.

#if defined _GUIL2K7_LIGHT_
    #endinput
#endif

#define _GUIL2K7_LIGHT_

#if !defined LIGHT_MAX_CODE_LENGTH
    #define LIGHT_MAX_CODE_LENGTH 256
#endif

#if !defined LIGHT_MAX_VARIABLES
    #define LIGHT_MAX_VARIABLES 64
#endif

#if !defined LIGHT_MAX_IDENTIFIER_LEN
    #define LIGHT_MAX_IDENTIFIER_LEN 24
#endif

#if !defined LIGHT_MAX_NATIVES
    #define LIGHT_MAX_NATIVES 32
#endif

#if !defined LIGHT_STACK_SIZE
    #define LIGHT_STACK_SIZE 128
#endif

/// The Light error handler.
/// This function is called when an error is found in the code.
stock LightThrowError(E_LIGHT_ERROR:error, {Float,_}:...);

enum E_LIGHT_ERROR {
    LIGHT_ERROR_NONE,
    LIGHT_ERROR_EXPECT_BUT_FOUND,
    LIGHT_ERROR_INVALID_EXPR,
    LIGHT_ERROR_INVALID_STMT,
    LIGHT_ERROR_LIGHT_MAX_VARS,
    LIGHT_ERROR_VAR_NOT_FOUND,
    LIGHT_ERROR_NATIVE_NOT_FOUND,
    LIGHT_ERROR_CALL_NUM_ARGS,
}

static enum E_FLOW_STATE {
    FLOW_STATE_HALT,
    FLOW_STATE_CONTINUE,
}

#define PROPAGATE_FLOW_STATE(%0) \
if ((%0) == FLOW_STATE_HALT) return FLOW_STATE_HALT

#define FNV_OFFSET_BASIS 0x811C9DC5
#define FNV_PRIME 0x01000193

static stock HashStringUTF8(const string[]) {
    new hash = FNV_OFFSET_BASIS;

    for (new i; string[i] != EOS; ++i) {
        hash ^= string[i];
        hash *= FNV_PRIME;
    }

    return hash;
}

#pragma naked
static stock CallFunctionFromAddr(fnAddress, ...) {
    #pragma unused fnAddress

    // Get the return address and save it into FRM.
    __emit pop.pri;
    __emit sctrl 5;

    // Get the arguments size, subtract 4 (one cell) and save the result into PRI.
    __emit pop.pri;
    __emit add.c -4;

    // Pop `fnAddress` into ALT.
    __emit pop.alt;

    // Push the arguments size.
    __emit push.pri;

    // Push the return address.
    __emit lctrl 5;
    __emit push.pri;

    // Move the value of PRI (`fnAddress`) to CIP.
    __emit move.pri;
    __emit sctrl 6;

    // Avoid the warning 209.
    // "function ... should return a value"
    return 0;
}

/* -------------------------------- Lexer -------------------------------- */

// NOTE: Any modifcation made here must also be made in `PRECEDENCE_TABLE`.
enum {
    LIGHT_TOKEN_EOF,
    LIGHT_TOKEN_IDENTIFIER,
    LIGHT_TOKEN_NUMBER,

    LIGHT_TOKEN_PLUS,     ///< +
    LIGHT_TOKEN_MINUS,    ///< -
    LIGHT_TOKEN_STAR,     ///< *
    LIGHT_TOKEN_SLASH,    ///< /
    LIGHT_TOKEN_AT,       ///< @
    LIGHT_TOKEN_DOLLAR,   ///< $
    LIGHT_TOKEN_SEMI,     ///< ;
    LIGHT_TOKEN_EQ,       ///< =
    LIGHT_TOKEN_LPAR,     ///< (
    LIGHT_TOKEN_RPAR,     ///< )
    LIGHT_TOKEN_COMMA,    ///< ,
}

static stock s_LexerCode[LIGHT_MAX_CODE_LENGTH];
static stock s_LexerCodeIndex;
static stock s_LexerToken;
static stock s_LexerIdentifier[LIGHT_MAX_IDENTIFIER_LEN + 1];
static stock s_LexerIdentifierLength;
static stock s_LexerIdentifierHash;
static stock s_LexerNumber;

static stock bool:is_digit(chr) {
    return '0' <= chr <= '9';
}

static stock bool:is_alpha(chr) {
    return ('a' <= chr <= 'z') || ('A' <= chr <= 'Z');
}

static stock bool:is_alnum(chr) {
    return is_alpha(chr) || is_digit(chr);
}

static stock LexerInit() {
    s_LexerCode[0] = 0;
    s_LexerCodeIndex = 0;
    s_LexerToken = LIGHT_TOKEN_EOF;
    s_LexerIdentifierHash = 0;
    s_LexerNumber = 0;
}

static stock LexerSkipWhitespace() {
    for (;;) {
        switch (s_LexerCode[s_LexerCodeIndex]) {
            case ' ', '\n', '\t', '\r': {
                ++s_LexerCodeIndex;
                continue;
            }

            default: break;
        }
    }
}

static stock bool:LexerParseOneCharToken(&token) {
    switch (s_LexerCode[s_LexerCodeIndex]) {
        case '+': token = LIGHT_TOKEN_PLUS;
        case '-': token = LIGHT_TOKEN_MINUS;
        case '*': token = LIGHT_TOKEN_STAR;
        case '/': token = LIGHT_TOKEN_SLASH;
        case '@': token = LIGHT_TOKEN_AT;
        case '$': token = LIGHT_TOKEN_DOLLAR;
        case '=': token = LIGHT_TOKEN_EQ;
        case ';': token = LIGHT_TOKEN_SEMI;
        case '(': token = LIGHT_TOKEN_LPAR;
        case ')': token = LIGHT_TOKEN_RPAR;
        case ',': token = LIGHT_TOKEN_COMMA;

        default:
            return false;
    }

    ++s_LexerCodeIndex;

    return true;
}

static stock bool:LexerParseNumber(&number) {
    if (is_digit(s_LexerCode[s_LexerCodeIndex]) == false)
        return false;

    number = s_LexerCode[s_LexerCodeIndex++] - '0';

    while (is_digit(s_LexerCode[s_LexerCodeIndex])) {
        number *= 10;
        number += s_LexerCode[s_LexerCodeIndex++] - '0';
    }

    return true;
}

static stock bool:LexerParseIdentifier() {
    if (is_alpha(s_LexerCode[s_LexerCodeIndex]) == false)
        return false;

    s_LexerIdentifierLength = 0;
    s_LexerIdentifierHash = FNV_OFFSET_BASIS;

    new chr = s_LexerCode[s_LexerCodeIndex];

    while (is_alnum(chr) || chr == '_') {
        s_LexerIdentifier[s_LexerIdentifierLength++] = chr;

        s_LexerIdentifierHash ^= chr;
        s_LexerIdentifierHash *= FNV_PRIME;

        chr = s_LexerCode[++s_LexerCodeIndex];
    }

    s_LexerIdentifier[s_LexerIdentifierLength] = EOS;

    return true;
}

static stock bool:LexerAdvance() {
    LexerSkipWhitespace();

    if (s_LexerCode[s_LexerCodeIndex] == EOS) {
        s_LexerToken = LIGHT_TOKEN_EOF;
        return false;
    }

    if (LexerParseOneCharToken(s_LexerToken))
        return true;

    if (LexerParseNumber(s_LexerNumber)) {
        s_LexerToken = LIGHT_TOKEN_NUMBER;
        return true;
    }

    if (LexerParseIdentifier()) {
        s_LexerToken = LIGHT_TOKEN_IDENTIFIER;
        return true;
    }

    return false;
}

static stock E_FLOW_STATE:LexerMatch(expected) {
    if (s_LexerToken != expected) {
        LightThrowError(LIGHT_ERROR_EXPECT_BUT_FOUND, expected, s_LexerToken);
        return FLOW_STATE_HALT;
    }

    LexerAdvance();

    return FLOW_STATE_CONTINUE;
}

/* -------------------------------- Variables -------------------------------- */

static stock s_Variables[LIGHT_MAX_VARIABLES * 2];
static stock s_VariablesCount;

/// LightVariableSetValue(variableIndex, value) -> void
#define LightVariableSetValue(%0,%1) s_Variables[(%0) * 2 + 1] = (%1)

/// LightVariableGetValue(variableIndex) -> cell
#define LightVariableGetValue(%0) (s_Variables[(%0) * 2 + 1])

stock LightVariableClear() {
    for (new i; i < s_VariablesCount; ++i)
        LightVariableSetValue(i, 0);

    s_VariablesCount = 0;
}

/// Creates a new variable with a specified value and returns its index
/// or -1 if the limit has been reached.
stock LightVariableCreate(key, value) {
    if (s_VariablesCount >= LIGHT_MAX_VARIABLES)
        return -1;

    s_Variables[s_VariablesCount * 2 + 0] = key;
    s_Variables[s_VariablesCount * 2 + 1] = value;
    s_VariablesCount += 1;

    return s_VariablesCount - 1;
}

/// Returns the variable index or -1 if it was not found.
stock LightVariableGet(key) {
    for (new i; i < s_VariablesCount; ++i) {
        if (s_Variables[i * 2] != key)
            continue;

        return i;
    }

    return -1;
}

/* -------------------------------- Native Functions --------------------------------  */

static enum E_NATIVE {
    NATIVE_HASH,
    NATIVE_ADDR,
    NATIVE_PARMS_COUNT
}

static stock s_Natives[LIGHT_MAX_NATIVES][E_NATIVE];
static stock s_NativesCount;

/// Returns false if the limit of native functions has been reached; otherwise true.
stock bool:LightRegisterNative(const name[], addr, parmsCount) {
    if (s_NativesCount >= LIGHT_MAX_NATIVES)
        return false;

    s_Natives[s_NativesCount][NATIVE_HASH] = HashStringUTF8(name);
    s_Natives[s_NativesCount][NATIVE_ADDR] = addr;
    s_Natives[s_NativesCount][NATIVE_PARMS_COUNT] = parmsCount;

    s_NativesCount += 1;

    return true;
}

static stock NativeGet(hash) {
    for (new i; i < s_NativesCount; ++i) {
        if (hash == s_Natives[i][NATIVE_HASH])
            return i;
    }

    return -1;
}

/* -------------------------------- Parser & ByteCode Builder --------------------------------  */

static enum {
    OPCODE_HALT,        ///< Halts the program executation.
    OPCODE_CONST,       ///< Pushes a constant value onto the stack.
    OPCODE_SYS,         ///< Calls a native function and pushes the return value onto the stack.
    OPCODE_GET,         ///< Gets the value of a variable.
    OPCODE_SET,         ///< Sets the value of a variable.

    /// # Binary Operations
    /// These instructions pop 2 cells from the stack, perform the operation
    /// and push the result onto the stack.

    OPCODE_ADD,
    OPCODE_SUB,
    OPCODE_MUL,
    OPCODE_DIV,
}

static const stock PRECEDENCE_TABLE[] = {
    0,  // LIGHT_TOKEN_EOF
    0,  // LIGHT_TOKEN_IDENTIFIER
    0,  // LIGHT_TOKEN_NUMBER
    1,  // LIGHT_TOKEN_PLUS
    1,  // LIGHT_TOKEN_MINUS
    2,  // LIGHT_TOKEN_STAR
    2,  // LIGHT_TOKEN_SLASH
    0,  // LIGHT_TOKEN_AT
    0,  // LIGHT_TOKEN_DOLLAR
    0,  // LIGHT_TOKEN_SEMI
    0,  // LIGHT_TOKEN_EQ
    0,  // LIGHT_TOKEN_LPAR
    0,  // LIGHT_TOKEN_RPAR
    0   // LIGHT_TOKEN_COMMA
};

static stock E_FLOW_STATE:ParserExpr(byteCode[], &byteCodeLength, minPrecedence);

static stock E_FLOW_STATE:ParserCall(byteCode[], &byteCodeLength) {
    // Eat @
    LexerAdvance();

    if (s_LexerToken != LIGHT_TOKEN_IDENTIFIER) {
        LightThrowError(LIGHT_ERROR_EXPECT_BUT_FOUND, LIGHT_TOKEN_IDENTIFIER, s_LexerToken);
        return FLOW_STATE_HALT;
    }

    new nativeIndex = NativeGet(s_LexerIdentifierHash);

    if (nativeIndex == -1) {
        LightThrowError(LIGHT_ERROR_NATIVE_NOT_FOUND, s_LexerIdentifier);
        return FLOW_STATE_HALT;
    }

    LexerAdvance();

    PROPAGATE_FLOW_STATE(LexerMatch(LIGHT_TOKEN_LPAR));

    new argsCount;

    for (;;) {
        if (s_LexerToken == LIGHT_TOKEN_RPAR)
            break;

        argsCount += 1;

        PROPAGATE_FLOW_STATE(ParserExpr(byteCode, byteCodeLength, 1));

        if (s_LexerToken != LIGHT_TOKEN_COMMA)
            break;

        LexerAdvance();
    }

    PROPAGATE_FLOW_STATE(LexerMatch(LIGHT_TOKEN_RPAR));

    if (argsCount != s_Natives[nativeIndex][NATIVE_PARMS_COUNT]) {
        LightThrowError(LIGHT_ERROR_CALL_NUM_ARGS, s_LexerIdentifier);
        return FLOW_STATE_HALT;
    }

    byteCode[byteCodeLength++] = OPCODE_SYS;
    byteCode[byteCodeLength++] = nativeIndex;

    return FLOW_STATE_CONTINUE;
}

static stock E_FLOW_STATE:ParserVariableAssign(byteCode[], &byteCodeLength) {
    // Eat $
    LexerAdvance();

    if (s_LexerToken != LIGHT_TOKEN_IDENTIFIER) {
        LightThrowError(LIGHT_ERROR_EXPECT_BUT_FOUND, LIGHT_TOKEN_IDENTIFIER, s_LexerToken);
        return FLOW_STATE_HALT;
    }

    new identifierHash = s_LexerIdentifierHash;
    new variableIndex = LightVariableGet(identifierHash);

    LexerAdvance();

    if (s_LexerToken != LIGHT_TOKEN_EQ) {
        LightThrowError(LIGHT_ERROR_INVALID_STMT, s_LexerToken);
        return FLOW_STATE_HALT;
    }

    if (variableIndex == -1) {
        variableIndex = LightVariableCreate(identifierHash, 0);

        if (variableIndex == -1) {
            LightThrowError(LIGHT_ERROR_LIGHT_MAX_VARS);
            return FLOW_STATE_HALT;
        }
    }

    LexerAdvance();

    PROPAGATE_FLOW_STATE(ParserExpr(byteCode, byteCodeLength, 1));

    byteCode[byteCodeLength++] = OPCODE_SET;
    byteCode[byteCodeLength++] = variableIndex;

    return FLOW_STATE_CONTINUE;
}

static stock E_FLOW_STATE:ParserExpr(byteCode[], &byteCodeLength, minPrecedence) {
    if (s_LexerToken == LIGHT_TOKEN_NUMBER) {
        byteCode[byteCodeLength++] = OPCODE_CONST;
        byteCode[byteCodeLength++] = s_LexerNumber;

        LexerAdvance();
    }
    else if (s_LexerToken == LIGHT_TOKEN_DOLLAR) {
        // Eat $
        LexerAdvance();

        if (s_LexerToken != LIGHT_TOKEN_IDENTIFIER) {
            LightThrowError(LIGHT_ERROR_EXPECT_BUT_FOUND, LIGHT_TOKEN_IDENTIFIER, s_LexerToken);
            return FLOW_STATE_HALT;
        }

        static identifier[LIGHT_MAX_IDENTIFIER_LEN + 1];
        strcopy(identifier, s_LexerIdentifier);

        new identifierHash = s_LexerIdentifierHash;
        new variableIndex = LightVariableGet(identifierHash);

        LexerAdvance();

        if (s_LexerToken == LIGHT_TOKEN_EQ) {
            LightThrowError(LIGHT_ERROR_INVALID_EXPR, s_LexerToken);
            return FLOW_STATE_HALT;
        }

        if (variableIndex == -1) {
            LightThrowError(LIGHT_ERROR_VAR_NOT_FOUND, identifier);
            return FLOW_STATE_HALT;
        }

        byteCode[byteCodeLength++] = OPCODE_GET;
        byteCode[byteCodeLength++] = variableIndex;
    }
    else if (s_LexerToken == LIGHT_TOKEN_AT) {
        PROPAGATE_FLOW_STATE(ParserCall(byteCode, byteCodeLength));
    }

    new precedence = PRECEDENCE_TABLE[s_LexerToken];

    while (precedence >= minPrecedence) {
        new token = s_LexerToken;
        LexerAdvance();

        PROPAGATE_FLOW_STATE(ParserExpr(byteCode, byteCodeLength, precedence + 1));

        switch (token) {
            case LIGHT_TOKEN_PLUS: byteCode[byteCodeLength++] = OPCODE_ADD;
            case LIGHT_TOKEN_MINUS: byteCode[byteCodeLength++] = OPCODE_SUB;
            case LIGHT_TOKEN_STAR: byteCode[byteCodeLength++] = OPCODE_MUL;
            case LIGHT_TOKEN_SLASH: byteCode[byteCodeLength++] = OPCODE_DIV;
        }

        precedence = PRECEDENCE_TABLE[s_LexerToken];
    }

    return FLOW_STATE_CONTINUE;
}

static stock E_FLOW_STATE:ParserStmt(byteCode[], &byteCodeLength) {
    if (s_LexerToken == LIGHT_TOKEN_DOLLAR) {
        PROPAGATE_FLOW_STATE(ParserVariableAssign(byteCode, byteCodeLength));
    }
    else {
        PROPAGATE_FLOW_STATE(ParserExpr(byteCode, byteCodeLength, 1));
    }

    PROPAGATE_FLOW_STATE(LexerMatch(LIGHT_TOKEN_SEMI));

    return FLOW_STATE_CONTINUE;
}

stock bool:LightCompile(const code[], byteCode[]) {
    new byteCodeLength;

    LexerInit();
    strcopy(s_LexerCode, code);

    LexerAdvance();

    while (s_LexerToken != LIGHT_TOKEN_EOF) {
        if (ParserStmt(byteCode, byteCodeLength) == FLOW_STATE_HALT)
            return false;
    }

    byteCode[byteCodeLength] = OPCODE_HALT;

    return true;
}

/* -------------------------------- Virtual Machine -------------------------------- */

stock LightExecute(const byteCode[]) {
    static stack[LIGHT_STACK_SIZE];

    new ip;
    new sp;

    for (;;) {
        new opcode = byteCode[ip++];

        switch (opcode) {
            case OPCODE_HALT:
                break;

            case OPCODE_CONST:
                stack[sp++] = byteCode[ip++];

            case OPCODE_SYS: {
                new nativeIndex = byteCode[ip++];

                sp -= s_Natives[nativeIndex][NATIVE_PARMS_COUNT];

                stack[sp] = CallFunctionFromAddr(
                    s_Natives[nativeIndex][NATIVE_ADDR],
                    stack[sp]
                );

                ++sp;
            }

            case OPCODE_GET:
                stack[sp++] = LightVariableGetValue(byteCode[ip++]);

            case OPCODE_SET:
                LightVariableSetValue(byteCode[ip++], stack[--sp]);

            case OPCODE_ADD: {
                --sp;
                stack[sp - 1] += stack[sp];
            }

            case OPCODE_SUB: {
                --sp;
                stack[sp - 1] -= stack[sp];
            }

            case OPCODE_MUL: {
                --sp;
                stack[sp - 1] *= stack[sp];
            }

            case OPCODE_DIV: {
                --sp;
                stack[sp - 1] /= stack[sp];
            }
        }
    }

    if (sp > 0)
        return stack[sp - 1];

    return 0;
}

#undef FNV_OFFSET_BASIS
#undef FNV_PRIME
#undef PROPAGATE_FLOW_STATE
