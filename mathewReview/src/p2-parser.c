/**
 * @file p2-parser.c
 * @brief Compiler phase 2: parser
 * Author: Matthew Murphy
 * I used Ai a lot on this project:
 *      I did the shell code on my own and wrote out some of the earlier parsers.
 *      Once that groundwork was set I used Ai to design and write out the rest of the parsers.
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
    if (input == NULL || TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input\n");
    }
    Token* token = TokenQueue_peek(input);
    if (token == NULL) {
        Error_throw_printf("Unexpected end of input\n");
    }
    return token->line;
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
    if (token == NULL) {
        Error_throw_printf("Unexpected end of input (expected \'%s\')\n", text);
    }
    if (token->type != type || !token_str_eq(token->text, text)) {
        Error_throw_printf("Expected \'%s\' but found '%s' on line %d\n",
                text, token->text, token->line);
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
 * @brief Check if a token represents a type (int, bool, void)
 * 
 * @param token Token to check
 * @returns True if the token is a type keyword, false if not
 */
bool is_type_token (Token* token)
{
    if (token == NULL || token->type != KEY) {
        return false;
    }
    return token_str_eq(token->text, "int") ||
           token_str_eq(token->text, "bool") ||
           token_str_eq(token->text, "void");
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
    if (token == NULL) {
        Error_throw_printf("Unexpected end of input (expected type)\n");
    }
    if (token->type != KEY) {
        Error_throw_printf("Invalid type '%s' on line %d\n", token->text, token->line);
    }
    DecafType t = VOID;
    if (token_str_eq("int", token->text)) {
        t = INT;
    } else if (token_str_eq("bool", token->text)) {
        t = BOOL;
    } else if (token_str_eq("void", token->text)) {
        t = VOID;
    } else {
        Error_throw_printf("Invalid type '%s' on line %d\n", token->text, token->line);
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
    if (token == NULL) {
        Error_throw_printf("Unexpected end of input (expected identifier)\n");
    }
    if (token->type != ID) {
        Error_throw_printf("Invalid ID '%s' on line %d\n", token->text, token->line);
    }
    snprintf(buffer, MAX_ID_LEN, "%s", token->text);
    Token_free(token);
}

/*
 * node-level parsing functions
 */

/* Forward declaration */
ASTNode* parse_block (TokenQueue* input);
ASTNode* parse_stmt (TokenQueue* input);
ASTNode* parse_if_stmt (TokenQueue* input);
ASTNode* parse_while_stmt (TokenQueue* input);
ASTNode* parse_return_stmt (TokenQueue* input);
ASTNode* parse_assign_stmt (TokenQueue* input);
ASTNode* parse_expr_stmt (TokenQueue* input);
ASTNode* parse_expr (TokenQueue* input);
ASTNode* parse_or_expr (TokenQueue* input);
ASTNode* parse_and_expr (TokenQueue* input);
ASTNode* parse_equality_expr (TokenQueue* input);
ASTNode* parse_relational_expr (TokenQueue* input);
ASTNode* parse_add_expr (TokenQueue* input);
ASTNode* parse_mul_expr (TokenQueue* input);
ASTNode* parse_unary_expr (TokenQueue* input);
ASTNode* parse_primary_expr (TokenQueue* input);
ASTNode* parse_literal (TokenQueue* input);
ASTNode* parse_location (TokenQueue* input);

// ex def int main() { return 0; }
ASTNode* parse_program (TokenQueue* input)
{
    if (input == NULL) {
        Error_throw_printf("Cannot parse program from NULL token queue\n");
    }
    
    NodeList* vars = NodeList_new();
    NodeList* funcs = NodeList_new();
    
    while (!TokenQueue_is_empty(input)) {
        Token* first = TokenQueue_peek(input);
        
        if (check_next_token(input, KEY, "def")) {
            // Function declaration: def type name(params) { body }
            int line = get_next_token_line(input);
            discard_next_token(input); // consume "def"
            DecafType return_type = parse_type(input);
            char name[MAX_TOKEN_LEN];
            parse_id(input, name);
            
            match_and_discard_next_token(input, SYM, "(");
            ParameterList* params = ParameterList_new();
            
            if (!check_next_token(input, SYM, ")")) {
                // Parse parameter list
                do {
                    DecafType param_type = parse_type(input);
                    char param_name[MAX_TOKEN_LEN];
                    parse_id(input, param_name);
                    
                    // Handle array parameters: type name[]
                    if (check_next_token(input, SYM, "[")) {
                        match_and_discard_next_token(input, SYM, "[");
                        match_and_discard_next_token(input, SYM, "]");
                        // For now, treat array parameters the same as regular parameters
                        // since the AST doesn't distinguish them
                    }
                    
                    ParameterList_add_new(params, param_name, param_type);
                } while (check_next_token(input, SYM, ",") && (discard_next_token(input), true));
            }
            
            match_and_discard_next_token(input, SYM, ")");
            ASTNode* body = parse_block(input);
            ASTNode* func = FuncDeclNode_new(name, return_type, params, body, line);
            NodeList_add(funcs, func);
        } else if (is_type_token(first)) {
            // Variable declaration: type name; or type name[size];
            int line = get_next_token_line(input);
            DecafType type = parse_type(input);
            
            // Variables cannot be of type void
            if (type == VOID) {
                Error_throw_printf("Variables cannot be of type void on line %d\n", line);
            }
            
            char name[MAX_TOKEN_LEN];
            parse_id(input, name);
            
            bool is_array = false;
            int array_length = 1;
            
            if (check_next_token(input, SYM, "[")) {
                is_array = true;
                match_and_discard_next_token(input, SYM, "[");
                if (check_next_token_type(input, DECLIT) || check_next_token_type(input, HEXLIT)) {
                    Token* size_token = TokenQueue_remove(input);
                    if (size_token->type == DECLIT) {
                        array_length = atoi(size_token->text);
                    } else { // HEXLIT
                        array_length = (int)strtol(size_token->text, NULL, 16);
                    }
                    
                    // Array size must be greater than 0
                    if (array_length <= 0) {
                        Error_throw_printf("Array size must be greater than 0 on line %d\n", get_next_token_line(input));
                    }
                    
                    Token_free(size_token);
                } else {
                    Error_throw_printf("Expected array size on line %d\n", get_next_token_line(input));
                }
                match_and_discard_next_token(input, SYM, "]");
            }
            
            match_and_discard_next_token(input, SYM, ";");
            ASTNode* var = VarDeclNode_new(name, type, is_array, array_length, line);
            NodeList_add(vars, var);
        } else {
            Error_throw_printf("Expected 'def' or type declaration on line %d\n", first->line);
        }
    }

    return ProgramNode_new(vars, funcs);
}





// ==== Blocks & Declarations ====

ASTNode* parse_block(TokenQueue* input)
{
    int source_line = get_next_token_line(input);
    match_and_discard_next_token(input, SYM, "{");
    NodeList* decls = NodeList_new();
    NodeList* stmts = NodeList_new();
    
    while (!TokenQueue_is_empty(input) && !check_next_token(input, SYM, "}")) {
        Token* next = TokenQueue_peek(input);
        if (is_type_token(next)) {
            // Variable declaration
            int var_line = next->line;
            DecafType type = parse_type(input);
            
            // Variables cannot be of type void
            if (type == VOID) {
                Error_throw_printf("Variables cannot be of type void on line %d\n", var_line);
            }
            
            char name[MAX_TOKEN_LEN];
            parse_id(input, name);
            
            bool is_array = false;
            int array_length = 1;
            
            if (check_next_token(input, SYM, "[")) {
                is_array = true;
                match_and_discard_next_token(input, SYM, "[");
                if (check_next_token_type(input, DECLIT) || check_next_token_type(input, HEXLIT)) {
                    Token* size_token = TokenQueue_remove(input);
                    if (size_token->type == DECLIT) {
                        array_length = atoi(size_token->text);
                    } else { // HEXLIT
                        array_length = (int)strtol(size_token->text, NULL, 16);
                    }
                    
                    // Array size must be greater than 0
                    if (array_length <= 0) {
                        Error_throw_printf("Array size must be greater than 0 on line %d\n", var_line);
                    }
                    
                    Token_free(size_token);
                } else {
                    Error_throw_printf("Expected array size on line %d\n", get_next_token_line(input));
                }
                match_and_discard_next_token(input, SYM, "]");
            }
            
            match_and_discard_next_token(input, SYM, ";");
            ASTNode* var = VarDeclNode_new(name, type, is_array, array_length, var_line);
            NodeList_add(decls, var);
        } else {
            ASTNode* stmt = parse_stmt(input);
            NodeList_add(stmts, stmt);
        }
    }
    
    match_and_discard_next_token(input, SYM, "}");
    return BlockNode_new(decls, stmts, source_line);
}



// ==== Statements ====

ASTNode* parse_stmt(TokenQueue* input)
{
    if (check_next_token(input, KEY, "if")) {
        return parse_if_stmt(input);
    } else if (check_next_token(input, KEY, "while")) {
        return parse_while_stmt(input);
    } else if (check_next_token(input, KEY, "return")) {
        return parse_return_stmt(input);
    } else if (check_next_token(input, KEY, "break")) {
        int line = get_next_token_line(input);
        discard_next_token(input);
        match_and_discard_next_token(input, SYM, ";");
        return BreakNode_new(line);
    } else if (check_next_token(input, KEY, "continue")) {
        int line = get_next_token_line(input);
        discard_next_token(input);
        match_and_discard_next_token(input, SYM, ";");
        return ContinueNode_new(line);
    } else if (check_next_token(input, SYM, "{")) {
        return parse_block(input);
    } else {
        // Check if this is an assignment or expression statement
        // Look ahead to see if there's an assignment operator
        Token* first = TokenQueue_peek(input);
        if (first && first->type == ID) {
            // Look for assignment pattern: ID = expr or ID[expr] = expr
            // For now, assume assignment if we see ID followed by = or [
            return parse_assign_stmt(input);
        } else {
            return parse_expr_stmt(input);
        }
    }
}

ASTNode* parse_if_stmt(TokenQueue* input)
{
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, KEY, "if");
    match_and_discard_next_token(input, SYM, "(");
    ASTNode* condition = parse_expr(input);
    match_and_discard_next_token(input, SYM, ")");
    ASTNode* if_block = parse_stmt(input);
    
    ASTNode* else_block = NULL;
    if (check_next_token(input, KEY, "else")) {
        discard_next_token(input);
        else_block = parse_stmt(input);
    }
    
    return ConditionalNode_new(condition, if_block, else_block, line);
}

ASTNode* parse_while_stmt(TokenQueue* input)
{
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, KEY, "while");
    match_and_discard_next_token(input, SYM, "(");
    ASTNode* condition = parse_expr(input);
    match_and_discard_next_token(input, SYM, ")");
    ASTNode* body = parse_stmt(input);
    
    return WhileLoopNode_new(condition, body, line);
}

ASTNode* parse_return_stmt(TokenQueue* input)
{
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, KEY, "return");
    
    ASTNode* value = NULL;
    if (!check_next_token(input, SYM, ";")) {
        value = parse_expr(input);
    }
    
    match_and_discard_next_token(input, SYM, ";");
    return ReturnNode_new(value, line);
}

ASTNode* parse_assign_stmt(TokenQueue* input)
{
    int line = get_next_token_line(input);
    ASTNode* location = parse_location(input);
    match_and_discard_next_token(input, SYM, "=");
    ASTNode* expr = parse_expr(input);
    match_and_discard_next_token(input, SYM, ";");
    
    return AssignmentNode_new(location, expr, line);
}

ASTNode* parse_expr_stmt(TokenQueue* input)
{
    ASTNode* expr = parse_expr(input);
    match_and_discard_next_token(input, SYM, ";");
    return expr;
}

// ==== Expressions (precedence climbing) ====

ASTNode* parse_expr(TokenQueue* input)
{
    return parse_or_expr(input);
}

ASTNode* parse_or_expr(TokenQueue* input)
{
    ASTNode* left = parse_and_expr(input);
    
    while (check_next_token(input, SYM, "||")) {
        int line = get_next_token_line(input);
        discard_next_token(input);
        ASTNode* right = parse_and_expr(input);
        left = BinaryOpNode_new(OROP, left, right, line);
    }
    
    return left;
}

ASTNode* parse_and_expr(TokenQueue* input)
{
    ASTNode* left = parse_equality_expr(input);
    
    while (check_next_token(input, SYM, "&&")) {
        int line = get_next_token_line(input);
        discard_next_token(input);
        ASTNode* right = parse_equality_expr(input);
        left = BinaryOpNode_new(ANDOP, left, right, line);
    }
    
    return left;
}

ASTNode* parse_equality_expr(TokenQueue* input)
{
    ASTNode* left = parse_relational_expr(input);
    
    while (check_next_token(input, SYM, "==") || check_next_token(input, SYM, "!=")) {
        int line = get_next_token_line(input);
        Token* op_token = TokenQueue_remove(input);
        BinaryOpType op = token_str_eq(op_token->text, "==") ? EQOP : NEQOP;
        Token_free(op_token);
        
        ASTNode* right = parse_relational_expr(input);
        left = BinaryOpNode_new(op, left, right, line);
    }
    
    return left;
}

ASTNode* parse_relational_expr(TokenQueue* input)
{
    ASTNode* left = parse_add_expr(input);
    
    while (check_next_token(input, SYM, "<") || check_next_token(input, SYM, "<=") ||
           check_next_token(input, SYM, ">") || check_next_token(input, SYM, ">=")) {
        int line = get_next_token_line(input);
        Token* op_token = TokenQueue_remove(input);
        BinaryOpType op;
        if (token_str_eq(op_token->text, "<")) op = LTOP;
        else if (token_str_eq(op_token->text, "<=")) op = LEOP;
        else if (token_str_eq(op_token->text, ">")) op = GTOP;
        else op = GEOP; // ">="
        Token_free(op_token);
        
        ASTNode* right = parse_add_expr(input);
        left = BinaryOpNode_new(op, left, right, line);
    }
    
    return left;
}

ASTNode* parse_add_expr(TokenQueue* input)
{
    ASTNode* left = parse_mul_expr(input);
    
    while (check_next_token(input, SYM, "+") || check_next_token(input, SYM, "-")) {
        int line = get_next_token_line(input);
        Token* op_token = TokenQueue_remove(input);
        BinaryOpType op = token_str_eq(op_token->text, "+") ? ADDOP : SUBOP;
        Token_free(op_token);
        
        ASTNode* right = parse_mul_expr(input);
        left = BinaryOpNode_new(op, left, right, line);
    }
    
    return left;
}

ASTNode* parse_mul_expr(TokenQueue* input)
{
    ASTNode* left = parse_unary_expr(input);
    
    while (check_next_token(input, SYM, "*") || check_next_token(input, SYM, "/") || check_next_token(input, SYM, "%")) {
        int line = get_next_token_line(input);
        Token* op_token = TokenQueue_remove(input);
        BinaryOpType op;
        if (token_str_eq(op_token->text, "*")) op = MULOP;
        else if (token_str_eq(op_token->text, "/")) op = DIVOP;
        else op = MODOP; // "%"
        Token_free(op_token);
        
        ASTNode* right = parse_unary_expr(input);
        left = BinaryOpNode_new(op, left, right, line);
    }
    
    return left;
}

ASTNode* parse_unary_expr(TokenQueue* input)
{
    if (check_next_token(input, SYM, "!") || check_next_token(input, SYM, "-")) {
        int line = get_next_token_line(input);
        Token* op_token = TokenQueue_remove(input);
        UnaryOpType op = token_str_eq(op_token->text, "!") ? NOTOP : NEGOP;
        Token_free(op_token);
        
        ASTNode* child = parse_unary_expr(input);
        return UnaryOpNode_new(op, child, line);
    } else {
        return parse_primary_expr(input);
    }
}

ASTNode* parse_primary_expr(TokenQueue* input)
{
    if (check_next_token(input, SYM, "(")) {
        discard_next_token(input);
        ASTNode* expr = parse_expr(input);
        match_and_discard_next_token(input, SYM, ")");
        return expr;
    } else if (check_next_token_type(input, DECLIT) || check_next_token_type(input, HEXLIT) ||
               check_next_token(input, KEY, "true") || check_next_token(input, KEY, "false") ||
               check_next_token_type(input, STRLIT)) {
        return parse_literal(input);
    } else if (check_next_token_type(input, ID)) {
        // Look ahead to see if this is a function call
        // We need to peek at the second token
        Token* first = TokenQueue_peek(input);
        // For simplicity, we'll assume function calls have parentheses immediately after ID
        // This requires looking ahead, but we'll handle it differently
        char name[MAX_TOKEN_LEN];
        parse_id(input, name);
        
        if (check_next_token(input, SYM, "(")) {
            // Function call
            int line = get_next_token_line(input);
            match_and_discard_next_token(input, SYM, "(");
            NodeList* args = NodeList_new();
            
            if (!check_next_token(input, SYM, ")")) {
                do {
                    ASTNode* arg = parse_expr(input);
                    NodeList_add(args, arg);
                } while (check_next_token(input, SYM, ",") && (discard_next_token(input), true));
            }
            
            match_and_discard_next_token(input, SYM, ")");
            return FuncCallNode_new(name, args, line);
        } else {
            // Location (variable or array access)
            int line = first->line;
            ASTNode* index = NULL;
            
            if (check_next_token(input, SYM, "[")) {
                discard_next_token(input);
                index = parse_expr(input);
                match_and_discard_next_token(input, SYM, "]");
            }
            
            return LocationNode_new(name, index, line);
        }
    } else {
        Token* token = TokenQueue_peek(input);
        Error_throw_printf("Unexpected token '%s' on line %d\n", token ? token->text : "EOF", get_next_token_line(input));
        return NULL;
    }
}

// ==== Literals, Locations, Calls ====

ASTNode* parse_literal(TokenQueue* input)
{
    int line = get_next_token_line(input);
    Token* token = TokenQueue_remove(input);
    if (token == NULL) {
        Error_throw_printf("Unexpected end of input (expected literal)\n");
    }
    ASTNode* result = NULL;
    
    if (token->type == DECLIT) {
        int value = atoi(token->text);
        result = LiteralNode_new_int(value, line);
    } else if (token->type == HEXLIT) {
        int value = (int)strtol(token->text, NULL, 16);
        result = LiteralNode_new_int(value, line);
    } else if (token->type == KEY && (token_str_eq(token->text, "true") || token_str_eq(token->text, "false"))) {
        bool value = token_str_eq(token->text, "true");
        result = LiteralNode_new_bool(value, line);
    } else if (token->type == STRLIT) {
        // Remove the quotes from string literals and handle escape sequences
        char str_value[MAX_TOKEN_LEN];
        int len = strlen(token->text);
        if (len >= 2 && token->text[0] == '\"' && token->text[len-1] == '\"') {
            strncpy(str_value, token->text + 1, len - 2);
            str_value[len - 2] = '\0';
            
            // Handle escape sequences
            char processed[MAX_TOKEN_LEN];
            int j = 0;
            for (int i = 0; str_value[i] && j < MAX_TOKEN_LEN - 1; i++) {
                if (str_value[i] == '\\' && str_value[i + 1] && j < MAX_TOKEN_LEN - 1) {
                    switch (str_value[i + 1]) {
                        case 'n': processed[j++] = '\n'; i++; break;
                        case 't': processed[j++] = '\t'; i++; break;
                        case 'r': processed[j++] = '\r'; i++; break;
                        case '\\': processed[j++] = '\\'; i++; break;
                        case '\"': processed[j++] = '\"'; i++; break;
                        default: 
                            if (j < MAX_TOKEN_LEN - 1) processed[j++] = str_value[i]; 
                            break;
                    }
                } else if (j < MAX_TOKEN_LEN - 1) {
                    processed[j++] = str_value[i];
                }
            }
            processed[j] = '\0';
            result = LiteralNode_new_string(processed, line);
        } else {
            result = LiteralNode_new_string(token->text, line);
        }
    } else {
        Error_throw_printf("Invalid literal '%s' on line %d\n", token->text, line);
    }
    
    Token_free(token);
    return result;
}

ASTNode* parse_location(TokenQueue* input)
{
    int line = get_next_token_line(input);
    char name[MAX_TOKEN_LEN];
    parse_id(input, name);
    
    ASTNode* index = NULL;
    if (check_next_token(input, SYM, "[")) {
        discard_next_token(input);
        index = parse_expr(input);
        match_and_discard_next_token(input, SYM, "]");
    }
    
    return LocationNode_new(name, index, line);
}




ASTNode* parse (TokenQueue* input)
{
    if (input == NULL) {
        Error_throw_printf("Cannot parse NULL token queue\n");
    }
    return parse_program(input);
}
