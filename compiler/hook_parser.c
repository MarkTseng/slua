/*
** See Copyright Notice in lua.h
*/

/*
* hook_parser.c - Add a hook to the parser in ldo.c
*
* Most of this code is from ldo.c
*/

#if !ENABLE_PARSER_HOOK

#include "ldo.c"

int slua_precall_lua (lua_State *L, StkId func, int nresults) {
  return luaD_precall_lua(L, func, nresults);
}

#else

#include "slua_compiler.h"

#include "ldo.c"

int slua_precall_jit (lua_State *L, StkId func, int nresults) {
  Closure *cl;
  ptrdiff_t funcr;
  CallInfo *ci;
  StkId st, base;
  Proto *p;

  funcr = savestack(L, func);
  cl = clvalue(func);
  p = cl->l.p;
  luaD_checkstack(L, p->maxstacksize);
  func = restorestack(L, funcr);
  base = func + 1;
  if (L->top > base + p->numparams)
    L->top = base + p->numparams;
  ci = L->ci;  /* now `enter' new function */
  ci->func = func;
  L->base = ci->base = base;
  ci->top = L->base + p->maxstacksize;
  lua_assert(ci->top <= L->stack_last);
  L->savedpc = p->code;  /* starting point */
  ci->nresults = nresults;
  for (st = L->top; st < ci->top; st++)
    setnilvalue(st);
  L->top = ci->top;
  if (L->hookmask & LUA_MASKCALL) {
    L->savedpc++;  /* hooks assume 'pc' is already incremented */
    luaD_callhook(L, LUA_HOOKCALL, -1);
    L->savedpc--;  /* correct 'pc' */
  }
  return (p->jit_func)(L); /* do the actual call */
}

int slua_precall_jit_vararg (lua_State *L, StkId func, int nresults) {
  Closure *cl;
  ptrdiff_t funcr;
  CallInfo *ci;
  StkId st, base;
  Proto *p;
  int nargs;

  funcr = savestack(L, func);
  cl = clvalue(func);
  p = cl->l.p;
  luaD_checkstack(L, p->maxstacksize);
  func = restorestack(L, funcr);
  nargs = cast_int(L->top - func) - 1;
  base = adjust_varargs(L, p, nargs);
  func = restorestack(L, funcr);  /* previous call may change the stack */
  ci = L->ci;  /* now `enter' new function */
  ci->func = func;
  L->base = ci->base = base;
  ci->top = L->base + p->maxstacksize;
  lua_assert(ci->top <= L->stack_last);
  L->savedpc = p->code;  /* starting point */
  ci->nresults = nresults;
  for (st = L->top; st < ci->top; st++)
    setnilvalue(st);
  L->top = ci->top;
  if (L->hookmask & LUA_MASKCALL) {
    L->savedpc++;  /* hooks assume 'pc' is already incremented */
    luaD_callhook(L, LUA_HOOKCALL, -1);
    L->savedpc--;  /* correct 'pc' */
  }
  return (p->jit_func)(L); /* do the actual call */
}

int slua_precall_lua (lua_State *L, StkId func, int nresults) {
  Closure *cl;
  Proto *p;

  cl = clvalue(func);
  p = cl->l.p;
  if(p->jit_func != NULL) {
    if (!p->is_vararg) {  /* no varargs? */
      cl->l.precall = slua_precall_jit;
      return slua_precall_jit(L, func, nresults);
    } else {
      cl->l.precall = slua_precall_jit_vararg;
      return slua_precall_jit_vararg(L, func, nresults);
    }
  }
  /* function didn't compile, fall-back to lua interpreter */
  cl->l.precall = luaD_precall_lua;
  return luaD_precall_lua(L, func, nresults);
}

#endif

