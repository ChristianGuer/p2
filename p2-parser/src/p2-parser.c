/**
 * @file p2-parser.c
 * @brief Compiler phase 2: parser
 */

#include "p2-parser.h"
ASTNode *parse_program(TokenQueue *input);
ASTNode *parse_vardecl(TokenQueue *input);
DecafType parse_type(TokenQueue *input);
void parse_id(TokenQueue *input, char *buffer);

/*
 * helper functions
 */

/**
 * @brief Look up the source line of the next token in the queue.
 *
 * @param input Token queue to examine
 * @returns Source line
 */
int get_next_token_line(TokenQueue *input)
{
  if (TokenQueue_is_empty(input))
  {
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
void match_and_discard_next_token(TokenQueue *input, TokenType type,
                                  const char *text)
{
  if (TokenQueue_is_empty(input))
  {
    Error_throw_printf("Unexpected end of input (expected \'%s\')\n", text);
  }
  Token *token = TokenQueue_remove(input);
  if (token->type != type || !token_str_eq(token->text, text))
  {
    Error_throw_printf("Expected \'%s\' but found '%s' on line %d\n", text,
                       token->text, get_next_token_line(input));
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
void discard_next_token(TokenQueue *input)
{
  if (TokenQueue_is_empty(input))
  {
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
bool check_next_token_type(TokenQueue *input, TokenType type)
{
  if (TokenQueue_is_empty(input))
  {
    return false;
  }
  Token *token = TokenQueue_peek(input);
  return (token->type == type);
}

/**
 * @brief Look ahead at the type and text of the next token
 *
 * @param input Token queue to examine
 * @param type Expected type of next token
 * @param text Expected text of next token
 * @returns True if the next token is of the expected type and text, false if
 * not
 */
bool check_next_token(TokenQueue *input, TokenType type, const char *text)
{
  if (TokenQueue_is_empty(input))
  {
    return false;
  }
  Token *token = TokenQueue_peek(input);
  return (token->type == type) && (token_str_eq(token->text, text));
}

/**
 * @brief Parse and return a Decaf type
 *
 * @param input Token queue to modify
 * @returns Parsed type (it is also removed from the queue)
 */
DecafType parse_type(TokenQueue *input)
{
  if (TokenQueue_is_empty(input))
  {
    Error_throw_printf("Unexpected end of input (expected type)\n");
  }
  Token *token = TokenQueue_remove(input);
  if (token->type != KEY)
  {
    Error_throw_printf("Invalid type '%s' on line %d\n", token->text,
                       get_next_token_line(input));
  }
  DecafType t = VOID;
  if (token_str_eq("int", token->text))
  {
    t = INT;
  }
  else if (token_str_eq("bool", token->text))
  {
    t = BOOL;
  }
  else if (token_str_eq("void", token->text))
  {
    t = VOID;
  }
  else
  {
    Error_throw_printf("Invalid type '%s' on line %d\n", token->text,
                       get_next_token_line(input));
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
void parse_id(TokenQueue *input, char *buffer)
{
  if (TokenQueue_is_empty(input))
  {
    Error_throw_printf("Unexpected end of input (expected identifier)\n");
  }
  Token *token = TokenQueue_remove(input);
  if (token->type != ID)
  {
    Error_throw_printf("Invalid ID '%s' on line %d\n", token->text,
                       get_next_token_line(input));
  }
  snprintf(buffer, MAX_ID_LEN, "%s", token->text);
  Token_free(token);
}

/*
 * node-level parsing functions
 */
ASTNode *parse_vardecl(TokenQueue *input)
{
  int line = TokenQueue_peek(input)->line;
  DecafType tokenType = parse_type(input);
  char name[MAX_TOKEN_LEN];
  parse_id(input, name);
  match_and_discard_next_token(input, SYM, ";");

  return VarDeclNode_new(name, tokenType, false, 1, line);
}

ASTNode *breakout_helper(TokenQueue *input)
{
  int value;
  int line = TokenQueue_peek(input)->line;
  Token *keyword = TokenQueue_peek(input);
  if (!strcmp(keyword->text, "continue"))
  {
    discard_next_token(input);
    match_and_discard_next_token(input, SYM, ";");
    return ContinueNode_new(line);
  }
  else if (!strcmp(keyword->text, "break"))
  {
    discard_next_token(input);
    match_and_discard_next_token(input, SYM, ";");
    return BreakNode_new(line);
  }
  else if (!strcmp(keyword->text, "return"))
  {
    discard_next_token(input);
    if (check_next_token_type(input, SYM))
    {
      match_and_discard_next_token(input, SYM, ";");
      return ReturnNode_new(NULL, line);
    }
    else if (check_next_token_type(input, ID))
    {
      // TODO check if part of greater expression
      char name[MAX_TOKEN_LEN];
      parse_id(input, name);
      match_and_discard_next_token(input, SYM, ";");
      return ReturnNode_new(LocationNode_new(name, NULL, line), line);
    }
    else if (check_next_token_type(input, DECLIT))
    {
      value = atoi(TokenQueue_remove(input)->text);
      match_and_discard_next_token(input, SYM, ";");
      return ReturnNode_new(LiteralNode_new_int(value, line), line);
    }
    else if (check_next_token_type(input, HEXLIT))
    {
      value = strtol(TokenQueue_remove(input)->text, NULL, 16);
      match_and_discard_next_token(input, SYM, ";");
      return ReturnNode_new(LiteralNode_new_int(value, line), line);
    }
    else if (check_next_token_type(input, STRLIT))
    {
      // Remove leading and trailing quotes
      char *strLit = TokenQueue_remove(input)->text;
      size_t len = strlen(strLit) - 2;
      char *parsed_str = malloc(len + 1);
      strncpy(parsed_str, strLit + 1, len);
      parsed_str[len] = '\0';

      ASTNode *str = LiteralNode_new_string(parsed_str, line);
      free(parsed_str);
      match_and_discard_next_token(input, SYM, ";");
      return ReturnNode_new(str, line);
    }
    else
    {
      Error_throw_printf("invalid return");
    }
  }
  return NULL;
}

ASTNode *parse_assignment(TokenQueue *input)
{
  // parse id
  int line = TokenQueue_peek(input)->line;
  char name[MAX_TOKEN_LEN];
  parse_id(input, name);
  ASTNode *left = LocationNode_new(name, NULL, line);

  // parse =
  match_and_discard_next_token(input, SYM, "=");

  // parse value
  ASTNode *value;
  Token *token_value = TokenQueue_peek(input);
  if (check_next_token_type(input, DECLIT))
  {
    value = LiteralNode_new_int(atoi(token_value->text), line);
  }
  else if (check_next_token_type(input, HEXLIT))
  {
    value = LiteralNode_new_int(strtol(token_value->text, NULL, 16), line);
  }
  else if (check_next_token_type(input, STRLIT))
  {
    value = LiteralNode_new_string(token_value->text, line);
  }
  else if (check_next_token(input, KEY, "true") ||
           check_next_token(input, KEY, "false"))
  {
    value = LiteralNode_new_bool(strcmp(token_value->text, "false"), line);
  }
  else if (check_next_token_type(input, ID))
  {
    // TODO add function calls later
    value = LocationNode_new(token_value->text, NULL, line);
  }
  else
  {
    Error_throw_printf("Invalid assignment");
  }
  discard_next_token(input);
  match_and_discard_next_token(input, SYM, ";");
  return AssignmentNode_new(left, value, line);
}

// Helper returns ASTNode representation of bool, int, variable.
ASTNode *type_helper(TokenQueue *input)
{
  Token *peek = TokenQueue_peek(input);
  int line = peek->line;
  char name[MAX_TOKEN_LEN];
  int value;

  if (peek->type == ID)
  {
    parse_id(input, name);
    return LocationNode_new(name, NULL, line);
  }
  else if (peek->type == DECLIT)
  {
    value = atoi(TokenQueue_remove(input)->text);
    return LiteralNode_new_int(value, line);
  }
  else if (check_next_token_type(input, HEXLIT))
  {
    value = strtol(TokenQueue_remove(input)->text, NULL, 16);
    return LiteralNode_new_int(value, line);
  }
  else if (check_next_token(input, KEY, "true") || check_next_token(input, KEY, "false"))
  {
    TokenQueue_remove(input);
    return LiteralNode_new_bool(strcmp(peek->text, "false"), line);
  }
}

BinaryOpType op_type_helper(TokenQueue *input)
{
  char value[MAX_TOKEN_LEN];
  strncpy(value, TokenQueue_peek(input)->text, MAX_TOKEN_LEN - 1);

  if (!strcmp(value, "||"))
  {
    return OROP;
  }
  if (!strcmp(value, "&&"))
  {
    return ANDOP;
  }
  if (!strcmp(value, "=="))
  {
    return EQOP;
  }
  if (!strcmp(value, "!="))
  {
    return NEQOP;
  }
  if (!strcmp(value, "<"))
  {
    return LEOP;
  }
  if (!strcmp(value, "<="))
  {
    return LEOP;
  }
  if (!strcmp(value, ">="))
  {
    return GEOP;
  }
  if (!strcmp(value, ">"))
  {
    return GTOP;
  }
  if (!strcmp(value, "+"))
  {
    return ADDOP;
  }
  if (!strcmp(value, "-"))
  {
    return SUBOP;
  }
  if (!strcmp(value, "*"))
  {
    return MULOP;
  }
  if (!strcmp(value, "/"))
  {
    return DIVOP;
  }
  if (!strcmp(value, "%"))
  {
    return MODOP;
  }
  Error_throw_printf("Invalid operator");
}

ASTNode *parse_expression(TokenQueue *input)
{
  // TODO this will need to be redone for Left associativity
  // TODO add functionality for parethesis
  // TODO add function call compatibility
  int line = TokenQueue_peek(input)->line;
  Token *peek;
  // check for !(op) case: need to add function call compatibility later
  if (check_next_token(input, SYM, "!"))
  {
    TokenQueue_remove(input);
    peek = TokenQueue_peek(input);
    if (parse_type(input) == BOOL)
    {
      bool value = strcmp(peek->text, "false");
      return UnaryOpNode_new(NOTOP, LiteralNode_new_bool(value, line), line);
    }
  }
  else if (check_next_token(input, KEY, "true") || check_next_token(input, KEY, "false"))
  {
    // boolean base case
    peek = TokenQueue_remove(input);
    return LiteralNode_new_bool(strcmp(peek->text, "false"), line);
  }
  else
  {
    // left side of expression
    ASTNode *left = type_helper(input);

    // Get binary operator from helper
    BinaryOpType operator = op_type_helper(input);

    ASTNode *right = type_helper(input);
    return BinaryOpNode_new(operator, left, right, line);
  }
}

ASTNode *parse_conditional(TokenQueue *input)
{
  int line = TokenQueue_peek(input)->line;
  match_and_discard_next_token(input, KEY, "if");
  match_and_discard_next_token(input, SYM, "(");
  ASTNode *expression = parse_expression(input);
  match_and_discard_next_token(input, SYM, ")");
  ASTNode *block = parse_block(input);
  if (check_next_token(input, KEY, "else"))
  {
    ASTNode *else_block = parse_block(input);
    return ConditionalNode_new(expression, block, else_block, line);
  }
  else
  {
    return ConditionalNode_new(expression, block, NULL, line);
  }
}

ASTNode *parse_block(TokenQueue *input)
{
  // match brackets
  int line = TokenQueue_peek(input)->line;
  match_and_discard_next_token(input, SYM, "{");

  // Loop for variable declarations
  Token *next = TokenQueue_peek(input);
  NodeList *vars = NodeList_new();
  NodeList *stmts = NodeList_new();
  while (strcmp(next->text, "int") == 0 || strcmp(next->text, "bool") == 0)
  {
    ASTNode *vardecl = parse_vardecl(input);
    NodeList_add(vars, vardecl);
    next = TokenQueue_peek(input);
  }
  // Loop for assignments TODO modify to accept every statement
  while (next->type == ID)
  {
    ASTNode *assignment = parse_assignment(input);
    NodeList_add(stmts, assignment);
    next = TokenQueue_peek(input);
  }
  ASTNode *breakOut = breakout_helper(input);
  NodeList_add(stmts, breakOut);

  // match brackets and return
  match_and_discard_next_token(input, SYM, "}");
  return BlockNode_new(vars, stmts, line);
}

ParameterList *param_helper(TokenQueue *input)
{
  Token *peek = TokenQueue_peek(input);
  ParameterList *pl = ParameterList_new();
  char name[MAX_TOKEN_LEN];
  DecafType type;
  while (!strcmp(peek->text, "int") || !strcmp(peek->text, "bool"))
  {

    type = parse_type(input);
    parse_id(input, name);
    ParameterList_add_new(pl, name, type);
    if (check_next_token(input, SYM, ","))
    {
      match_and_discard_next_token(input, SYM, ",");
    }
    else
    {
      break;
    }
    peek = TokenQueue_peek(input);
  }
  return pl;
}

ASTNode *parse_function_decl(TokenQueue *input)
{
  int line = TokenQueue_peek(input)->line;
  // parse def
  match_and_discard_next_token(input, KEY, "def");
  // Return type
  DecafType returnType = parse_type(input);
  // Identifier
  char name[MAX_TOKEN_LEN];
  parse_id(input, name);

  // Parse parantheses and Squiggle brackets
  match_and_discard_next_token(input, SYM, "(");
  ParameterList *parameters = param_helper(input);
  match_and_discard_next_token(input, SYM, ")");
  ASTNode *block = parse_block(input);
  return FuncDeclNode_new(name, returnType, parameters, block, line);
}

ASTNode *parse_program(TokenQueue *input)
{
  NodeList *vars = NodeList_new();
  NodeList *funcs = NodeList_new();

  while (!TokenQueue_is_empty(input))
  {
    Token *next = TokenQueue_peek(input);

    if (next == NULL)
    {
      break;
    }

    if (check_next_token(input, KEY, "def"))
    {
      // function def
      ASTNode *func_decl = parse_function_decl(input);
      NodeList_add(funcs, func_decl);
    }
    else
    {
      // variable dec
      ASTNode *vardecl = parse_vardecl(input);
      NodeList_add(vars, vardecl);
    }
  }

  return ProgramNode_new(vars, funcs);
}

ASTNode *parse(TokenQueue *input) { return parse_program(input); }
