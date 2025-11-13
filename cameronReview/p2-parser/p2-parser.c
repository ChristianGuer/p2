/**
 * @file p2-parser.c
 * @brief Compiler phase 2: parser
 * @name cameron sateri
 * I used copilot for this project
 */

#include "p2-parser.h"

/*
 * helper functions
 */

/**
 * @brief Look up the source line of the next token in the queue.
 * 
 * @param input Token queue to examine
 * @returns Source line
 */
int get_next_token_line (TokenQueue* input)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input\n");
    }
    return TokenQueue_peek(input)->line;
}

/**
 * @brief Check next token for a particular type and text and discard it
 * 
 * Throws an error if there are no more tokens or if the next token in the
 * queue does not match the given type or text.
 * 
 * @param input Token queue to modify
 * @param type Expected type of next token
 * @param text Expected text of next token
 */
void match_and_discard_next_token (TokenQueue* input, TokenType type, const char* text)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected \'%s\')\n", text);
    }
    Token* token = TokenQueue_remove(input);
    if (token->type != type || !token_str_eq(token->text, text)) {
        Error_throw_printf("Expected \'%s\' but found '%s' on line %d\n",
                text, token->text, get_next_token_line(input));
    }
    Token_free(token);
}

/**
 * @brief Remove next token from the queue
 * 
 * Throws an error if there are no more tokens.
 * 
 * @param input Token queue to modify
 */
void discard_next_token (TokenQueue* input)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input\n");
    }
    Token_free(TokenQueue_remove(input));
}

/**
 * @brief Look ahead at the type of the next token
 * 
 * @param input Token queue to examine
 * @param type Expected type of next token
 * @returns True if the next token is of the expected type, false if not
 */
bool check_next_token_type (TokenQueue* input, TokenType type)
{
    if (TokenQueue_is_empty(input)) {
        return false;
    }
    Token* token = TokenQueue_peek(input);
    return (token->type == type);
}

/**
 * @brief Look ahead at the type and text of the next token
 * 
 * @param input Token queue to examine
 * @param type Expected type of next token
 * @param text Expected text of next token
 * @returns True if the next token is of the expected type and text, false if not
 */
bool check_next_token (TokenQueue* input, TokenType type, const char* text)
{
    if (TokenQueue_is_empty(input)) {
        return false;
    }
    Token* token = TokenQueue_peek(input);
    return (token->type == type) && (token_str_eq(token->text, text));
}

/**
 * @brief Parse and return a Decaf type
 * 
 * @param input Token queue to modify
 * @returns Parsed type (it is also removed from the queue)
 */
DecafType parse_type (TokenQueue* input)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected type)\n");
    }
    Token* token = TokenQueue_remove(input);
    if (token->type != KEY) {
        Error_throw_printf("Invalid type '%s' on line %d\n", token->text, get_next_token_line(input));
    }
    DecafType t = VOID;
    if (token_str_eq("int", token->text)) {
        t = INT;
    } else if (token_str_eq("bool", token->text)) {
        t = BOOL;
    } else if (token_str_eq("void", token->text)) {
        t = VOID;
    } else {
        Error_throw_printf("Invalid type '%s' on line %d\n", token->text, get_next_token_line(input));
    }
    Token_free(token);
    return t;
}

/**
 * @brief Parse and return a Decaf identifier
 * 
 * @param input Token queue to modify
 * @param buffer String buffer for parsed identifier (should be at least
 * @c MAX_TOKEN_LEN characters long)
 */
void parse_id (TokenQueue* input, char* buffer)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected identifier)\n");
    }
    Token* token = TokenQueue_remove(input);
    if (token->type != ID) {
        Error_throw_printf("Invalid ID '%s' on line %d\n", token->text, get_next_token_line(input));
    }
    snprintf(buffer, MAX_ID_LEN, "%s", token->text);
    Token_free(token);
}

/*
 * node-level parsing functions
 */

// Lab 04 step 2.0
ASTNode* parse_vardecl (TokenQueue* input)
{
    // Error checking
    if (input == NULL) {
        Error_throw_printf("TokenQueue is NULL\n");
    }

    // Lab 04 step 2.1
    DecafType type = parse_type(input);

    // Lab 04 step 2.2
    char name[MAX_ID_LEN];
    parse_id(input, name);

    // Lab 04 step 2.3
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, SYM, ";");

    // Lab 04 step 2.4
    return VarDeclNode_new(name, type, false, 1, line);
}

// location for references and assignments
ASTNode* parse_location(TokenQueue* input)
{
    char name[MAX_ID_LEN];
    parse_id(input, name);
    int line = get_next_token_line(input);
    return LocationNode_new(name, NULL, line);
}

// map string symbol to BinaryOpType
BinaryOpType symbol_to_binop(const char* sym) {
    if (strcmp(sym, "+") == 0) return ADDOP;
    if (strcmp(sym, "-") == 0) return SUBOP;
    if (strcmp(sym, "*") == 0) return MULOP;
    if (strcmp(sym, "/") == 0) return DIVOP;
    if (strcmp(sym, "%") == 0) return MODOP;
    if (strcmp(sym, "||") == 0) return OROP;
    if (strcmp(sym, "&&") == 0) return ANDOP;
    if (strcmp(sym, "==") == 0) return EQOP;
    if (strcmp(sym, "!=") == 0) return NEQOP;
    if (strcmp(sym, "<") == 0) return LTOP;
    if (strcmp(sym, "<=") == 0) return LEOP;
    if (strcmp(sym, ">=") == 0) return GEOP;
    if (strcmp(sym, ">") == 0) return GTOP;
    return -1; // error case
}

// parse expressions case (c level testing)
ASTNode* parse_expr(TokenQueue* input) {
    if (TokenQueue_is_empty(input)) { // error checking
        Error_throw_printf("Unexpected end of input\n");
    }

    // save next token to look ahead
    Token* token = TokenQueue_peek(input);
    int line = get_next_token_line(input);
    ASTNode* left = NULL;
    if (token->type == SYM && token_str_eq(token->text, "(")) { // parenthesis case (another expression)
        match_and_discard_next_token(input, SYM, "(");
        left = parse_expr(input);
        match_and_discard_next_token(input, SYM, ")");
    } else if (token->type == DECLIT) { // decimal literal case
        token = TokenQueue_remove(input);
        left = LiteralNode_new_int(atoi(token->text), line);
        Token_free(token);
    } else if (token->type == HEXLIT) { // hex literal case
        token = TokenQueue_remove(input);
        int value = (int)strtol(token->text, NULL, 16);
        left = LiteralNode_new_int(value, line);
        Token_free(token);
    } else if (token->type == STRLIT) { // string literal case
        token = TokenQueue_remove(input);
        const char* raw = token->text;
        char buf[MAX_TOKEN_LEN];
        if (strlen(raw) >= 2 && raw[0] == '"' && raw[strlen(raw) - 1] == '"') { // remove surrounding quotes
            snprintf(buf, sizeof(buf), "%.*s", (int)(strlen(raw) - 2), raw + 1);
            left = LiteralNode_new_string(buf, line);
        } else {
            left = LiteralNode_new_string(raw, line);
        }
        Token_free(token);
    } else if (token->type == ID) { // identifier case
        left = parse_location(input);
    } else { // error case
        Error_throw_printf("Unsupported expression starting with '%s' on line %d\n", token->text, line);
    }

    // binary operator case
    if (!TokenQueue_is_empty(input)) {
        Token* op_token = TokenQueue_peek(input);
        if (op_token->type == SYM &&
            (token_str_eq(op_token->text, "+") || token_str_eq(op_token->text, "-") || token_str_eq(op_token->text, "*") ||
             token_str_eq(op_token->text, "/") || token_str_eq(op_token->text, "%") || token_str_eq(op_token->text, "||") ||
             token_str_eq(op_token->text, "&&") || token_str_eq(op_token->text, "==") || token_str_eq(op_token->text, "!=") ||
             token_str_eq(op_token->text, "<") || token_str_eq(op_token->text, "<=") || token_str_eq(op_token->text, ">=") ||
             token_str_eq(op_token->text, ">"))) { // check next token for binary operator
            op_token = TokenQueue_remove(input);
            ASTNode* right = parse_expr(input);
            BinaryOpType op_type = symbol_to_binop(op_token->text);
            if (op_type == -1) { // error case from helper function
                Error_throw_printf("Unsupported binary operator '%s' on line %d\n", op_token->text, line);
            }
            ASTNode* binop = BinaryOpNode_new(op_type, left, right, line);
            Token_free(op_token);
            return binop;
        }
    }
    return left;
}

// parse statements case (c level testing)
ASTNode* parse_stmt(TokenQueue* input) {
    if (input == NULL) { // error checking
        Error_throw_printf("TokenQueue is NULL\n");
    }

    int line = get_next_token_line(input);
    if (check_next_token_type(input, ID)) { // identifier case
        ASTNode* location = parse_location(input);
        match_and_discard_next_token(input, SYM, "=");
        ASTNode* value = parse_expr(input);
        match_and_discard_next_token(input, SYM, ";");
        return AssignmentNode_new(location, value, line);
    }
    // keyword cases
    if (check_next_token(input, KEY, "break")) { // break case
        match_and_discard_next_token(input, KEY, "break");
        match_and_discard_next_token(input, SYM, ";");
        return BreakNode_new(line);
    } else if (check_next_token(input, KEY, "continue")) { // continue case
        match_and_discard_next_token(input, KEY, "continue");
        match_and_discard_next_token(input, SYM, ";");
        return ContinueNode_new(line);
    } else if (check_next_token(input, KEY, "return")) { // return case
        match_and_discard_next_token(input, KEY, "return");
        ASTNode* value = NULL;
        if (!check_next_token(input, SYM, ";")) { // return identifier or expression
            value = parse_expr(input);
        }
        match_and_discard_next_token(input, SYM, ";");
        return ReturnNode_new(value, line);
    }
    // error case
    Error_throw_printf("Unknown statement at line %d\n", line);
    return NULL;
}

// parse block case (c level testing)
// although I don't think this needs to be changed much, if at all, for other testing levels :)
ASTNode* parse_block(TokenQueue* input) {
    if (input == NULL) { // error checking
        Error_throw_printf("TokenQueue is NULL\n");
    }
    
    // start block
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, SYM, "{");
    NodeList* decls = NodeList_new();
    NodeList* stmts = NodeList_new();

    // type checker
    while (check_next_token(input, KEY, "int") || check_next_token(input, KEY, "bool") || check_next_token(input, KEY, "void")) {
        NodeList_add(decls, parse_vardecl(input));
    }
    // expressions until end of block
    while (!check_next_token(input, SYM, "}")) {
        NodeList_add(stmts, parse_stmt(input));
    }
    match_and_discard_next_token(input, SYM, "}");
    return BlockNode_new(decls, stmts, line);
}

// parse function declaration case (c level testing)
ASTNode* parse_funcdecl(TokenQueue* input) {
    if (input == NULL) {
        Error_throw_printf("TokenQueue is NULL\n");
    }

    // parse function declaration
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, KEY, "def");

    // return type and function name
    DecafType rettype = parse_type(input);
    char name[MAX_ID_LEN];
    parse_id(input, name);

    // parameters
    NodeList* params = NodeList_new();
    match_and_discard_next_token(input, SYM, "(");
    if (!check_next_token(input, SYM, ")")) { // check for parameters
        do { // loop through parameters
            DecafType param_type = parse_type(input);
            char param_name[MAX_ID_LEN];
            parse_id(input, param_name);
            NodeList_add(params, VarDeclNode_new(param_name, param_type, false, 0, get_next_token_line(input)));
            if (check_next_token(input, SYM, ",")) {
                match_and_discard_next_token(input, SYM, ",");
            } else { // no more parameters
                break;
            }
        } while (1);
    }
    match_and_discard_next_token(input, SYM, ")");
    
    // start block
    ASTNode* block = parse_block(input);
    return FuncDeclNode_new(name, rettype, (ParameterList*)params, block, line);
}

// parse program (c level testing)
ASTNode* parse_program (TokenQueue* input)
{
    if (input == NULL) { // error checking
        Error_throw_printf("TokenQueue is NULL\n");
    }

    // variables and functions
    NodeList* vars = NodeList_new();
    NodeList* funcs = NodeList_new();

    // loop through all declarations
    while (!TokenQueue_is_empty(input)) {
        Token* next_token = TokenQueue_peek(input);
        if (next_token->type == KEY && token_str_eq(next_token->text, "def")) { // function declaration case
            NodeList_add(funcs, parse_funcdecl(input));
        } else if (next_token->type == KEY &&
                   (token_str_eq(next_token->text, "int") || token_str_eq(next_token->text, "bool") ||
                    token_str_eq(next_token->text, "void"))) { // variable declaration case
            NodeList_add(vars, parse_vardecl(input));
        } else { // error case
            Error_throw_printf("Expected keyword at top level, but found '%s' (type=%d) on line %d\n", next_token->text, next_token->type, next_token->line);
        }
    }
    return ProgramNode_new(vars, funcs);
}

// enter from main file
ASTNode* parse (TokenQueue* input)
{
    if (input == NULL) { // error checking
        Error_throw_printf("TokenQueue is NULL\n");
    }
    return parse_program(input);
}
