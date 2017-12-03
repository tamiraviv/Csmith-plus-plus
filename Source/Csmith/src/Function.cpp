// -*- mode: C++ -*-
//
// Copyright (c) 2007, 2008, 2010, 2011, 2013, 2014 The University of Utah
// All rights reserved.
//
// This file is part of `csmith', a random generator of C programs.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

//
// This file was derived from a random program generator written by Bryan
// Turner.  The attributions in that file was:
//
// Random Program Generator
// Bryan Turner (bryan.turner@pobox.com)
// July, 2005
//
#ifdef WIN32
#pragma warning(disable : 4786)   /* Disable annoying warning messages */
#endif
#include "Function.h"

#include <cassert>

#include "Common.h"
#include "Block.h"
#include "CGContext.h"
#include "CGOptions.h"
#include "Constant.h"
#include "Effect.h"
#include "Statement.h"
#include "Type.h"
#include "VariableSelector.h"
#include "FactMgr.h"
#include "random.h"
#include "util.h"
#include "Fact.h"
#include "FactPointTo.h"
#include "VectorFilter.h"
#include "Error.h"
#include "DepthSpec.h"
#include "ExtensionMgr.h"
#include "OutputMgr.h"

using namespace std;

///////////////////////////////////////////////////////////////////////////////

static vector<Function*> FuncList;		// List of all functions in the program
static vector<Function*> AllFuncList;
static vector<FactMgr*>  FMList;        // list of fact managers for each function
static long cur_func_idx;				// Index into FuncList that we are currently working on
static bool param_first = true;			// Flag to track output of commas
static int builtin_functions_cnt;

/*
 * find FactMgr for a function
 */
FactMgr*
get_fact_mgr_for_func(const Function* func)
{
	for (size_t i = 0; i < AllFuncList.size(); i++) {
		if (AllFuncList[i] == func) {
			return FMList[i];
		}
	}
	return 0;
}

/*
 *
 */
FactMgr*
get_fact_mgr(const CGContext* cg)
{
	return get_fact_mgr_for_func(cg->get_current_func());
}

const Function*
find_function_by_name(const string& name)
{
	size_t i;
	for (i = 0; i < AllFuncList.size(); i++) {
		if (AllFuncList[i]->name == name) {
			return AllFuncList[i];
		}
	}
	return NULL;
}

int
find_function_in_set(const vector<const Function*>& set, const Function* f)
{
	size_t i;
	for (i = 0; i < set.size(); i++) {
		if (set[i] == f) {
			return i;
		}
	}
	return -1;
}

const Block*
find_blk_for_var(const Variable* v)
{
	if (v->is_global()) {
		return NULL;
	}
	size_t i, j;
	for (i = 0; i < FuncList.size(); i++) {
		const Function* func = FuncList[i];
		// for a parameter of a function, pretend it's a variable belongs to the top block
		if (v->is_argument() && find_variable_in_set(func->param, v) != -1) {
			return func->body;
		}
		for (j = 0; j < func->blocks.size(); j++) {
			const Block* blk = func->blocks[j];
			if (find_variable_in_set(blk->local_vars, v) != -1) {
				return blk;
			}
		}
	}
	return NULL;
}

bool
Function::is_var_on_stack(const Variable* var, const Statement* stm) const
{
	size_t i;
	for (i = 0; i < param.size(); i++) {
		if (param[i]->match(var)) {
			return true;
		}
	}
	const Block* b = stm->parent;
	while (b) {
		if (find_variable_in_set(b->local_vars, var) != -1) {
			return true;
		}
		b = b->parent;
	}
	return false;
}

/* find whether a variable is visible in the given statement
 */
bool
Function::is_var_visible(const Variable* var, const Statement* stm) const
{
	return (var->is_global() || is_var_on_stack(var, stm));
}

/*
 * return true if a variable is out-of-scope at given statement
 * note variable must be a local variable of this function to
 * be a oos variable (sometimes DFA would analyze local variables
 * from callers, they should remain "in-scope" for this function
 */
bool
Function::is_var_oos(const Variable* var, const Statement* stm) const
{
	if (!is_var_visible(var, stm)) {
		//return true;
		size_t i;
		for (i = 0; i < blocks.size(); i++) {
			if (find_variable_in_set(blocks[i]->local_vars, var) != -1) {
				return true;
			}
		}
	}
	return false;
}

bool
Function::reach_max_functions_cnt()
{
	return ((static_cast<int>(FuncList.size()) - builtin_functions_cnt) >= CGOptions::max_funcs());
}

const vector<Function*>&
get_all_functions(void)
{
	return FuncList;
}

/*
 *
 */
 // unsigned
long
FuncListSize(void)
{
	return FuncList.size();
}

/*
 *
 */
Function *
GetFirstFunction(void)
{
	assert((builtin_functions_cnt >= 0) && "Invalid builtin_functions_cnt!");
	return FuncList[builtin_functions_cnt];
}

/*
 *
 */
static string
RandomFunctionName(void)
{
	int rand_letter = rnd_upto((unsigned int)('z' - 'a'));
	char temp = (char)rand_letter + 'a';
	string start = "func_";
	return start + temp;
}

/*-------------------------------------------------------------
 *  choose a random return type. only struct/unions and integer types
 *  (not incl. void)  are qualified, (no arrays)
 *************************************************************/
static const Type*
RandomReturnType(void)
{
	const Type* t = 0;
	t = Type::choose_random();
	return t;
}

Function *
Function::get_one_function(const vector<Function *> &ok_funcs)
{
	vector<Function *>::size_type ok_size = ok_funcs.size();

	if (ok_size == 0) {
		return 0;
	}
	if (ok_size == 1) {
		return ok_funcs[0];
	}
	int index = rnd_upto(ok_size);
	return ok_funcs[index];
}

/*
 * Choose a function from `funcs' to invoke.
 * Return null if no suitable function can be found.
 */
Function *
Function::choose_func(vector<Function *> funcs,
	const CGContext& cg_context,
	const Type* type,
	const CVQualifiers* qfer,
	bool ignore_effects_and_context)
{
	vector<Function *> ok_funcs;
	vector<Function *> ok_builtin_funcs;
	vector<Function *>::iterator i;

	for (i = funcs.begin(); i != funcs.end(); ++i) {
		// skip any function which has incompatible return type
		// if type = 0, we don't care
		if (type && !type->is_convertable((*i)->return_type))
			continue;
		// Changing the behavior of is_convertable is quite dangerous.
		// Making constraint here has less global effect.
		//if (type && (*i)->return_type->is_float() && !type->is_float())
		//	continue;
		if (qfer && (*i)->rv && !qfer->match((*i)->rv->qfer))
			continue;
		// We cannot call a function that has an as-yet unknown effect.
		// TODO: in practice, this means we never call an as-yet unbuilt func.
		if (!ignore_effects_and_context && (*i)->is_effect_known() == false) {
			continue;
		}
		// We cannot call a function with a side-effect that is in conflict with the current context
		if (!ignore_effects_and_context && cg_context.in_conflict((*i)->get_feffect())) {
			continue;
		}
		if (!ignore_effects_and_context && CGOptions::strict_volatile_rule()) {
			if (!((*i)->get_feffect().is_side_effect_free())
				&& !cg_context.get_effect_context().is_side_effect_free()) {
				continue;
			}
		}
		//// TODO: is this too strong for what we want?
		//if (!((*i)->get_feffect().is_side_effect_free())
		//	&& !effect_context.is_side_effect_free()) {
		//	continue;
		//}
		//// We cannot call a function that has a race with the current context.
		//if ((*i)->get_feffect().has_race_with(effect_context)) {
		//	continue;
		//}
		// Otherwise, this is an acceptable choice.
		if ((*i)->is_builtin)
			ok_builtin_funcs.push_back(*i);
		else
			ok_funcs.push_back(*i);
	}

	Function *f = NULL;
	if (CGOptions::builtins() && rnd_flipcoin(BuiltinFunctionProb)) {
		f = Function::get_one_function(ok_builtin_funcs);
	}
	if (f == NULL) {
		f = Function::get_one_function(ok_funcs);
	}
	return f;
}

/*
 *
 */
static unsigned int
ParamListProbability()
{
	return rnd_upto(CGOptions::max_params());
}

static void
GenerateParameterListFromString(Function &currFunc, const string &params_string)
{
	vector<string> vs;
	StringUtils::split_string(params_string, vs, ",");
	int params_cnt = vs.size();
	assert((params_cnt > 0) && "Invalid params_string!");
	if ((params_cnt == 1) && (vs[0] == "Void")) {
		return;
	}
	for (int i = 0; i < params_cnt; i++) {
		assert((vs[i] != "Void") && "Invalid parameter type!");
		CVQualifiers qfer;
		qfer.add_qualifiers(false, false);
		const Type *ty = Type::get_type_from_string(vs[i]);
		Variable *v = VariableSelector::GenerateParameterVariable(ty, &qfer);
		assert(v);
		currFunc.param.push_back(v);
	}
}

/*
 *
 */
static void
GenerateParameterList(Function &curFunc)
{
	unsigned int max = ParamListProbability();
	ERROR_RETURN();
	if (curFunc.parent != nullptr)
	{
		curFunc.is_const = rnd_flipcoin(30);
		Type* pointer_type = Type::find_pointer_type(curFunc.parent, true);
		CVQualifiers qfer = CVQualifiers({ curFunc.is_const,true }, { false,false });
		ERROR_RETURN();
		Variable *param = VariableSelector::new_variable("this", pointer_type, 0, &qfer);
		ERROR_RETURN();
		curFunc.param.push_back(param);
	}
	for (unsigned int i = 0; i <= max; i++) {
		// With some probability, choose a new random variable, or one from
		// parentParams, parentLocals, or the globals list.
		//
		// Also, build the parent's link structure for this invocation and push
		// it onto the back.
		//
		VariableSelector::GenerateParameterVariable(curFunc);
		ERROR_RETURN();
	}
	while (!OverloadValid(curFunc))
	{
		VariableSelector::GenerateParameterVariable(curFunc);
	}
}

static bool OverloadValid(Function & curFunc)
{
	vector<Function*> funcListInScope;
	if (curFunc.parent != nullptr)
		funcListInScope = curFunc.parent->func_list;
	else
		funcListInScope = FuncList;
	for (size_t i = 0; i < funcListInScope.size(); i++) {
		Function* func = funcListInScope[i];

		if (func != &curFunc)
		{
			if (func->name == curFunc.name && curFunc.param.size() == func->param.size())
			{
				/*for (size_t j = 0; j < curFunc.param.size(); j++)
				{
					if (curFunc.param[j]->type != func->param[j]->type)
						return true;
				}*/
				return false;
			}
		}
	}
	return true;
}

bool Function::compare_signature(const Function& first, const Function& second)
{
	if (&first == &second)
		return true;

	if (first.name != second.name)
		return false;

	if (first.is_const != second.is_const)
		return false;

	if (first.param.size() != second.param.size())
		return false;

	for (size_t i = 1; i < first.param.size(); i++)
		if (first.param[i]->type != second.param[i]->type)
			return false;

	return true;
}

bool Function::try_make_virtual(Function *func)
{
	assert(func);
	assert(func->parent);

	if (func->is_virtual)
		return true;

	func->is_marked_as_virtual = true;

	if (!Type::runFinalOverridersCheckForAllTypes())
	{
		func->is_marked_as_virtual = false;

		// recall virtual
		for (Function* func : AllFuncList)
		{
			if (func->parent != nullptr && func->is_virtual_candidate)
				func->is_virtual_candidate = false;
		}

		return false;
	}
	else
	{
		// commit virtual
		for (Function* func : AllFuncList)
		{
			if (func->parent != nullptr && func->is_virtual_candidate)
				func->is_virtual = true;
		}

		return true;
	}
}

void Function::MarkVirtualFunctions()
{
	size_t memberFunctionCount = std::count_if(AllFuncList.begin(), AllFuncList.end(), 
		[](const Function *func) 
		{ 
			if (func->parent != nullptr) 
				return true; 
			return false;
		});

	size_t funcionsToMark = rnd_upto(memberFunctionCount);

	for (Function *func : AllFuncList)
	{
		if (func->parent != nullptr)
		{
			// if marked the amount oaff functions we wamted to mark or failed at the last marking - stop
			if (funcionsToMark == 0 || !try_make_virtual(func))	
				break;

			funcionsToMark--;
		}
	}

};

/*
 *
 */
Function::Function(const string &name, const Type *return_type, Type* parent, eAccess access)
	: name(name),
	parent(parent),
	return_type(return_type),
	body(0),
	fact_changed(false),
	union_field_read(false),
	is_inlined(false),
	is_builtin(false),
	visited_cnt(0),
	build_state(UNBUILT),
	is_virtual(false),
	is_const(false)
{
	if (parent == nullptr)
	{
		FuncList.push_back(this);			// Add to global list of functions.
	}
	else
	{
		parent->add_to_func_list(this);
		parent->func_list_access.push_back(access);
	}
	AllFuncList.push_back(this);

}

Function::Function(const string &name, const Type *return_type, bool builtin)
	: name(name),
	return_type(return_type),
	body(0),
	fact_changed(false),
	union_field_read(false),
	is_inlined(false),
	is_builtin(builtin),
	visited_cnt(0),
	build_state(UNBUILT),
	is_virtual(false),
	is_const(false)
{
	FuncList.push_back(this);			// Add to global list of functions.
	AllFuncList.push_back(this);
}

Function *
Function::make_random_signature(const CGContext& cg_context, const Type* type, const CVQualifiers* qfer, Type* expressionType)
{
	if (type == 0)
		type = RandomReturnType();

	DEPTH_GUARD_BY_TYPE_RETURN(dtFunction, NULL);
	ERROR_GUARD(NULL);
	eAccess access = Type::choose_random_access(expressionType, cg_context.get_current_func()->parent);
	Function *f = new Function(RandomFunctionName(), type, expressionType, access);
	//if (access == ePublic)
	f->is_virtual = false;
	// dummy variable representing return variable, we don't care about the type, so use 0
	string rvname = f->name + "_" + "rv";
	CVQualifiers ret_qfer = qfer == 0 ? CVQualifiers::random_qualifiers(type, Effect::READ, cg_context, true)
		: qfer->random_qualifiers(true, Effect::READ, cg_context);
	ERROR_GUARD(NULL);
	f->rv = Variable::CreateVariable(rvname, type, NULL, &ret_qfer);
	GenerateParameterList(*f);
	FMList.push_back(new FactMgr(f));
	if (CGOptions::inline_function() && rnd_flipcoin(InlineFunctionProb))
		f->is_inlined = true;
	return f;
}

Function *
Function::try_clone_signature(const CGContext& cg_context, const Function *func_to_clone, Type* expressionType)
{
	DEPTH_GUARD_BY_TYPE_RETURN(dtFunction, NULL);
	ERROR_GUARD(NULL);

	// choose access
	eAccess access = Type::choose_random_access(expressionType, cg_context.get_current_func()->parent);

	Function *f = new Function(func_to_clone->name, func_to_clone->return_type, expressionType, access);

	// dummy variable representing return variable, we don't care about the type, so use 0
	string rvname = f->name + "_" + "rv";
	f->rv = Variable::CreateVariable(rvname, func_to_clone->return_type, nullptr, &func_to_clone->rv->qfer);
	f->is_const = func_to_clone->is_const;

	// create the first param (this)
	Type* pointer_type = Type::find_pointer_type(f->parent, true);
	CVQualifiers qfer = CVQualifiers({ f->is_const,true }, { false,false });
	ERROR_GUARD(nullptr);
	Variable *param = VariableSelector::new_variable("this", pointer_type, 0, &qfer);
	ERROR_GUARD(nullptr);
	f->param.push_back(param);

	// copy other paramiters
	for (size_t i = 1; i < func_to_clone->param.size(); i++)
	{
		Variable * var = func_to_clone->param[i];
		f->param.push_back(VariableSelector::new_variable(var->name, var->type, nullptr, &var->qfer));
	}

	while (!OverloadValid(*f))
	{
		VariableSelector::GenerateParameterVariable(*f);
	}

	FMList.push_back(new FactMgr(f));
	f->is_inlined = func_to_clone->is_inlined;

	return f;
}


/*
 *
 */
Function *
Function::make_random(const CGContext& cg_context, const Type* type, const CVQualifiers* qfer)
{
	Function* f = make_random_signature(cg_context, type, qfer);
	ERROR_GUARD(NULL);
	f->GenerateBody(cg_context);
	ERROR_GUARD(NULL);
	return f;
}

/*
 *
 */
Function *
Function::make_first(void)
{
	const Type *ty = RandomReturnType();
	ERROR_GUARD(NULL);

	Function *f = new Function(RandomFunctionName(), ty);
	// dummy variable representing return variable, we don't care about the type, so use 0
	string rvname = f->name + "_" + "rv";
	CVQualifiers ret_qfer = CVQualifiers::random_qualifiers(ty);
	ERROR_GUARD(NULL);
	f->rv = Variable::CreateVariable(rvname, ty, NULL, &ret_qfer);

	// create a fact manager for this function, with empty global facts
	FactMgr* fm = new FactMgr(f);
	FMList.push_back(fm);

	ExtensionMgr::GenerateFirstParameterList(*f);

	// No Parameter List
	f->GenerateBody(CGContext::get_empty_context());
	if (CGOptions::inline_function() && rnd_flipcoin(InlineFunctionProb))
		f->is_inlined = true;
	fm->setup_in_out_maps(true);

	// update global facts to merged facts at all possible function exits
	fm->global_facts = fm->map_facts_out[f->body];
	f->body->add_back_return_facts(fm, fm->global_facts);

	// collect info about global dangling pointers
	fm->find_dangling_global_ptrs(f);
	return f;
}

/*
 *
 */
static int
OutputFormalParam(Variable *var, std::ostream *pOut)
{
	std::ostream &out = *pOut;
	if (!param_first) out << ", ";
	param_first = false;
	//var->type->Output( out );
	if (!CGOptions::arg_structs() && var->type)
		assert(var->type->eType != eStruct);
	if (!CGOptions::arg_unions() && var->type)
		assert(var->type->eType != eUnion);

	var->output_qualified_type(out);
	out << " " << var->name;
	return 0;
}

/*
 *
 */
void
Function::OutputFormalParamList(std::ostream &out)
{
	if ((parent != nullptr && param.size() == 1) || param.size() == 0) {
		assert(Type::void_type);
		Type::void_type->Output(out);
	}
	else {
		param_first = true;
		auto itr = param.begin();
		if (parent != nullptr)
			itr++;
		for_each(itr,
			param.end(),
			std::bind2nd(std::ptr_fun(OutputFormalParam), &out));
	}
}

/*
 *
 */
void
Function::OutputHeader(std::ostream &out, bool isPrototype)
{
	if (!CGOptions::return_structs() && return_type)
		assert(return_type->eType != eStruct);
	if (!CGOptions::return_unions() && return_type)
		assert(return_type->eType != eUnion);
	if (is_inlined)
		out << "inline ";
	// force functions to be static if necessary
	if (parent == nullptr && CGOptions::force_globals_static()) {
		out << "static ";
	}
	if (is_marked_as_virtual && isPrototype)
		out << "virtual ";
	rv->qfer.output_qualified_type(return_type, out);
	out << " ";
	if (parent != nullptr && !isPrototype)
	{
		out << "S" << parent->sid << "::";
	}
	out << name << "(";
	OutputFormalParamList(out);
	//if (parent != nullptr)
	//{
	//	out << ") const"; // for now, make all the functions const
	//}
	//else
	//{
	out << ")";
	if(!isPrototype)
		out << " ";

	if (is_const)
	{
		out << " const";
	}
	//}

}

/*
 *
 */
void
Function::OutputForwardDecl(std::ostream &out)
{
	if (is_builtin)
		return;
	OutputHeader(out);
	out << ";";
	outputln(out);
}

/*
 *
 */
void
Function::Output(std::ostream &out)
{
	if (is_builtin)
		return;
	OutputMgr::set_curr_func(name);
	output_comment_line(out, "------------------------------------------");
	if (!CGOptions::concise()) {
		feffect.Output(out);
	}
	OutputHeader(out);
	outputln(out);

	if (CGOptions::depth_protect()) {
		out << "if (DEPTH < MAX_DEPTH) ";
		outputln(out);
	}

	FactMgr* fm = get_fact_mgr_for_func(this);
	// if nothing interesting happens, we don't want to see facts for statements
	if (!fact_changed && !union_field_read && !is_pointer_referenced()) {
		fm = 0;
	}
	body->Output(out, fm);

	if (CGOptions::depth_protect()) {
		out << "else";
		outputln(out);

		// TODO: Needs to be fixed when return types are no longer simple
		// types.

		out << "return ";
		ret_c->Output(out);
		out << ";";
		outputln(out);
	}

	outputln(out);
	outputln(out);
}

/*
 * Used for protecting depth
 */
void
Function::make_return_const()
{
	if (CGOptions::depth_protect() && need_return_stmt()) {
		assert(return_type);
		if (return_type->eType == eSimple)
			assert(return_type->simple_type != eVoid);
		Constant *c = Constant::make_random(return_type);
		ERROR_RETURN();
		this->ret_c = c;
	}
}

bool Function::need_return_stmt()
{
	return (return_type->eType != eSimple || return_type->simple_type != eVoid);
}

/*
 *
 */
void
Function::GenerateBody(const CGContext &prev_context)
{
	if (build_state != UNBUILT) {
		cerr << "warning: ignoring attempt to regenerate func" << endl;
		return;
	}

	build_state = BUILDING;
	Effect effect_accum;
	CGContext cg_context(this, prev_context.get_effect_context(), &effect_accum);
	cg_context.extend_call_chain(prev_context);
	FactMgr* fm = get_fact_mgr_for_func(this);
	for (size_t i = 0; i < param.size(); i++) {
		if (param[i]->type->ptr_type != 0) {
			fm->global_facts.push_back(FactPointTo::make_fact(param[i], FactPointTo::tbd_ptr));
		}
	}
	// Fill in the Function body.
	if (is_builtin)
		body = Block::make_dummy_block(cg_context);
	else
		body = Block::make_random(cg_context);
	ERROR_RETURN();
	body->set_depth_protect(true);

	// compute the pointers that are statically referenced in the function
	// including ones referenced by its callees
	body->get_referenced_ptrs(referenced_ptrs);

	// Compute the function's externally visible effect.  Currently, this
	// is just the effect on globals.
	//effect.add_external_effect(*cg_context.get_effect_accum());
	feffect.add_external_effect(fm->map_stm_effect[body]);

	make_return_const();
	ERROR_RETURN();

	// Mark this function as built.
	build_state = BUILT;
}

void
Function::generate_body_with_known_params(const CGContext &prev_context, Effect& effect_accum)
{
	if (build_state != UNBUILT) {
		cerr << "warning: ignoring attempt to regenerate func" << endl;
		return;
	}

	build_state = BUILDING;
	FactMgr* fm = get_fact_mgr_for_func(this);
	CGContext cg_context(this, prev_context.get_effect_context(), &effect_accum);
	cg_context.extend_call_chain(prev_context);

	// inherit proper no-read/write directives from caller
	VariableSet no_reads, no_writes, must_reads, must_writes, frame_vars;
	prev_context.find_reachable_frame_vars(fm->global_facts, frame_vars);
	prev_context.get_external_no_reads_writes(no_reads, no_writes, frame_vars);
	RWDirective rwd(no_reads, no_writes, must_reads, must_writes);
	cg_context.rw_directive = &rwd;
	cg_context.flags = 0;

	// Fill in the Function body.
	body = Block::make_random(cg_context);
	ERROR_RETURN();
	body->set_depth_protect(true);

	compute_summary();

	make_return_const();
	ERROR_RETURN();

	// Mark this function as built.
	build_state = BUILT;
}

void
Function::initialize_builtin_functions()
{
	// format: return_type; builtin_func_name; (param1_type, param2_type, ...)
	// supported type: Void, Char, UChar, Short, UShort, Int,
	// 		   UInt, Long, ULong, Longlong, ULonglong
	string builtin_function_strings[] = {
		"UInt; __builtin_ia32_crc32qi; (UInt, UChar); x86",
		"Int; __builtin_clz; (UInt); x86",
		"Int; __builtin_clzl; (ULong); x86",
		"Int; __builtin_clzll; (ULonglong); x86",
		"Int; __builtin_ctz; (UInt); x86",
		"Int; __builtin_ctzl; (ULong); x86",
				"Int; __builtin_ctzll; (ULonglong); x86",
		"Int; __builtin_ffs; (Int); x86",
		"Int; __builtin_ffsl; (Long); x86",
		"Int; __builtin_ffsll; (Longlong); x86",
		"Int; __builtin_parity; (UInt); x86",
		"Int; __builtin_parityl; (ULong); x86",
		"Int; __builtin_parityll; (ULonglong); x86",
		"Int; __builtin_popcount; (UInt); x86",
		"Int; __builtin_popcountl; (ULong); x86",
		"Int; __builtin_popcountll; (ULonglong); x86",
		"UInt; __builtin_bswap32; (UInt); x86",
		"ULonglong; __builtin_bswap64; (ULonglong); x86",
		"Int; __builtin_ctzs; (UShort); clang",
		"Int; __builtin_clzs; (UShort); clang",
		"UShort; __builtin_bswap16; (UShort); ppc | clang"
	};

	int cnt = sizeof(builtin_function_strings) / sizeof(builtin_function_strings[0]);
	for (int i = 0; i < cnt; i++) {
		make_builtin_function(builtin_function_strings[i]);
	}
}

void
Function::make_builtin_function(const string &function_string)
{
	vector<string> v;
	StringUtils::split_string(function_string, v, ";");
	if (v.size() == 4) {
		if (!CGOptions::enabled_builtin(v[3]))
			return;
	}
	else if (v.size() == 3) {
		if (!CGOptions::enabled_builtin("generic"))
			return;
	}
	else {
		assert(0 && "Invalid builtin function format!");
	}

	const Type *ty = Type::get_type_from_string(v[0]);
	Function *f = new Function(v[1], ty, /*is_builtin*/true);

	// dummy variable representing return variable, we don't care about the type, so use 0
	string rvname = f->name + "_" + "rv";
	CVQualifiers ret_qfer = CVQualifiers::random_qualifiers(ty);
	f->rv = Variable::CreateVariable(rvname, ty, NULL, &ret_qfer);

	// create a fact manager for this function, with empty global facts
	FactMgr* fm = new FactMgr(f);
	FMList.push_back(fm);

	GenerateParameterListFromString(*f, StringUtils::get_substring(v[2], '(', ')'));
	f->GenerateBody(CGContext::get_empty_context());

	// update global facts to merged facts at all possible function exits
	fm->global_facts = fm->map_facts_out[f->body];
	f->body->add_back_return_facts(fm, fm->global_facts);

	// collect info about global dangling pointers
	fm->find_dangling_global_ptrs(f);
	++builtin_functions_cnt;
}

void
Function::compute_summary(void)
{
	FactMgr* fm = get_fact_mgr_for_func(this);
	// compute the pointers that are statically referenced in the function
	// including ones referenced by its callees
	body->get_referenced_ptrs(referenced_ptrs);

	// Compute the function's externally visible effect.
	//effect.add_external_effect(*cg_context.get_effect_accum());
	feffect.add_external_effect(fm->map_stm_effect[body]);

	// determine whether an union field is read
	union_field_read = body->read_union_field();
}

/*
 *
 */
void
GenerateFunctions(void)
{
	FactMgr::add_interested_facts(CGOptions::interested_facts());
	if (CGOptions::builtins())
		Function::initialize_builtin_functions();
	// -----------------
	// Create a basic first function, then generate a random graph from there.
	/* Function *first = */ Function::make_first();
	ERROR_RETURN();

	// -----------------
	// Create body of each function, continue until no new functions are created.
	for (cur_func_idx = 0; cur_func_idx < FuncListSize(); cur_func_idx++) {
		// Dynamically adds new functions to the end of the list..
		if (FuncList[cur_func_idx]->is_built() == false) {
			FuncList[cur_func_idx]->GenerateBody(CGContext::get_empty_context());
			ERROR_RETURN();
		}
	}
	FactPointTo::aggregate_all_pointto_sets();
	ExtensionMgr::GenerateValues();
}

/*
 *
 */
static int
OutputForwardDecl(Function *func, std::ostream *pOut)
{
	func->OutputForwardDecl(*pOut);
	return 0;
}

/*
 *
 */
static int
OutputFunction(Function *func, std::ostream *pOut)
{
	func->Output(*pOut);
	return 0;
}

/*
 *
 */
void
OutputForwardDeclarations(std::ostream &out)
{

	outputln(out);
	outputln(out);
	output_comment_line(out, "--- FORWARD DECLARATIONS ---");
	for_each(FuncList.begin(), FuncList.end(),
		std::bind2nd(std::ptr_fun(OutputForwardDecl), &out));
}

/*
 *
 */
void
OutputFunctions(std::ostream &out)
{
	outputln(out);
	outputln(out);
	output_comment_line(out, "--- FUNCTIONS ---");
	for_each(AllFuncList.begin(), AllFuncList.end(),
		std::bind2nd(std::ptr_fun(OutputFunction), &out));
}

/*
 * Delete a single function
 */
int
Function::deleteFunction(Function* func)
{
	if (func) {
		delete func;
		func = 0;
	}
	return 0;
}

/*
 * Release all dynamic memory
 */
void
Function::doFinalization(void)
{
	for_each(FuncList.begin(), FuncList.end(), std::ptr_fun(deleteFunction));

	FuncList.clear();

	std::vector<FactMgr*>::iterator i;
	for (i = FMList.begin(); i != FMList.end(); ++i) {
		delete (*i);
	}
	FMList.clear();
	FactMgr::doFinalization();
}

Function::~Function()
{
	param.clear();

	delete rv;

	assert(stack.empty());

	if (body) {
		delete body;
		body = NULL;
	}

	if (CGOptions::depth_protect() && ret_c) {
		delete ret_c;
		ret_c = NULL;
	}
}


///////////////////////////////////////////////////////////////////////////////

// Local Variables:
// c-basic-offset: 4
// tab-width: 4
// End:

// End of file.
