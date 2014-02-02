#define realex_version "0.81"
#include "parsetree.h"
#include "parser.tab.h"
#include <iostream>
#include <fstream>
#include "buffer.h"
#include "lexer.h"
#include "version.h"
#include "built-in-types.h"
#include "constants.h"
#include <stdexcept>
#include "evaluator.h"
#include <cstring>
/*2:*//*6:*/
#line 197 "main.w"
#ifdef NREADLINE
#define readline NULL
#define add_history NULL
#define clear_history()
#else
#include <readline/readline.h>
#include <readline/history.h>
#endif/*:6*//*3:*/
#line 133 "main.w"

int yylex(YYSTYPE*,YYLTYPE*);
int yyparse(atlas::interpreter::expr*parsed_expr,int*verbosity);/*:3*/
#line 112 "main.w"

namespace{/*4:*/
#line 146 "main.w"
const char*keywords[]=
{"quit"
,"set","let","in","begin","end"
,"if","then","else","elif","fi"
,"and","or","not"
,"while","do","od","next","for","from","downto"
,"true","false"
,"quiet","verbose"
,"whattype","showall","forget"
,NULL};/*:4*/
#line 113 "main.w"
}/*5:*/
#line 166 "main.w"

int yylex(YYSTYPE*valp,YYLTYPE*locp)
{return atlas::interpreter::lex->get_token(valp,locp);}


void yyerror(YYLTYPE*locp,atlas::interpreter::expr*,int*,char const*s)
{atlas::interpreter::main_input_buffer->show_range
(std::cerr,
locp->first_line,locp->first_column,
locp->last_line,locp->last_column);
std::cerr<<s<<std::endl;
atlas::interpreter::main_input_buffer->close_includes();
}/*:5*//*7:*/
#line 217 "main.w"
int main(int argc,char* *argv)
{using namespace std;using namespace atlas::interpreter;/*10:*/
#line 335 "main.w"
bool use_readline=argc<2 or std::strcmp(argv[1],"-nr")!=0;/*:10*/
#line 222 "main.w"
BufferedInput input_buffer("expr> "
,use_readline?readline:NULL
,use_readline?add_history:NULL);
main_input_buffer= &input_buffer;
Hash_table hash;main_hash_table= &hash;
Lexical_analyser ana(input_buffer,hash,keywords,prim_names);lex= &ana;
Id_table main_table;global_id_table= &main_table;
overload_table main_overload_table;
global_overload_table= &main_overload_table;/*8:*/
#line 281 "main.w"
#ifndef NREADLINE
using_history();
rl_completion_entry_function=id_completion_func;
#endif

ana.set_comment_delims('{','}');
initialise_evaluator();initialise_builtin_types();/*:8*/
#line 233 "main.w"

cout<<"This is 'realex', version " realex_version " (compiled on "
<<atlas::version::COMPILEDATE<<
").\nIt is the programmable interpreter interface to the library (version "
<<atlas::version::VERSION<<") of\n"
<<atlas::version::NAME<<". http://www.liegroups.org/\n";

last_value=shared_value(new tuple_value(0));
last_type=copy(void_type);

while(ana.reset())
{expr parse_tree;
int old_verbosity=verbosity;
ofstream redirect;
if(yyparse(&parse_tree,&verbosity)!=0)
continue;
if(verbosity!=0)
{if(verbosity<0)
break;
if(verbosity==2 or verbosity==3)

{/*11:*/
#line 344 "main.w"
{redirect.open(ana.scanned_file_name(),ios_base::out|
(verbosity==2?ios_base::trunc:ios_base::app));
if(redirect.is_open())
output_stream= &redirect;
else
{cerr<<"Failed to open "<<ana.scanned_file_name()<<endl;
continue;
}
}/*:11*/
#line 256 "main.w"
verbosity=old_verbosity;
}
if(verbosity==1)
cout<<"Expression before type analysis: "<<parse_tree<<endl;
}/*9:*/
#line 298 "main.w"
{bool type_OK=false;
try
{expression_ptr e;
last_type=analyse_types(parse_tree,e);
type_OK=true;
if(verbosity>0)
cout<<"Type found: "<< *last_type<<endl
<<"Converted expression: "<< *e<<endl;
e->evaluate(expression_base::single_value);
last_value=pop_value();
static type_expr empty(type_list_ptr(NULL));
if(*last_type!=empty)
*output_stream<<"Value: "<< *last_value<<endl;
destroy_expr(parse_tree);
}
catch(runtime_error&err)
{if(type_OK)
cerr<<"Runtime error:\n  "<<err.what()<<"\nEvaluation aborted.";
else cerr<<err.what();
cerr<<std::endl;
reset_evaluator();main_input_buffer->close_includes();
}
catch(logic_error&err)
{cerr<<"Internal error: "<<err.what()<<", evaluation aborted.\n";
reset_evaluator();main_input_buffer->close_includes();
}
catch(exception&err)
{cerr<<err.what()<<", evaluation aborted.\n";
reset_evaluator();main_input_buffer->close_includes();
}
}/*:9*/
#line 263 "main.w"
output_stream= &cout;
}
clear_history();

cout<<"Bye.\n";
return 0;
}/*:7*//*:2*/
