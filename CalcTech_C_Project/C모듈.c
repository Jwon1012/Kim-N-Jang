#include "모듈I.h"

#ifndef EXPORT
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
#endif

EXPORT int convert_expr_to_latex(const char* expr, char* out_buf, int max_len)
{
    if (expr == NULL || out_buf == NULL || max_len <= 0) return 0;
    out_buf[0] = '\0';

    TokenList tokens = tokenize(expr);
    ASTNode* root = parse_expression(&tokens);
    if (root == NULL)
    {
        free_token_list(&tokens);
        return 0;
    }

    ast_to_latex(root, out_buf, max_len);
    free_ast(root);
    free_token_list(&tokens);
    return 1;
}