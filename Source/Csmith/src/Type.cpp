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

#include "Type.h"
#include <sstream>
#include <cassert>
#include <cmath>
#include "Common.h"
#include "CGOptions.h"
#include "random.h"
#include "Filter.h"
#include "Error.h"
#include "util.h"
#include "Bookkeeper.h"
#include "Probabilities.h"
#include "DepthSpec.h"
#include "Enumerator.h"
#include "Function.h"

using namespace std;

///////////////////////////////////////////////////////////////////////////////

/*
 *
 */
const Type *Type::simple_types[MAX_SIMPLE_TYPES];

Type *Type::void_type = NULL;
int SubObjectTree::SubObjectNode::subObjectlastId = 0;
// ---------------------------------------------------------------------
// List of all types used in the program
static vector<Type *> AllTypes;
static vector<Type *> derived_types;

//////////////////////////////////////////////////////////////////////
class NonVoidTypeFilter : public Filter
{
public:
	NonVoidTypeFilter();

	virtual ~NonVoidTypeFilter();

	virtual bool filter(int v) const;

	Type *get_type();

private:
	mutable Type *typ_;

};

NonVoidTypeFilter::NonVoidTypeFilter()
	: typ_(NULL)
{

}

NonVoidTypeFilter::~NonVoidTypeFilter()
{

}

bool
NonVoidTypeFilter::filter(int v) const
{
	assert(static_cast<unsigned int>(v) < AllTypes.size());
	Type *type = AllTypes[v];
	if (type->eType == eSimple && type->simple_type == eVoid)
		return true;

	if (!type->used) {
		Bookkeeper::record_type_with_bitfields(type);
		type->used = true;
	}

	typ_ = type;
	if (type->eType == eSimple) {
		Filter *filter = SIMPLE_TYPES_PROB_FILTER;
		return filter->filter(typ_->simple_type);
	}

	return false;
}

Type *
NonVoidTypeFilter::get_type()
{
	assert(typ_);
	return typ_;
}

class NonVoidNonVolatileTypeFilter : public Filter
{
public:
	NonVoidNonVolatileTypeFilter();

	virtual ~NonVoidNonVolatileTypeFilter();

	virtual bool filter(int v) const;

	Type *get_type();

private:
	mutable Type *typ_;

};

NonVoidNonVolatileTypeFilter::NonVoidNonVolatileTypeFilter()
	: typ_(NULL)
{

}

NonVoidNonVolatileTypeFilter::~NonVoidNonVolatileTypeFilter()
{

}

bool
NonVoidNonVolatileTypeFilter::filter(int v) const
{
	assert(static_cast<unsigned int>(v) < AllTypes.size());
	Type *type = AllTypes[v];
	if (type->eType == eSimple && type->simple_type == eVoid)
		return true;

	if (type->is_aggregate() && type->is_volatile_struct_union())
		return true;

	if ((type->eType == eStruct) && (!CGOptions::arg_structs())) {
		return true;
	}

	if ((type->eType == eUnion) && (!CGOptions::arg_unions())) {
		return true;
	}

	if (!type->used) {
		Bookkeeper::record_type_with_bitfields(type);
		type->used = true;
	}

	typ_ = type;
	if (type->eType == eSimple) {
		Filter *filter = SIMPLE_TYPES_PROB_FILTER;
		return filter->filter(typ_->simple_type);
	}

	return false;
}

Type *
NonVoidNonVolatileTypeFilter::get_type()
{
	assert(typ_);
	return typ_;
}

class ChooseRandomTypeFilter : public Filter
{
public:
	ChooseRandomTypeFilter(bool for_field_var, bool struct_has_assign_ops = false);

	virtual ~ChooseRandomTypeFilter();

	virtual bool filter(int v) const;

	Type *get_type();

	bool for_field_var_;
	bool struct_has_assign_ops_;
private:
	mutable Type *typ_;
};

ChooseRandomTypeFilter::ChooseRandomTypeFilter(bool for_field_var, bool struct_has_assign_ops)
	: for_field_var_(for_field_var),
	struct_has_assign_ops_(struct_has_assign_ops),
	typ_(NULL)
{
}

ChooseRandomTypeFilter::~ChooseRandomTypeFilter()
{

}

bool
ChooseRandomTypeFilter::filter(int v) const
{
	assert((v >= 0) && (static_cast<unsigned int>(v) < AllTypes.size()));
	typ_ = AllTypes[v];
	assert(typ_);
	if (typ_->eType == eSimple) {
		Filter *filter = SIMPLE_TYPES_PROB_FILTER;
		return filter->filter(typ_->simple_type);
	}
	else if ((typ_->eType == eStruct) && (!CGOptions::return_structs())) {
		return true;
	}

	// Struct without assignment ops can not be made a field of a struct with assign ops 
	// with current implementation of these ops
	if (for_field_var_ && struct_has_assign_ops_ && !typ_->has_assign_ops()) {
		assert(CGOptions::lang_cpp());
		return true;
	}
	if (for_field_var_ && typ_->get_struct_depth() >= CGOptions::max_nested_struct_level()) {
		return true;
	}
	return false;
}

Type *
ChooseRandomTypeFilter::get_type()
{
	assert(typ_);
	return typ_;
}

///////////////////////////////////////////////////////////////////////////////

// Helper functions

///////////////////////////////////////////////////////////////////////////////

static bool checkImplicitNontrivialAssignOps(vector<const Type*> fields)
{
	if (!CGOptions::lang_cpp()) return false;
	for (size_t i = 0; i < fields.size(); ++i) {
		const Type* field = fields[i];
		if (field->has_implicit_nontrivial_assign_ops()) {
			assert((field->eType == eStruct) || (field->eType == eUnion));
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

// --------------------------------------------------------------
/* constructor for simple types
 ********************************************************/
Type::Type(eSimpleType simple_type) :
	eType(eSimple),
	ptr_type(0),
	simple_type(simple_type),
	sid(0), // not used for simple types
	used(false),
	printed(false),
	packed_(false),
	has_assign_ops_(false),
	has_implicit_nontrivial_assign_ops_(false)
{
	// Nothing else to do.
}

// --------------------------------------------------------------
 /* constructor for struct or union types
  *******************************************************/
Type::Type(vector<const Type*>& struct_fields,
	bool isStruct,
	bool packed,
	vector<CVQualifiers> &qfers,
	vector<int> &fields_length,
	bool hasAssignOps,
	bool hasImplicitNontrivialAssignOps,
	const vector<Type*> _base_classes,
	vector<eAccess> _base_classes_access,
	const vector<bool> _isVirtualBaseClasses,
	vector<Constant*> &_fields_init,
	const vector<string> &_fields_name, vector<eAccess> &_fields_access, vector<Type*> _friend_classes) :
	ptr_type(0),
	simple_type(MAX_SIMPLE_TYPES), // not a valid simple type
	fields(struct_fields),
	used(false),
	printed(false),
	packed_(packed),
	has_assign_ops_(hasAssignOps),
	has_implicit_nontrivial_assign_ops_(hasImplicitNontrivialAssignOps),
	qfers_(qfers),
	bitfields_length_(fields_length),
	base_classes(_base_classes),
	base_classes_access(_base_classes_access),
	isVirtualBaseClasses(_isVirtualBaseClasses),
	fields_init(_fields_init),
	fields_name(_fields_name),
	fields_access(_fields_access),
	friend_classes(_friend_classes)
{
	static unsigned int sequence = 0;
	if (isStruct)
		eType = eStruct;
	else
		eType = eUnion;
	sid = sequence++;

	sub_object_tree.buildTree(this);

}

// --------------------------------------------------------------
 /* constructor for pointers
  *******************************************************/
Type::Type(const Type* t) :
	eType(ePointer),
	ptr_type(t),
	simple_type(MAX_SIMPLE_TYPES), // not a valid simple type
	used(false),
	printed(false),
	packed_(false),
	has_assign_ops_(false),
	has_implicit_nontrivial_assign_ops_(false)
{
	// Nothing else to do.
}

// --------------------------------------------------------------
Type::~Type(void)
{
	// Nothing to do.
}

// --------------------------------------------------------------
#if 0
Type &
Type::operator=(const Type& t)
{
	if (this == &t) {
		return *this;
	}

	eType = t.eType;
	simple_type = t.simple_type;
	dimensions = t.dimensions;
	fields = t.fields;

	return *this;
}
#endif

// ---------------------------------------------------------------------
const Type &
Type::get_simple_type(eSimpleType st)
{
	static bool inited = false;

	assert(st != MAX_SIMPLE_TYPES);

	if (!inited) {
		for (int i = 0; i < MAX_SIMPLE_TYPES; ++i) {
			Type::simple_types[i] = 0;
		}
		inited = true;
	}

	if (Type::simple_types[st] == 0) {
		// find if type is in the allTypes already (most likely only "eVoid" is not there)
		for (size_t i = 0; i < AllTypes.size(); i++) {
			Type* tt = AllTypes[i];
			if (tt->eType == eSimple && tt->simple_type == st) {
				Type::simple_types[st] = tt;
			}
		}
		if (Type::simple_types[st] == 0) {
			Type *t = new Type(st);
			Type::simple_types[st] = t;
			AllTypes.push_back(t);
		}
	}
	return *Type::simple_types[st];
}

const Type *
Type::get_type_from_string(const string &type_string)
{
	if (type_string == "Void") {
		return Type::void_type;
	}
	else if (type_string == "Char") {
		return &Type::get_simple_type(eChar);
	}
	else if (type_string == "UChar") {
		return &Type::get_simple_type(eUChar);
	}
	else if (type_string == "Short") {
		return &Type::get_simple_type(eShort);
	}
	else if (type_string == "UShort") {
		return &Type::get_simple_type(eUShort);
	}
	else if (type_string == "Int") {
		return &Type::get_simple_type(eInt);
	}
	else if (type_string == "UInt") {
		return &Type::get_simple_type(eUInt);
	}
	else if (type_string == "Long") {
		return &Type::get_simple_type(eLong);
	}
	else if (type_string == "ULong") {
		return &Type::get_simple_type(eULong);
	}
	else if (type_string == "Longlong") {
		return &Type::get_simple_type(eLongLong);
	}
	else if (type_string == "ULonglong") {
		return &Type::get_simple_type(eULongLong);
	}
	else if (type_string == "Float") {
		return &Type::get_simple_type(eFloat);
	}

	assert(0 && "Unsupported type string!");
	return NULL;
}

// ---------------------------------------------------------------------
/* return the most commonly used type - integer
 *************************************************************/
const Type *
get_int_type()
{
	return &Type::get_simple_type(eInt);
}

Type*
Type::find_type(const Type* t)
{
	for (size_t i = 0; i < AllTypes.size(); i++) {
		if (AllTypes[i] == t) {
			return AllTypes[i];
		}
	}
	return 0;
}

// ---------------------------------------------------------------------
/* find the pointer type to the given type in existing types,
 * return 0 if not found
 *************************************************************/
Type*
Type::find_pointer_type(const Type* t, bool add)
{
	for (size_t i = 0; i < derived_types.size(); i++) {
		if (derived_types[i]->ptr_type == t) {
			return derived_types[i];
		}
	}
	if (add) {
		Type* ptr_type = new Type(t);
		derived_types.push_back(ptr_type);
		return ptr_type;
	}
	return 0;
}

bool
Type::is_const_struct_union() const
{
	if (!is_aggregate()) return false;
	assert(fields.size() == qfers_.size());

	for (size_t i = 0; i < fields.size(); ++i) {
		const Type *field = fields[i];
		if (field->is_const_struct_union()) {
			return true;
		}
		const CVQualifiers& cf = qfers_[i];
		if (cf.is_const()) return true;
	}
	return false;
}

bool
Type::is_volatile_struct_union() const
{
	if (!is_aggregate()) return false;
	assert(fields.size() == qfers_.size());

	for (size_t i = 0; i < fields.size(); ++i) {
		const Type *field = fields[i];
		if (field->is_volatile_struct_union()) {
			return true;
		}
		const CVQualifiers& cf = qfers_[i];
		if (cf.is_volatile())
			return true;
	}
	return false;
}

bool
Type::has_int_field() const
{
	if (is_int()) return true;
	for (size_t i = 0; i < fields.size(); ++i) {
		const Type* t = fields[i];
		if (t->has_int_field()) return true;
	}
	return false;
}

bool
Type::signed_overflow_possible() const
{
	return eType == eSimple && is_signed() && ((int)SizeInBytes()) >= CGOptions::int_size();
}

eAccess Type::choose_random_access(const Type * currentType, const Type * targetType)
{
	vector<eAccess> valid_access;
	eAccess access = Type::get_min_access(currentType, targetType);
	valid_access.push_back(ePublic);
	if (access <= eProtected)
		valid_access.push_back(eProtected);
	if (access <= ePrivate)
		valid_access.push_back(ePrivate);
	int index = pure_rnd_upto(valid_access.size());
	return valid_access[index];
}

eAccess Type::get_min_access(const Type * currentType, const Type * targetType)
{
	if (currentType != nullptr)
	{
		if (currentType == targetType)
			return ePrivate;

		for (size_t i = 0; i < currentType->friend_classes.size(); i++)
		{
			if (currentType->friend_classes[i] == targetType)
				return ePrivate;
		}
		/*for (size_t i = 0; i < currentType->unambiguous_base_classes.size(); i++)
		{
			if (currentType->unambiguous_base_classes[i] == targetType)
				return eProtected;
		}*/
	}
	return ePublic;
}

string Type::get_access_name(eAccess access)
{
	switch (access)
	{
	case ePrivate:
		return "private";
	case eProtected:
		return "protected";
	case ePublic:
		return "public";
	}
	return "";
}

vector<Function*> Type::get_func_list_by_target_type(const Type * targetType)
{
	vector<Function*> func_list_by_target_type;
	eAccess access = get_min_access(this, targetType);
	for (size_t i = 0; i < func_list.size(); i++)
	{
		if (access <= func_list_access[i])
			func_list_by_target_type.push_back(func_list[i]);
	}
	return func_list_by_target_type;

}

void make_random_accesses(size_t field_cnt, vector<eAccess> &accesses)
{
	vector<eAccess> valid_access;
	valid_access.push_back(ePrivate);
	valid_access.push_back(eProtected);
	valid_access.push_back(ePublic);
	valid_access.push_back(ePublic);
	for (size_t i = 0; i < field_cnt; i++)
	{
		int index = pure_rnd_upto(valid_access.size());
		accesses.push_back(valid_access[index]);
	}
}

void
Type::get_all_ok_struct_union_types(vector<Type *> &ok_types, bool no_const, bool no_volatile, bool need_int_field, bool bStruct)
{
	vector<Type *>::iterator i;
	for (i = AllTypes.begin(); i != AllTypes.end(); ++i) {
		Type* t = (*i);
		if (bStruct && t->eType != eStruct) continue;
		if (!bStruct && t->eType != eUnion) continue;
		if ((no_const && t->is_const_struct_union()) ||
			(no_volatile && t->is_volatile_struct_union()) ||
			(need_int_field && (!t->has_int_field()))) {
			continue;
		}
		ok_types.push_back(t);
	}
}

bool
Type::if_struct_will_have_assign_ops()
{
	// randomly choose if the struct will have assign operators (for C++):
	if (!CGOptions::lang_cpp())
		return false;
	return rnd_flipcoin(RegularVolatileProb);
}

// To have volatile unions in C++. I am not sure if we need those
// (If not, will be enough to return false here)
bool
Type::if_union_will_have_assign_ops()
{
	// randomly choose if the union will have assign operators (for C++):
	if (!CGOptions::lang_cpp())
		return false;
	return rnd_flipcoin(RegularVolatileProb);
}

Type*
Type::choose_random_struct_union_type(vector<Type *> &ok_types)
{
	size_t sz = ok_types.size();
	assert(sz > 0);

	int index = rnd_upto(ok_types.size());
	ERROR_GUARD(0);
	assert(index >= 0);
	Type *rv_type = ok_types[index];
	if (!rv_type->used) {
		Bookkeeper::record_type_with_bitfields(rv_type);
		rv_type->used = true;
	}
	return rv_type;
}

const Type*
Type::choose_random_pointer_type(void)
{
	unsigned int index = rnd_upto(derived_types.size());
	ERROR_GUARD(NULL);
	return derived_types[index];
}

bool
Type::has_pointer_type(void)
{
	return derived_types.size() > 0;
}

/* for exhaustive mode only */
const Type*
Type::choose_random_struct_from_type(const Type* type, bool no_volatile)
{
	if (!type)
		return NULL;

	const Type* t = type;
	vector<Type *> ok_struct_types;
	get_all_ok_struct_union_types(ok_struct_types, no_volatile, false, true, true);

	if (ok_struct_types.size() > 0) {
		DEPTH_GUARD_BY_DEPTH_RETURN(1, NULL);

		t = Type::choose_random_struct_union_type(ok_struct_types);
		ERROR_GUARD(NULL);
	}
	return t;
}

void Type::choose_random_struct(vector<Type*> &_base_classes,unsigned int limit)
{
	vector<Type *> ok_struct_types;
	get_all_ok_struct_union_types(ok_struct_types, false, false, false, true);

	if (ok_struct_types.size() > 0)
	{
		int numOfBaseClasses = rnd_upto(min(ok_struct_types.size(), limit));
		while (numOfBaseClasses > 0)
		{
			DEPTH_GUARD_BY_DEPTH_NORETURN(1);

			Type* t = Type::choose_random_struct_union_type(ok_struct_types);
			ERROR_RETURN();

			_base_classes.push_back(t);

			ok_struct_types.erase(std::find(ok_struct_types.begin(), ok_struct_types.end(), t));

			numOfBaseClasses--;
		}

	}
}
Type* Type::choose_random_struct(void)
{
	vector<Type *> ok_struct_types;
	get_all_ok_struct_union_types(ok_struct_types, false, false, false, true);
	if (ok_struct_types.size() > 0)
	{
		return Type::choose_random_struct_union_type(ok_struct_types);
	}
	return nullptr;
}

const Type*
Type::random_type_from_type(const Type* type, bool no_volatile, bool strict_simple_type)
{
	const Type* t = type;
	DEPTH_GUARD_BY_TYPE_RETURN(dtRandomTypeFromType, NULL);
	if (type == 0) {
		t = no_volatile ? choose_random_nonvoid_nonvolatile() : choose_random_nonvoid();
		ERROR_GUARD(NULL);
	}
	if (type->eType == eSimple && !strict_simple_type) {
		t = choose_random_simple();
		ERROR_GUARD(NULL);
	}
	if (t->eType == eSimple) {
		assert(t->simple_type != eVoid);
	}
	if (t->eType == ePointer || t->eType == eStruct) {
		if (rnd_flipcoin(50))
		{
			int indirect_level = type->get_indirect_level();
			if (indirect_level <= 1)
			{
				const Type* base_type = type->get_base_type();
				if (!base_type->unambiguous_base_classes.empty())
				{
					int index = rnd_upto(base_type->unambiguous_base_classes.size());
					const Type* t = base_type->unambiguous_base_classes[index];
					for (int i = 0; i < indirect_level; i++)
					{
						t = find_pointer_type(t, true);
					}
				}
			}
		}
	}

	return t;
}

const Type*
Type::random_derive_type_from_type(const Type* type)
{
	const Type* t = type;
	if (rnd_flipcoin(50))
	{
		int indirect_level = type->get_indirect_level();
		if (indirect_level <= 1)
		{
			const Type* base_type = type->get_base_type();
			if (!base_type->unambiguous_derive_classes.empty())
			{
				int index = rnd_upto(base_type->unambiguous_derive_classes.size());
				const Type* t = base_type->unambiguous_derive_classes[index];
				for (int i = 0; i < indirect_level; i++)
				{
					t = find_pointer_type(t, true);
				}
			}
		}
	}
	return t;
}

// ---------------------------------------------------------------------
static bool
MoreTypesProbability(void)
{
	if (CGOptions::lang_cpp())	// 29.07 Ilya: cpp programs should be more object oriented and therefore have more types
	{
		// Always have at least 17 types in the program.
		if (AllTypes.size() < 17)
			return true;
	}
	else
	{
		// Always have at least 10 types in the program.
		if (AllTypes.size() < 10)
			return true;
	}

	// by default 80% probability for each additional struct or union type.
	return rnd_flipcoin(MoreStructUnionTypeProb);
}

// ---------------------------------------------------------------------
eSimpleType
Type::choose_random_nonvoid_simple(void)
{
	eSimpleType simple_type;
#if 0
	vector<unsigned int> vs;
	vs.push_back(eVoid);

	if (!CGOptions::allow_int64()) {
		vs.push_back(eLongLong);
		vs.push_back(eULongLong);
	}

	VectorFilter filter(vs);
#endif

	simple_type = (eSimpleType)rnd_upto(MAX_SIMPLE_TYPES, SIMPLE_TYPES_PROB_FILTER);

	return simple_type;
}

void
Type::make_one_bitfield(vector<const Type*> &random_fields, vector<CVQualifiers> &qualifiers,
	vector<int> &fields_length)
{
	int max_length = CGOptions::int_size() * 8;
	bool sign = rnd_flipcoin(BitFieldsSignedProb);
	ERROR_RETURN();

	const Type *type = sign ? &Type::get_simple_type(eInt) : &Type::get_simple_type(eUInt);
	random_fields.push_back(type);
	CVQualifiers qual = CVQualifiers::random_qualifiers(type, FieldConstProb, FieldVolatileProb);
	ERROR_RETURN();
	qualifiers.push_back(qual);
	int length = rnd_upto(max_length);
	ERROR_RETURN();

	bool no_zero_len = fields_length.empty() || (fields_length.back() == 0);
	// force length to be non-zero is required
	if (length == 0 && no_zero_len) {
		if (max_length <= 2) length = 1;
		else length = rnd_upto(max_length - 1) + 1;
	}
	ERROR_RETURN();
	fields_length.push_back(length);
}

// ---------------------------------------------------------------------
void
Type::make_full_bitfields_struct_fields(size_t field_cnt, vector<const Type*> &random_fields,
	vector<CVQualifiers> &qualifiers,
	vector<int> &fields_length,
	vector<Constant*> &fields_init,
	bool structHasAssignOps)
{
	for (size_t i = 0; i < field_cnt; i++) {
		bool is_non_bitfield = rnd_flipcoin(ScalarFieldInFullBitFieldsProb);
		if (is_non_bitfield) {
			make_one_struct_field(random_fields, qualifiers, fields_length, structHasAssignOps);
		}
		else {
			make_one_bitfield(random_fields, qualifiers, fields_length);
		}

		if (random_fields.back()->eType == eTypeDesc::eSimple)
		{
			if (is_non_bitfield)
				fields_init.push_back(Constant::make_random(random_fields.back()));
			else
				fields_init.push_back(Constant::make_random_in_range(random_fields.back(), fields_length.back()));
		}
		else
		{
			fields_init.push_back(nullptr);
		}
	}
}

void
Type::make_one_struct_field(vector<const Type*> &random_fields,
	vector<CVQualifiers> &qualifiers,
	vector<int> &fields_length,
	bool structHasAssignOps)
{
	ChooseRandomTypeFilter f(/*for_field_var*/true, structHasAssignOps);
	unsigned int i = rnd_upto(AllTypes.size(), &f);
	ERROR_RETURN();
	const Type* type = AllTypes[i];
	random_fields.push_back(type);
	CVQualifiers qual = CVQualifiers::random_qualifiers(type, FieldConstProb, FieldVolatileProb);
	ERROR_RETURN();
	qualifiers.push_back(qual);
	fields_length.push_back(-1);
}

void
Type::make_one_union_field(vector<const Type*> &fields, vector<CVQualifiers> &qfers, vector<int> &lens)
{
	bool is_bitfield = CGOptions::bitfields() && !CGOptions::ccomp() && rnd_flipcoin(BitFieldInNormalStructProb);
	if (is_bitfield) {
		make_one_bitfield(fields, qfers, lens);
	}
	else {
		size_t i;
		vector<Type*> ok_nonstruct_types;
		vector<Type*> struct_types;
		for (i = 0; i < AllTypes.size(); i++) {
			if ((AllTypes[i]->eType != eStruct) && (AllTypes[i]->eType != eUnion)) {
				ok_nonstruct_types.push_back(AllTypes[i]);
				continue;
			}
			// filter out structs and unions containing bit-fields. Their layout is implementation 
			// defined, we don't want to mess with them in unions for now
			if (AllTypes[i]->has_bitfields())
				continue;

			// filter out structs/unions with assign operators (C++ only) 
			if (AllTypes[i]->has_implicit_nontrivial_assign_ops())
				continue;

			if (AllTypes[i]->eType == eStruct) {
				struct_types.push_back(AllTypes[i]);
			}
			else {	// union
					// no union in union currently
			}
		}
		const Type* type = NULL;
		do {
			// 15% chance to be struct field
			if (!struct_types.empty() && pure_rnd_flipcoin(15)) {
				type = struct_types[pure_rnd_upto(struct_types.size())];
				assert(type->eType == eStruct);
			}
			// 10% chance to be char* if pointer is allowed
			else if (CGOptions::pointers() && CGOptions::int8() && pure_rnd_flipcoin(10)) {
				type = find_pointer_type(&get_simple_type(eChar), true);
			}
			else {
				unsigned int i = pure_rnd_upto(ok_nonstruct_types.size());
				const Type* t = ok_nonstruct_types[i];
				if (t->eType == eSimple && SIMPLE_TYPES_PROB_FILTER->filter(t->simple_type)) {
					continue;
				}
				type = t;
			}
		} while (type == NULL);

		fields.push_back(type);
		CVQualifiers qual = CVQualifiers::random_qualifiers(type, FieldConstProb, FieldVolatileProb);
		ERROR_RETURN();
		qfers.push_back(qual);
		lens.push_back(-1);
	}
}

void
Type::make_normal_struct_fields(size_t field_cnt, vector<const Type*> &random_fields,
	vector<CVQualifiers> &qualifiers,
	vector<int> &fields_length,
	vector<Constant*> &fields_init,
	bool structHasAssignOps)
{
	for (size_t i = 0; i < field_cnt; i++)
	{
		bool is_bitfield = CGOptions::bitfields() && rnd_flipcoin(BitFieldInNormalStructProb);
		if (is_bitfield) {
			make_one_bitfield(random_fields, qualifiers, fields_length);
		}
		else {
			make_one_struct_field(random_fields, qualifiers, fields_length, structHasAssignOps);
		}

		if (random_fields.back()->eType == eTypeDesc::eSimple)
		{
			if (is_bitfield)
				fields_init.push_back(Constant::make_random_in_range(random_fields.back(), fields_length.back()));
			else
				fields_init.push_back(Constant::make_random(random_fields.back()));
		}
		else
		{
			fields_init.push_back(nullptr);
		}

	}
}

#define ZERO_BITFIELD 0
#define RANDOM_BITFIELD 1
//#define MAX_BITFIELD 2
#define ENUM_BITFIELD_SIZE 2

void
Type::init_is_bitfield_enumerator(Enumerator<string> &enumerator, int bitfield_prob)
{
	int field_cnt = CGOptions::max_struct_fields();
	for (int i = 0; i < field_cnt; ++i) {
		std::ostringstream ss;
		ss << "bitfield" << i;

		if (CGOptions::bitfields()) {
			enumerator.add_bool_elem(ss.str(), bitfield_prob);
		}
		else {
			enumerator.add_bool_elem(ss.str(), 0);
		}
	}
}

void
Type::init_fields_enumerator(Enumerator<string> &enumerator,
	Enumerator<string> &bitfield_enumerator,
	int type_bound, int qual_bound,
	int bitfield_qual_bound)
{
	int field_cnt = CGOptions::max_struct_fields();
	for (int i = 0; i < field_cnt; ++i) {
		std::ostringstream ss;
		ss << "bitfield" << i;
		bool is_bitfield = bitfield_enumerator.get_elem(ss.str()) != 0;
		if (is_bitfield) {
			std::ostringstream ss1, ss2, ss3;
			ss1 << "bitfield_sign" << i;
			ss2 << "bitfield_qualifier" << i;
			ss3 << "bitfield_length" << i;
			enumerator.add_bool_elem_of_bool(ss1.str(), false);
			enumerator.add_elem(ss2.str(), bitfield_qual_bound);
			enumerator.add_elem(ss3.str(), ENUM_BITFIELD_SIZE);
		}
		else {
			std::ostringstream ss1, ss2;
			ss1 << "field" << i;
			ss2 << "qualifier" << i;
			enumerator.add_elem(ss1.str(), type_bound);
			enumerator.add_elem(ss2.str(), qual_bound);
		}
	}
	enumerator.add_bool_elem_of_bool("packed", CGOptions::packed_struct());
}

int
Type::get_bitfield_length(int length_flag)
{
	int max_length = CGOptions::int_size() * 8;
	assert(max_length > 0);
	int length = 0;
	switch (length_flag) {
	case ZERO_BITFIELD:
		length = 0;
		break;
#if 0
	case MAX_BITFIELD:
		length = max_length;
		break;
#endif
	case RANDOM_BITFIELD:
		length = pure_rnd_upto(max_length);
		break;
	default:
		assert(0);
		break;
	}
	return length;
}

bool
Type::make_one_bitfield_by_enum(Enumerator<string> &enumerator,
	vector<CVQualifiers> &all_bitfield_quals,
	vector<const Type*> &random_fields,
	vector<CVQualifiers> &qualifiers,
	vector<int> &fields_length,
	int index, bool &last_is_zero)
{

	std::ostringstream ss1, ss2, ss3;
	ss1 << "bitfield_sign" << index;
	ss2 << "bitfield_qualifier" << index;
	ss3 << "bitfield_length" << index;

	bool sign = enumerator.get_elem(ss1.str()) != 0;
	// we cannot allow too many structs,
	// so randomly choose the sign of fields.
	if (pure_rnd_flipcoin(50))
		sign = true;
	const Type *type = sign ? &Type::get_simple_type(eInt) : &Type::get_simple_type(eUInt);
	random_fields.push_back(type);
	int qual_index = enumerator.get_elem(ss2.str());
	assert((qual_index >= 0) && ((static_cast<unsigned int>(qual_index)) < all_bitfield_quals.size()));
	CVQualifiers qual = all_bitfield_quals[qual_index];
	qualifiers.push_back(qual);

	int length_flag = enumerator.get_elem(ss3.str());

	int length = get_bitfield_length(length_flag);
	if ((index == 0 || last_is_zero) && (length == 0)) {
		return false;
	}
	last_is_zero = (length == 0) ? true : false;
	fields_length.push_back(length);
	return true;
}

bool
Type::make_one_normal_field_by_enum(Enumerator<string> &enumerator, vector<const Type*> &all_types,
	vector<CVQualifiers> &all_quals, vector<const Type*> &fields,
	vector<CVQualifiers> &quals, vector<int> &fields_length, int i)
{
	int types_size = all_types.size();
	int quals_size = all_quals.size();
	Filter *filter = SIMPLE_TYPES_PROB_FILTER;

	std::ostringstream ss1, ss2;
	ss1 << "field" << i;
	int typ_index = enumerator.get_elem(ss1.str());
	assert(typ_index >= 0 && typ_index < types_size);
	Type *typ = const_cast<Type*>(all_types[typ_index]);
	if (typ->eType == eSimple) {
		assert(typ->simple_type != eVoid);
		if (filter->filter(typ->simple_type))
			return false;
	}

	assert(typ != NULL);
	fields.push_back(typ);

	ss2 << "qualifier" << i;
	int qual_index = enumerator.get_elem(ss2.str());
	assert(qual_index >= 0 && qual_index < quals_size);
	CVQualifiers qual = all_quals[qual_index];
	quals.push_back(qual);

	fields_length.push_back(-1);
	return true;
}

void
Type::make_all_struct_types_(Enumerator<string> &bitfields_enumerator, vector<const Type*> &accum_types,
	vector<const Type*> &all_types, vector<CVQualifiers> &all_quals,
	vector<CVQualifiers> &all_bitfield_quals)
{
	Enumerator<string> fields_enumerator;
	init_fields_enumerator(fields_enumerator, bitfields_enumerator, all_types.size(),
		all_quals.size(), all_bitfield_quals.size());

	Enumerator<string> *i;
	for (i = fields_enumerator.begin(); i != fields_enumerator.end(); i = i->next()) {
		make_all_struct_types_with_bitfields(*i, bitfields_enumerator, accum_types, all_types, all_quals, all_bitfield_quals);
	}
}


void make_random_member_names(size_t field_cnt, vector<string> &members_names)
{
	size_t highest_letter_number = std::min(field_cnt * 2, (unsigned int)('z' - 'a'));

	int rand_letter;
	while (field_cnt != 0)
	{
		rand_letter = rnd_upto(highest_letter_number);

		char temp = (char)rand_letter + 'a';
		string new_member_name;
		new_member_name += temp;

		if (find(members_names.begin(), members_names.end(), new_member_name) == members_names.end())
		{
			members_names.push_back(new_member_name);
			field_cnt--;
		}
	}
}

void
Type::make_all_struct_types_with_bitfields(Enumerator<string> &enumerator,
	Enumerator<string> &bitfields_enumerator, vector<const Type*> &accum_types,
	vector<const Type*> &all_types, vector<CVQualifiers> &all_quals,
	vector<CVQualifiers> &all_bitfield_quals)
{
	vector<const Type*> fields;
	vector<CVQualifiers> quals;
	vector<int> fields_length;
	int field_cnt = CGOptions::max_struct_fields();
	bool last_is_zero = false;

	int bitfields_cnt = 0;
	int normal_fields_cnt = 0;
	for (int i = 0; i < field_cnt; ++i) {
		std::ostringstream ss;
		ss << "bitfield" << i;
		bool is_bitfield = bitfields_enumerator.get_elem(ss.str()) != 0;
		bool rv = false;
		if (is_bitfield) {
			rv = make_one_bitfield_by_enum(enumerator, all_bitfield_quals, fields, quals, fields_length, i, last_is_zero);
			bitfields_cnt++;
		}
		else {
			rv = make_one_normal_field_by_enum(enumerator, all_types, all_quals, fields, quals, fields_length, i);
			last_is_zero = rv ? false : last_is_zero;
			normal_fields_cnt++;
		}
		if (!rv)
			return;
	}
	if ((ExhaustiveBitFieldsProb > 0) && (ExhaustiveBitFieldsProb < 100) &&
		((bitfields_cnt == field_cnt) || (normal_fields_cnt == field_cnt)))
		return;

	bool packed = enumerator.get_elem("packed") != 0;
	bool hasAssignOps = if_struct_will_have_assign_ops();
	bool hasImplicitNontrivialAssignOps = hasAssignOps || checkImplicitNontrivialAssignOps(fields);

	vector<Constant*> fields_init;
	vector<string> fields_name;
	vector<eAccess> fields_accesses;

	make_random_member_names(field_cnt, fields_name);
	make_random_accesses(fields_name.size(), fields_accesses);
	Type* new_type = new Type(fields, true, packed, quals, fields_length, hasAssignOps, hasImplicitNontrivialAssignOps, vector< Type*>(), vector<eAccess>(), vector<bool>(), fields_init, fields_name, fields_accesses, vector< Type*>());
	new_type->used = true;
	accum_types.push_back(new_type);
}

/*
 * level control's the nested level of struct
 */
void
Type::copy_all_fields_types(vector<const Type*> &dest_types, vector<const Type*> &src_types)
{
	vector<const Type*>::const_iterator i;
	for (i = src_types.begin(); i != src_types.end(); ++i)
		dest_types.push_back(*i);
}

void
Type::reset_accum_types(vector<const Type*> &accum_types)
{
	accum_types.clear();
	vector<Type*>::const_iterator i;
	for (i = AllTypes.begin(); i != AllTypes.end(); ++i)
		accum_types.push_back(*i);
}

void
Type::delete_useless_structs(vector<const Type*> &all_types, vector<const Type*> &accum_types)
{
	assert(all_types.size() <= accum_types.size());
	for (size_t i = 0; i < all_types.size(); ++i) {
		const Type *t = all_types[i];
		if (t->eType == eStruct) {
			const Type *t1 = accum_types[i];
			delete t1;
			accum_types[i] = t;
		}
	}
}

void
Type::make_all_struct_types(int level, vector<const Type*> &accum_types)
{

	if (level > 0) {
		make_all_struct_types(level - 1, accum_types);
	}
	vector<const Type*> all_types;
	copy_all_fields_types(all_types, accum_types);
	reset_accum_types(accum_types);

	vector<CVQualifiers> all_quals;
	CVQualifiers::get_all_qualifiers(all_quals, RegularConstProb, RegularVolatileProb);

	vector<CVQualifiers> all_bitfield_quals;
	CVQualifiers::get_all_qualifiers(all_bitfield_quals, FieldConstProb, FieldVolatileProb);

	Enumerator<string> fields_enumerator;
	init_is_bitfield_enumerator(fields_enumerator, ExhaustiveBitFieldsProb);

	Enumerator<string> *i;
	for (i = fields_enumerator.begin(); i != fields_enumerator.end(); i = i->next()) {
		make_all_struct_types_(*i, accum_types, all_types, all_quals, all_bitfield_quals);
	}
	delete_useless_structs(all_types, accum_types);
}

void
Type::make_all_struct_union_types(void)
{
	int level = CGOptions::max_nested_struct_level();
	if (CGOptions::dfs_exhaustive()) {
		vector<const Type*> accum_types;
		reset_accum_types(accum_types);
		make_all_struct_types(level, accum_types);
		assert(accum_types.size() >= AllTypes.size());
		for (size_t i = AllTypes.size(); i < accum_types.size(); ++i)
			AllTypes.push_back(const_cast<Type*>(accum_types[i]));
	}
}

bool
Type::has_aggregate_field(const vector<const Type *> &fields)
{
	for (vector<const Type *>::const_iterator iter = fields.begin(),
		iter_end = fields.end(); iter != iter_end; ++iter) {
		if ((*iter)->is_aggregate())
			return true;
	}
	return false;
}

bool
Type::has_longlong_field(const vector<const Type *> &fields)
{
	for (vector<const Type *>::const_iterator iter = fields.begin(),
		iter_end = fields.end(); iter != iter_end; ++iter) {
		if ((*iter)->is_long_long())
			return true;
	}
	return false;
}

Type*
Type::make_random_struct_type(void)
{
	size_t field_cnt = 0;
	size_t max_cnt = CGOptions::max_struct_fields();
	if (CGOptions::fixed_struct_fields())
		field_cnt = max_cnt;
	else
		field_cnt = rnd_upto(max_cnt) + 1;
	ERROR_GUARD(NULL);

	// choose random structs to inherit from
	vector<Type*> _base_clasees;
	vector<eAccess> _base_clasees_access;
	choose_random_struct(_base_clasees,5);
	make_random_accesses(_base_clasees.size(), _base_clasees_access);
	vector<bool> _isVirtualBaseClasses(_base_clasees.size());
	for (size_t i = 0; i < _base_clasees.size(); i++)
	{
		if (rnd_flipcoin(50))
		{
			_isVirtualBaseClasses[i] = true; // this base class is virtual
		}
		else
		{
			_isVirtualBaseClasses[i] = false; // // this base class is not virtual
		}
	}


	vector<Type*> _friend_clasees;
	if (rnd_flipcoin(10))
		choose_random_struct(_friend_clasees,2);

	ERROR_GUARD(NULL);

	vector<const Type*> random_fields;
	vector<CVQualifiers> qualifiers;
	vector<int> fields_length;
	vector<Constant*> fields_init;
	bool is_bitfields = CGOptions::bitfields() && rnd_flipcoin(BitFieldsCreationProb);
	ERROR_GUARD(NULL);
	bool hasAssignOps = if_struct_will_have_assign_ops();
	//if (CGOptions::bitfields())
	if (is_bitfields)
		make_full_bitfields_struct_fields(field_cnt, random_fields, qualifiers, fields_length, fields_init, hasAssignOps);
	else
		make_normal_struct_fields(field_cnt, random_fields, qualifiers, fields_length, fields_init, hasAssignOps);

	ERROR_GUARD(NULL);

	vector<string> members_names;
	vector<eAccess> members_access;
	make_random_member_names(field_cnt, members_names);
	make_random_accesses(members_names.size(), members_access);

	// for now, no union type
	bool packed = false;
	if (CGOptions::packed_struct()) {
		if (CGOptions::ccomp() && (has_aggregate_field(random_fields) || has_longlong_field(random_fields))) {
			// Nothing to do
		}
		else {
			packed = rnd_flipcoin(10);
			ERROR_GUARD(NULL);
		}
	}
	bool hasImplicitNontrivialAssignOps = hasAssignOps || checkImplicitNontrivialAssignOps(random_fields);
	Type* new_type = new Type(random_fields, true, packed, qualifiers, fields_length, hasAssignOps, hasImplicitNontrivialAssignOps, _base_clasees, _base_clasees_access, _isVirtualBaseClasses, fields_init, members_names, members_access, _friend_clasees);
	// update child classes
	for (Type * base_class : _base_clasees)
	{
		base_class->child_classes.push_back(new_type);
	}
	return new_type;
}

Type*
Type::make_random_union_type(void)
{
	size_t max_cnt = CGOptions::max_union_fields();
	size_t field_cnt = rnd_upto(max_cnt) + 1;
	ERROR_GUARD(NULL);

	vector<const Type*> fields;
	vector<CVQualifiers> qfers;
	vector<int> lens;

	for (size_t i = 0; i < field_cnt; i++) {
		make_one_union_field(fields, qfers, lens);
		assert(!fields.back()->has_bitfields());
	}
	bool hasAssignOps = if_union_will_have_assign_ops();
	bool hasImplicitNontrivialAssignOps = hasAssignOps || checkImplicitNontrivialAssignOps(fields);

	vector<Constant*> fields_init(fields.size(), nullptr);
	vector<string> members_names;
	vector<eAccess> members_access;
	make_random_member_names(field_cnt, members_names);
	make_random_accesses(members_names.size(), members_access);
	Type* new_type = new Type(fields, false, false, qfers, lens, hasAssignOps, hasImplicitNontrivialAssignOps, vector< Type*>(), vector<eAccess>(), vector<bool>(), fields_init, members_names, members_access, vector< Type*>());
	return new_type;
}

// ---------------------------------------------------------------------
Type*
Type::make_random_pointer_type(void)
{
	//Type* new_type = 0;
	//Type* ptr_type = 0;
	// occasionally choose pointer to pointers
	if (rnd_flipcoin(20)) {
		ERROR_GUARD(NULL);
		if (derived_types.size() > 0) {
			unsigned int rnd_num = rnd_upto(derived_types.size());
			ERROR_GUARD(NULL);
			const Type* t = derived_types[rnd_num];
			if (t->get_indirect_level() < CGOptions::max_indirect_level()) {
				return find_pointer_type(t, true);
			}
		}
	}

	// choose a pointer to basic/aggregate types
	const Type* t = choose_random();
	ERROR_GUARD(NULL);
	// consolidate all integer pointer types into "int*", hopefully this increase
	// chance of pointer assignments and dereferences
	if (t->eType == eSimple) {
		t = get_int_type();
		ERROR_GUARD(NULL);
	}
	return find_pointer_type(t, true);
}

// ---------------------------------------------------------------------
void
Type::GenerateSimpleTypes(void)
{
	unsigned int st;
	for (st = eChar; st < MAX_SIMPLE_TYPES; st++)
	{
		AllTypes.push_back(new Type((enum eSimpleType)st));
	}
	Type::void_type = new Type((enum eSimpleType)eVoid);
}

// ---------------------------------------------------------------------
void
GenerateAllTypes(void)
{
	// In the exhaustive mode, we want to generate all type first.
	// We don't support struct for now
	if (CGOptions::dfs_exhaustive()) {
		Type::GenerateSimpleTypes();
		if (CGOptions::use_struct() && CGOptions::expand_struct())
			Type::make_all_struct_union_types();
		return;
	}

	Type::GenerateSimpleTypes();
	if (CGOptions::use_struct()) {
		while (MoreTypesProbability()) {
			Type *ty = Type::make_random_struct_type();
			AllTypes.push_back(ty);
		}
	}
	if (CGOptions::use_union()) {
		while (MoreTypesProbability()) {
			Type *ty = Type::make_random_union_type();
			AllTypes.push_back(ty);
		}
	}
}

// ---------------------------------------------------------------------
const Type *
Type::choose_random()
{
	ChooseRandomTypeFilter f(/*for_field_var*/false);
	rnd_upto(AllTypes.size(), &f);
	ERROR_GUARD(NULL);
	Type *rv_type = f.get_type();
	if (!rv_type->used) {
		Bookkeeper::record_type_with_bitfields(rv_type);
		rv_type->used = true;
	}
	return rv_type;
}

const Type *
Type::choose_random_nonvoid(void)
{
	DEPTH_GUARD_BY_DEPTH_RETURN(1, NULL);
	NonVoidTypeFilter f;
	rnd_upto(AllTypes.size(), &f);

	ERROR_GUARD(NULL);
	Type *typ = f.get_type();
	assert(typ);
	return typ;
}

const Type *
Type::choose_random_nonvoid_nonvolatile(void)
{
	DEPTH_GUARD_BY_DEPTH_RETURN(1, NULL);
	NonVoidNonVolatileTypeFilter f;
	rnd_upto(AllTypes.size(), &f);

	ERROR_GUARD(NULL);
	Type *typ = f.get_type();
	assert(typ);
	return typ;
}

// ---------------------------------------------------------------------
const Type *
Type::choose_random_simple(void)
{
	DEPTH_GUARD_BY_TYPE_RETURN(dtTypeChooseSimple, NULL);
	eSimpleType ty = choose_random_nonvoid_simple();
	ERROR_GUARD(NULL);
	assert(ty != eVoid);
	return &get_simple_type(ty);
}

// ---------------------------------------------------------------------
int
Type::get_indirect_level() const
{
	int level = 0;
	const Type* pt = ptr_type;
	while (pt != 0) {
		level++;
		pt = pt->ptr_type;
	}
	return level;
}

// ---------------------------------------------------------------------
int
Type::get_struct_depth() const
{
	int depth = 0;
	if (eType == eStruct) {
		depth++;
		int max_depth = 0;
		for (size_t i = 0; i < fields.size(); i++) {
			int field_depth = fields[i]->get_struct_depth();
			if (field_depth > max_depth) {
				max_depth = field_depth;
			}
		}
		depth += max_depth;
	}
	return depth;
}

bool
Type::is_unamed_padding(size_t index) const
{
	size_t sz = bitfields_length_.size();
	if (sz == 0)
		return false;

	assert(index < sz);
	return (bitfields_length_[index] == 0);
}

bool
Type::is_bitfield(size_t index) const
{
	assert(index < bitfields_length_.size());
	return (bitfields_length_[index] >= 0);
}

bool
Type::has_bitfields() const
{
	for (size_t i = 0; i < fields.size(); i++) {
		if (bitfields_length_[i] >= 0) {
			return true;
		}
		if (fields[i]->eType == eStruct && fields[i]->has_bitfields()) {
			return true;
		}
	}
	return false;
}

// conservatively assume padding is present in all unpacked structures
// or whenever there is bitfields
bool
Type::has_padding(void) const
{
	if (eType == eStruct && !packed_) return true;
	for (size_t i = 0; i < fields.size(); i++) {
		if (is_bitfield(i) || fields[i]->has_padding()) {
			return true;
		}
	}
	return false;
}

bool
Type::is_full_bitfields_struct() const
{
	if (eType != eStruct) return false;
	size_t i;
	for (i = 0; i < bitfields_length_.size(); ++i) {
		if (bitfields_length_[i] < 0)
			return false;
	}
	return true;
}

bool
Type::is_signed(void) const
{
	switch (eType) {
	default:
		return false;

	case eSimple:
		switch (simple_type) {
		case eUChar:
		case eUInt:
		case eUShort:
		case eULong:
		case eULongLong:
			return false;
			break;
		default:
			break;
		}
		break;
	}
	return true;
}

const Type*
Type::to_unsigned(void) const
{
	if (eType == eSimple) {
		switch (simple_type) {
		case eUChar:
		case eUInt:
		case eUShort:
		case eULong:
		case eULongLong:
			return this;
		case eChar: return &get_simple_type(eUChar);
		case eInt: return &get_simple_type(eUInt);
		case eShort: return &get_simple_type(eUShort);
		case eLong: return &get_simple_type(eULong);
		case eLongLong: return &get_simple_type(eULongLong);
		default:
			break;
		}
	}
	return NULL;
}

const Type*
Type::get_base_type(void) const
{
	const Type* tmp = this;
	while (tmp->ptr_type != 0) {
		tmp = tmp->ptr_type;
	}
	return tmp;
}

bool
Type::is_promotable(const Type* t) const
{
	if (eType == eSimple && t->eType == eSimple) {
		eSimpleType t2 = t->simple_type;
		switch (simple_type) {
		case eChar:
		case eUChar: return (t2 != eVoid);
		case eShort:
		case eUShort: return (t2 != eVoid && t2 != eChar && t2 != eUChar);
		case eInt:
		case eUInt: return (t2 != eVoid && t2 != eChar && t2 != eUChar && t2 != eShort && t2 != eUShort);
		case eLong:
		case eULong: return (t2 == eLong || t2 == eULong || t2 == eLongLong || t2 == eULongLong);
		case eLongLong:
		case eULongLong: return (t2 == eLongLong || t2 == eULongLong);
		case eFloat: return (t2 != eVoid);
		default: break;
		}
	}
	return false;
}

// ---------------------------------------------------------------------
/* generally integer types can be converted to any other interger types
 * void / struct / union / array types are not. Pointers depend on the
 * type they point to. (unsigned int*) is convertable to (int*), etc
 *************************************************************/
bool
Type::is_convertable(const Type* t) const
{
	if (this == t)
		return true;
	if (eType == eSimple && t->eType == eSimple) {
		// forbiden conversion from float to int
		if (t->is_float() && !is_float())
			return false;
		if ((simple_type != eVoid && t->simple_type != eVoid) ||
			simple_type == t->simple_type)
			return true;
	}
	else if (eType == ePointer && t->eType == ePointer) {
		if (ptr_type == t->ptr_type) {
			return true;
		}
		if (ptr_type->eType == eSimple && t->ptr_type->eType == eSimple) {
			if (ptr_type->simple_type == t->ptr_type->simple_type)
				return true;
			else if (CGOptions::strict_float() &&
				((ptr_type->simple_type == eFloat && t->ptr_type->simple_type != eFloat) ||
				(ptr_type->simple_type != eFloat && t->ptr_type->simple_type == eFloat)))
				return false;
			else if (CGOptions::lang_cpp())
				return false;	// or we need an explicit cast here
			else
				return ptr_type->SizeInBytes() == t->ptr_type->SizeInBytes();
		}
		/*else if (ptr_type->eType == eStruct&& t->ptr_type->eType == eStruct)
		{
			return ptr_type->is_struct_convertable(t->ptr_type);
		}*/

	}
	/*else if (eType == eStruct && t->eType == eStruct) {
		return is_struct_convertable(t);
	}*/
	return false;
}
bool
Type::is_struct_convertable(const Type* t) const
{
	if (this == t)
		return true;
	for (size_t i = 0; i < unambiguous_derive_classes.size(); i++)
	{
		if (unambiguous_derive_classes[i] == t)
			return true;
	}
	return false;
}

// eLong & eInt, eULong & eUInt are equivalent
bool
Type::is_equivalent(const Type* t) const
{
	if (this == t)
		return true;

	if (eType == eSimple) {
		return (is_signed() == t->is_signed()) && (SizeInBytes() == t->SizeInBytes());
	}

	return false;
}

bool
Type::needs_cast(const Type* t) const
{
	return (eType == ePointer) && !get_base_type()->is_equivalent(t->get_base_type());
}

bool
Type::match(const Type* t, enum eMatchType mt) const
{
	switch (mt) {
	case eExact: return (this == t);
	case eConvert: return is_convertable(t);
	case eDereference: return is_dereferenced_from(t);
	case eDerefExact: return (t == this || is_dereferenced_from(t));
	case eFlexible: return is_derivable(t);
	default: break;
	}
	return false;
}

// ---------------------------------------------------------------------
/* return true if this type can be derived from the given type
 * by dereferencing
 *************************************************************/
bool
Type::is_dereferenced_from(const Type* t) const
{
	if (t->eType == ePointer) {
		const Type* pt = t->ptr_type;
		while (pt) {
			if (pt == this) {
				return true;
			}
			pt = pt->ptr_type;
		}
	}
	return false;
}

// ---------------------------------------------------------------------
/* return true if this type can be derived from the given type
 * by taking address(one level) or dereferencing (multi-level)
 * question: allow interger conversion? i.e. char* p = &(int)i?
 *************************************************************/
bool
Type::is_derivable(const Type* t) const
{
	if (this == t) {
		return true;
	}
	return is_convertable(t) || is_dereferenced_from(t) || (ptr_type == t);
}

unsigned long
Type::SizeInBytes(void) const
{
	size_t i;
	switch (eType) {
	default: break;
	case eSimple:
		switch (simple_type) {
		case eVoid:		return 0;
		case eInt:		return 4;
		case eShort:	return 2;
		case eChar:		return 1;
		case eLong:		return 4;
		case eLongLong:	return 8;
		case eUChar:	return 1;
		case eUInt:		return 4;
		case eUShort:	return 2;
		case eULong:	return 4;
		case eULongLong:return 8;
		case eFloat:	return 4;
			//		case eDouble:	return 8;
		}
		break;
	case eUnion: {
		unsigned int max_size = 0;
		for (i = 0; i < fields.size(); i++) {
			unsigned int sz = 0;
			if (is_bitfield(i)) {
				assert(i < bitfields_length_.size());
				sz = (int)(ceil(bitfields_length_[i] / 8.0) * 8);
			}
			else {
				sz = fields[i]->SizeInBytes();
			}
			if (sz == SIZE_UNKNOWN) return sz;
			if (sz > max_size) {
				max_size = sz;
			}
		}
		return max_size;
	}
	case eStruct: {
		if (!this->packed_) return SIZE_UNKNOWN;
		// give up if there are bitfields, too much compiler-dependence and machine-dependence
		if (this->has_bitfields()) return SIZE_UNKNOWN;
		unsigned int total_size = 0;
		for (i = 0; i < fields.size(); i++) {
			unsigned int sz = fields[i]->SizeInBytes();
			if (sz == SIZE_UNKNOWN) return sz;
			total_size += sz;
		}
		return total_size;
	}
	case ePointer:
		CGOptions::pointer_size();
		break;
	}
	return 0;
}

// --------------------------------------------------------------
 /* Select a left hand type for assignments
  ************************************************************/
const Type *
Type::SelectLType(bool no_volatile, eAssignOps op)
{
	const Type* type = NULL;
	// occasionally we want to play with pointers
	// We haven't implemented pointer arith,
	// so choose pointer types iff we create simple assignment
	// (see Statement::make_random)
	if (op == eSimpleAssign && rnd_flipcoin(PointerAsLTypeProb)) {
		ERROR_GUARD(NULL);
		type = Type::make_random_pointer_type();
	}
	ERROR_GUARD(NULL);

	// choose a struct type as LHS type
	if (!type && (op == eSimpleAssign)) {
		vector<Type *> ok_struct_types;
		get_all_ok_struct_union_types(ok_struct_types, true, no_volatile, false, true);
		if ((ok_struct_types.size() > 0) && rnd_flipcoin(StructAsLTypeProb)) {
			type = Type::choose_random_struct_union_type(ok_struct_types);
		}
	}

	// choose float as LHS type
	if (!type) {
		if (StatementAssign::AssignOpWorksForFloat(op) && rnd_flipcoin(FloatAsLTypeProb)) {
			type = &Type::get_simple_type(eFloat);
		}
	}

	// default is any integer type
	if (!type) {
		type = get_int_type();
	}
	return type;
}

void
Type::get_int_subfield_names(string prefix, vector<string>& names, const vector<int>& excluded_fields) const
{
	if (eType == eSimple) {
		names.push_back(prefix);
	}
	else if (is_aggregate()) {
		size_t i;
		size_t j = 0;
		for (i = 0; i < sub_object_tree.unambiguousMembers.size(); i++) {
			if (sub_object_tree.unambiguousMembersAccess[i] == eNoAccess) {
				j++;
				continue;
			}
			// skip excluded fields
			if (std::find(excluded_fields.begin(), excluded_fields.end(), j) != excluded_fields.end()) {
				j++;
				continue;
			}
			const Type *memberInType = sub_object_tree.unambiguousMembers[j].first->type;
			int memberIndex = sub_object_tree.unambiguousMembers[j].second;
			j++;

			if (memberInType->is_unamed_padding(memberIndex)) continue; // skip 0 length bitfields

			ostringstream oss;

			oss << prefix << "." << memberInType->fields_name[memberIndex];
			vector<int> empty;
			memberInType->fields[memberIndex]->get_int_subfield_names(oss.str(), names, empty);
		}
	}
}

bool
Type::contain_pointer_field(void) const
{
	if (eType == ePointer) return true;
	if (eType == eStruct || eType == eUnion) {
		for (size_t i = 0; i < fields.size(); i++) {
			if (fields[i]->contain_pointer_field()) {
				return true;
			}
		}
	}
	return false;
}

// ---------------------------------------------------------------------
void
Type::Output(std::ostream &out, bool use_elaborated_type_specifier) const
{
	switch (eType) {
	case eSimple:
		if (this->simple_type == eVoid) {
			out << "void";
		}
		else if (this->simple_type == eFloat) {
			out << "float";
		}
		else {
			out << (is_signed() ? "int" : "uint");
			out << (SizeInBytes() * 8);
			out << "_t";
		}
		break;
	case ePointer:   ptr_type->Output(out); out << "*"; break;
	case eUnion:
		if (use_elaborated_type_specifier) out << "union U" << sid;
		else out << "U" << sid;
		break;
	case eStruct:
		if (use_elaborated_type_specifier) out << "class S" << sid;
		else out << "::S" << sid;
		break;
	}
}

void
Type::get_type_sizeof_string(std::string &s) const
{
	ostringstream ss;
	ss << "sizeof(";
	Output(ss);
	ss << ")";
	s = ss.str();
}

// If this is C++, create assign operators for volatile structs:
// Semantically this should work, though for bit fields it's probably not what was intended.
//volatile struct S0& operator=(const volatile struct S0& val) volatile {
//	if (this == &val) {
//		return *this;
//	}
//	f0 = val.f0;
//	f1 = val.f1;
//	return *this;
//}
// As a result of generating this, have to generate 'default' assignment operator as well
void OutputStructAssignOp(Type* type, std::ostream &out, bool vol)
{
	if (CGOptions::lang_cpp()) {
		if (type->has_assign_ops() && (type->eType == eStruct)) {
			out << "   ";
			if (vol) {
				out << "volatile ";
			}
			type->Output(out); out << "& operator=(const ";
			if (vol) {
				out << "volatile ";
			}
			type->Output(out); out << "& val) ";
			if (vol) {
				out << "volatile ";
			}
			out << "{"; really_outputln(out);
			out << "       if (this == &val) {"; really_outputln(out);
			out << "           return *this;"; really_outputln(out);
			out << "       }"; really_outputln(out);

			for (size_t i = 0; i < type->base_classes.size(); i++)
			{
				if (type->base_classes_access[i] == ePublic)
				{
					out << "       ";
					type->base_classes[i]->Output(out);
					out << "::operator=(val);";
					really_outputln(out);
				}
			}

			// for all fields:
			for (size_t i = 0; i < type->sub_object_tree.unambiguousMembers.size(); i++) {
				if (type->sub_object_tree.unambiguousMembersAccess[i] != eNoAccess)
				{
					const Type *memberInType = type->sub_object_tree.unambiguousMembers[i].first->type;
					int memberIndex = type->sub_object_tree.unambiguousMembers[i].second;
					out << "       ";
					out << " " << memberInType->fields_name[memberIndex] << "= val." << memberInType->fields_name[memberIndex] << ";";
					really_outputln(out);
				}
			}


			out << "       return *this;"; really_outputln(out);
			out << "   }"; really_outputln(out);
		}
	}
}

// To have volatile unions in C++ they need assignment operators:
//union U1& operator=(const union U1& val){
//	if (this == &val) {
//		return *this;
//	}
//	memcpy(this, &val, sizeof(union U1));
//	return *this;
//}
//volatile union U1& operator=(const volatile union U1& val) volatile {
//	if (this == &val) {
//		return *this;
//	}
//	memcpy((union U1*)this, (const union U1*)(&val), sizeof(union U1));
//	return *this;
//}

void OutputUnionAssignOps(Type* type, std::ostream &out, bool vol)
{
	if (CGOptions::lang_cpp()) {
		if (type->has_assign_ops() && (type->eType == eUnion)) {

			out << "    ";
			if (vol) {
				out << "volatile ";
			}
			type->Output(out); out << "& operator=(const ";
			if (vol) {
				out << "volatile ";
			}
			type->Output(out); out << "& val) ";
			if (vol) {
				out << "volatile ";
			}
			out << "{"; really_outputln(out);
			out << "        if (this == &val) {"; really_outputln(out);
			out << "            return *this;"; really_outputln(out);
			out << "        }"; really_outputln(out);


			out << "        memcpy((";
			type->Output(out);
			out << "*)this, (const ";
			type->Output(out);
			out << "*)(&val), sizeof(";
			type->Output(out);
			out << ")); "; really_outputln(out);

			out << "        return *this;"; really_outputln(out);
			out << "    }"; really_outputln(out);
		}
	}
}

void OutputDefaultCtor(Type* type, std::ostream &out)
{
	out << "   ";
	out << "S" << type->sid;
	out << "() {}";
	really_outputln(out);
}


void OutputMainFriend(std::ostream &out)
{
	out << "   ";
	out << "friend int main (int argc, char* argv[]);";
	really_outputln(out);
}


void OutputFriends(Type* type, std::ostream &out)
{
	for (size_t i = 0; i < type->friend_classes.size(); i++)
	{
		really_outputln(out);
		out << "   ";
		out << "friend ";
		type->friend_classes[i]->Output(out);
		out << ";";
	}
	really_outputln(out);
}


// ---------------------------------------------------------------------
/* print struct definition (fields etc)
 *************************************************************/
void OutputStructUnion(Type* type, std::ostream &out)
{
	size_t i;
	// sanity check
	assert(type->is_aggregate());

	if (!type->printed) {
		// output dependent structs, if any
		for (i = 0; i < type->fields.size(); i++) {
			if (type->fields[i]->is_aggregate()) {
				OutputStructUnion((Type*)type->fields[i], out);
			}
		}
		// output myself
		if (type->packed_) {
			if (!CGOptions::ccomp()) {
				out << "#pragma pack(push)";
				really_outputln(out);
			}
			out << "#pragma pack(1)";
			really_outputln(out);
		}
		type->Output(out, true);
		if (type->base_classes.size() != 0)
		{
			out << ": ";
			for (size_t i = 0; i < type->base_classes.size(); i++)
			{
				out << Type::get_access_name(type->base_classes_access[i]) << " ";
				if (type->isVirtualBaseClasses[i] == true)
				{
					out << "virtual ";
				}
				out << "S" << type->base_classes[i]->sid;
				if (i != type->base_classes.size() - 1)
				{
					out << ", ";
				}

			}
		}

		out << " {";

		if (type->eType == eStruct)
		{
			OutputFriends(type, out);
		}

		really_outputln(out);

		OutputStructInAccess(type, out, ePrivate);
		OutputStructInAccess(type, out, eProtected);

		OutputAccessSpecifier(out, ePublic);
		OutputStructFieldsInAccess(type, out, ePublic);
		OutputStructFuncInAccess(type, out, ePublic);

		if (type->eType == eStruct) {
			OutputDefaultCtor(type, out);
			//OutputStructAssignOp(type, out, false);
			//OutputStructAssignOp(type, out, true);
			OutputMainFriend(out);
		}
		else {
			OutputUnionAssignOps(type, out, false);
			OutputUnionAssignOps(type, out, true);
		}

		out << "};";
		really_outputln(out);
		if (type->packed_) {
			if (CGOptions::ccomp()) {
				out << "#pragma pack()";
			}
			else {
				out << "#pragma pack(pop)";
			}
			really_outputln(out);
		}
		type->printed = true;
		really_outputln(out);
	}
}


bool TypeHasAccess(Type* type, eAccess access)
{
	for (size_t i = 0; i < type->fields.size(); i++) {
		if (type->fields_access[i] == access)
			return true;
	}

	for (size_t i = 0; i < type->func_list.size(); i++) {
		if (type->func_list_access[i] == access)
			return true;
	}
	return false;
}

void Type::getAllBaseClassesFunctions(set<Function*> &functionSet, bool includeThisClass)
{
	if (includeThisClass)
		for (Function* memberFunc : this->func_list)
			functionSet.insert(memberFunc);

	for (Type* baseClass : this->base_classes)
		baseClass->getAllBaseClassesFunctions(functionSet, true);
}

void Type::getAllChildClassesFunctions(set<Function*> &functionSet, bool includeThisClass)
{
	if (includeThisClass)
		for (Function* memberFunc : this->func_list)
			functionSet.insert(memberFunc);

	for (Type* childClass : this->child_classes)
		childClass->getAllChildClassesFunctions(functionSet, true);
}


void OutputStructInAccess(Type* type, std::ostream &out, eAccess access)
{
	if (TypeHasAccess(type, access))
	{
		OutputAccessSpecifier(out, access);
		OutputStructFieldsInAccess(type, out, access);
		OutputStructFuncInAccess(type, out, access);
	}
}

void OutputAccessSpecifier(std::ostream &out, eAccess access)
{
	out << Type::get_access_name(access) << ":";
}

void OutputStructFieldsInAccess(Type* type, std::ostream &out, eAccess access)
{
	really_outputln(out);

	assert(type->fields.size() == type->qfers_.size());
	unsigned int j = 0;
	for (size_t i = 0; i < type->fields.size(); i++) {
		if (type->fields_access[i] != access)
			continue;
		out << "   ";
		const Type *field = type->fields[i];
		bool is_bitfield = type->is_bitfield(i);
		if (is_bitfield) {
			assert(field->eType == eSimple);
			type->qfers_[i].OutputFirstQuals(out);
			if (field->simple_type == eInt)
				out << "signed";
			else if (field->simple_type == eUInt)
				out << "unsigned";
			else
				assert(0);
			int length = type->bitfields_length_[i];
			assert(length >= 0);
			if (length == 0)
			{
				out << " : ";
				out << length << ";";
			}
			else
			{
				out << " " << type->fields_name[j++] << " : ";
				out << length;
				if (type->fields_init.size() > i && type->fields_init[i] != nullptr)
				{
					out << " = ";
					type->fields_init[i]->Output(out);
				}
				out << ";";
			}
		}
		else {
			type->qfers_[i].output_qualified_type(field, out);
			out << " " << type->fields_name[i];
			if (type->fields_init.size() > i && type->fields_init[i] != nullptr)
			{
				out << " = ";
				type->fields_init[i]->Output(out);
			}
			out << ";";
		}
		really_outputln(out);
	}
}

void OutputStructFuncInAccess(Type* type, std::ostream &out, eAccess access)
{
	really_outputln(out);
	for (size_t i = 0; i < type->func_list.size(); i++) {
		if (type->func_list_access[i] != access)
			continue;
		out << "   ";
		Function* func = type->func_list[i];
		func->OutputHeader(out, true);
		out << ";";
		really_outputln(out);
	}
}

// ---------------------------------------------------------------------
/* print all struct definitions (fields etc)
 *************************************************************/
void
OutputStructUnionDeclarations(std::ostream &out)
{
	size_t i;
	really_outputln(out);
	output_comment_line(out, "--- Class Declarations ---");
	for (i = 0; i < AllTypes.size(); i++)
	{

		Type* t = AllTypes[i];
		if (t->used && (t->eType == eStruct || t->eType == eUnion)) {
			really_outputln(out);
			out << "class S" << t->sid << ";";
		}
	}

	really_outputln(out);
	output_comment_line(out, "--- Class Definitions ---");
	for (i = 0; i < AllTypes.size(); i++)
	{
		Type* t = AllTypes[i];
		if (t->used && (t->eType == eStruct || t->eType == eUnion)) {
			OutputStructUnion(AllTypes[i], out);
		}
	}
}

/*
 * return the printf directive string for the type. for example, int -> "%d"
 */
std::string
Type::printf_directive(void) const
{
	string ret;
	size_t i;
	switch (eType) {
	case eSimple:
		if (SizeInBytes() >= 8) {
			ret = is_signed() ? "%lld" : "%llu";
		}
		else {
			ret = is_signed() ? "%d" : "%u";
		}
		break;
	case ePointer:
		ret = "0x%0x";
		break;
	case eUnion:
	case eStruct:
		ret = "{";
		for (i = 0; i < fields.size(); i++) {
			if (i > 0) ret += ", ";
			ret += fields[i]->printf_directive();
		}
		ret += "}";
		break;
	}
	return ret;
}

bool Type::runFinalOverridersCheckForAllTypes()
{
	for (Type * type : AllTypes)
		if (type->eType == eTypeDesc::eStruct)
			if (!type->sub_object_tree.runFinalOverridersCheck())
				return false;

	return true;
}

/*
 *
 */
void
Type::doFinalization(void)
{
	vector<Type *>::iterator j;
	for (j = AllTypes.begin(); j != AllTypes.end(); ++j)
		delete (*j);
	AllTypes.clear();

	for (j = derived_types.begin(); j != derived_types.end(); ++j)
		delete (*j);
	derived_types.clear();
}

SubObjectTree::SubObjectNode::SubObjectNode(const Type *_type) : subObjectId(++subObjectlastId), type(_type), type_access(ePrivate)
{
	// nothing else to do
}


bool SubObjectTree::SubObjectNode::isBaseClassSubObjectOf(const SubObjectNode &potentialSuperObject)
{
	for (auto directSubObject : potentialSuperObject.directBaseSubObjects)
	{
		if (*directSubObject == *this)
			return true;
	}

	for (auto directSubObject : potentialSuperObject.directBaseSubObjects)
	{
		if (this->isBaseClassSubObjectOf(*directSubObject))
			return true;
	}

	return false;
}

bool SubObjectTree::SubObjectNode::operator==(const SubObjectNode &other)
{
	return this->subObjectId == other.subObjectId;
}

bool SubObjectTree::SubObjectNode::isValid(string memberName)
{
	return memberNameToSMapping[memberName].isValid;
}

void SubObjectTree::buildTreeRecursion(shared_ptr<SubObjectNode> node, eAccess access)
{

	for (size_t i = 0; i < node->type->base_classes.size(); i++)
	{
		Type* base_class = node->type->base_classes[i];
		eAccess base_access = node->type->base_classes_access[i];
		const bool isVirtualBaseClass = node->type->isVirtualBaseClasses[i];
		bool isNew = true;
		shared_ptr<SubObjectNode> baseClassNode;

		if (isVirtualBaseClass)
		{
			if (typeToVirtualSubObjectMapping.find(base_class) != typeToVirtualSubObjectMapping.end())
			{
				baseClassNode = typeToVirtualSubObjectMapping[base_class];
				isNew = false;
			}
			else
			{
				baseClassNode = make_shared<SubObjectNode>(base_class);
				typeToVirtualSubObjectMapping[base_class] = baseClassNode;
			}
		}
		else
		{
			baseClassNode = make_shared<SubObjectNode>(base_class);
		}

		if (typeToUnambiguousData.find(base_class) == typeToUnambiguousData.end())
		{
			typeToUnambiguousData[base_class] = UnambiguousData(isVirtualBaseClass, min(base_access, access));
		}
		else
		{
			// type is Unambiguous
			if (typeToUnambiguousData[base_class].isValid)
			{
				// if the one of base classes is not virtual
				if (!typeToUnambiguousData[base_class].isVirtual || !isVirtualBaseClass)
					typeToUnambiguousData[base_class].isValid = false;
			}
			typeToUnambiguousData[base_class].access = max(typeToUnambiguousData[base_class].access, min(base_access, access));
		}
		node->directBaseSubObjects.push_back(baseClassNode);
		if (isNew)
			buildTreeRecursion(baseClassNode, min(base_access, access));

		switch (base_access)
		{
		case ePrivate:
			node->private_set.insert(baseClassNode->protected_set.begin(), baseClassNode->protected_set.end());
			node->private_set.insert(baseClassNode->public_set.begin(), baseClassNode->public_set.end());
			break;
		case eProtected:
			node->protected_set.insert(baseClassNode->protected_set.begin(), baseClassNode->protected_set.end());
			node->protected_set.insert(baseClassNode->public_set.begin(), baseClassNode->public_set.end());
			break;
		case ePublic:
			node->protected_set.insert(baseClassNode->protected_set.begin(), baseClassNode->protected_set.end());
			node->public_set.insert(baseClassNode->public_set.begin(), baseClassNode->public_set.end());
			break;
		}

	}

	for (size_t i = 0; i < node->type->fields.size(); i++)
	{
		switch (node->type->fields_access[i])
		{
		case ePrivate:
			node->private_set.insert({ node, i });
			break;
		case eProtected:
			node->protected_set.insert({ node, i });
			break;
		case ePublic:
			node->public_set.insert({ node, i });
			break;
		}
	}
}

SubObjectTree::SubObjectTree() {}

void SubObjectTree::buildTree(Type* _root)
{
	root = make_shared<SubObjectNode>(_root);
	root->type_access = ePublic;

	buildTreeRecursion(root);

	updateUnambiguousDeriveClasses(_root);

	calculateMembers(root);

	for (auto &memberNameSPair : root->memberNameToSMapping)
	{
		const string &memberName = memberNameSPair.first;
		const auto S = memberNameSPair.second;

		for (auto &member : S.declaration_set)
			if (S.isValid)
			{
				this->unambiguousMembers.push_back(member);
				this->unambiguousMembersAccess.push_back(getUnambiguousMembersAccess(member));
			}
	}
}

eAccess SubObjectTree::getUnambiguousMembersAccess(std::pair<shared_ptr<SubObjectNode>, int> member)
{
	if (root->public_set.find(member) != root->public_set.end())
		return ePublic;
	if (root->protected_set.find(member) != root->protected_set.end())
		return eProtected;
	if (root->private_set.find(member) != root->private_set.end())
		return ePrivate;
	return eNoAccess;
}




void SubObjectTree::updateUnambiguousDeriveClasses(Type* _root)
{
	for (auto data : typeToUnambiguousData)
	{
		//is type Unambiguous
		if (data.second.isValid && data.second.access == ePublic)
		{
			data.first->unambiguous_derive_classes.push_back(_root);
			_root->unambiguous_base_classes.push_back(data.first);
		}
	}
}

bool SubObjectTree::areDeclarationSetsEqual(const SubObjectNode::S &s1, const SubObjectNode::S &s2)
{
	if (!s1.isValid || !s2.isValid)	// an invalid declaration set is considered different from any other
		return false;

	return s1.declaration_set == s2.declaration_set;
}

/*
*  Check whether each member in s1 is a base class subobject of at least one of the members is s2
*  Note: argument order matters
*/
bool SubObjectTree::subObjectSetComparison(const unordered_set<shared_ptr<SubObjectNode>> &set1, const unordered_set<shared_ptr<SubObjectNode>> &set2)
{
	for (auto firstSubObject : set1)
	{
		bool isBaseClassSubObject = false;
		for (auto secondSubObject : set2)
		{
			if (firstSubObject->isBaseClassSubObjectOf(*secondSubObject))
			{
				isBaseClassSubObject = true;
				break;
			}
		}

		if (!isBaseClassSubObject)
			return false;
	}

	return true;
}

void SubObjectTree::calculateMembers(shared_ptr<SubObjectNode> node)
{
	auto& myMemberNameToSMapping = node->memberNameToSMapping;

	// add all this sets members to corresponding S set
	for (size_t i = 0; i < node->type->fields_name.size(); i++)
	{
		myMemberNameToSMapping[node->type->fields_name[i]].declaration_set.insert({ node, i });
		myMemberNameToSMapping[node->type->fields_name[i]].subobject_set.insert(node);
	}
	// end of adding members

	for (shared_ptr<SubObjectNode> subObject : node->directBaseSubObjects)
	{
		calculateMembers(subObject);	// recursevly calculate direct base sub objects member name to S mapping

		auto& otherMemberNameToSMapping = subObject->memberNameToSMapping;

		for (auto& it : otherMemberNameToSMapping)	// go over others S sets and merge them to mine
		{
			const string& memberName = it.first;
			const auto& otherMemberS = it.second;

			{// check whether this set already has this member name, if so, continue to next name
				bool condition = false;
				for (size_t i = 0; i < node->type->fields_name.size(); i++) {
					if (memberName == node->type->fields_name[i]) {
						condition = true;
						break;
					}
				}
				if (condition)	// leave S uncahnged and continue
					continue;
			}// end of last check


			{// check if I dont have a S set for this member name, if so copy others set and continue to next name
				if (myMemberNameToSMapping.find(memberName) == myMemberNameToSMapping.end())
				{
					myMemberNameToSMapping[memberName] = otherMemberNameToSMapping[memberName];
					continue;
				}
			}// end of last check

			auto& myMemberS = myMemberNameToSMapping[memberName];

			{// check whether each member in other subobject set is a base class subobject of at least one of the members is my sub object set
				bool condition = subObjectSetComparison(otherMemberS.subobject_set, myMemberS.subobject_set);
				if (condition)	// leave S unchanged and continue
					continue;
			}// end of last check


			{// check whether each member in my subobject set is a base class subobject of at least one of the members is other subobject set
				bool condition = subObjectSetComparison(myMemberS.subobject_set, otherMemberS.subobject_set);
				if (condition)	// copy others set and continue
				{
					myMemberNameToSMapping[memberName] = otherMemberNameToSMapping[memberName];
					continue;
				}
			}// end of last check


			{// check whether declaration sets differ, if so merge is ambiguous
				bool condition = areDeclarationSetsEqual(myMemberS, otherMemberS);
				if (!condition)
				{
					// declaration set is now invalid, and subobject set is now union of the two subobject sets
					myMemberS.isValid = false;
					myMemberS.subobject_set.insert(otherMemberS.subobject_set.begin(), otherMemberS.subobject_set.end());
					continue;
				}
			}// end of last check


			{// otherwise, the new set is a lookup set with the shared set of declarations and the union of the subobject sets
				myMemberS.subobject_set.insert(otherMemberS.subobject_set.begin(), otherMemberS.subobject_set.end());
				continue;
			}// end of last check

		}
	}
}

SubObjectTree::UnambiguousData::UnambiguousData(bool _isVirtual, eAccess _access) : isValid(true), isVirtual(_isVirtual), access(_access) {

}

bool SubObjectTree::runFinalOverridersCheck()
{
	// clear data from previuos calculatations
	functionToFinalOverridersMapping.clear();

	culculateFinalOverridersRecursion(this->root);

	for (auto &funcAndOverridesPair : functionToFinalOverridersMapping)
		if (funcAndOverridesPair.second.size() >= 2)
			return false;

	return true;
}

void SubObjectTree::culculateFinalOverridersRecursion(shared_ptr<SubObjectNode> node)
{
	for (shared_ptr<SubObjectNode> subObj : node->directBaseSubObjects)
		culculateFinalOverridersRecursion(subObj);

	unordered_set<SubObjectMemberFunction, SubObjectMemberFunction::hash> subObjectsFunctions;

	for (shared_ptr<SubObjectNode> subObj : node->directBaseSubObjects)
	{
		for (const SubObjectMemberFunction &subObjFunc : subObj->allSubTreeFunctions)
		{
			subObjectsFunctions.insert(subObjFunc);
		}
	}

	for (const SubObjectMemberFunction &subObjFunc : subObjectsFunctions)
	{
		if (subObjFunc.func->is_marked_as_virtual)
		{
			for (Function* myFunc : node->type->func_list)
			{
				if (Function::compare_signature(*subObjFunc.func, *myFunc))	// if ovveride occurs
				{
					myFunc->is_virtual_candidate = true;

					for (auto itr = functionToFinalOverridersMapping[subObjFunc].begin(); itr != functionToFinalOverridersMapping[subObjFunc].end();)
					{
						if (subObjectsFunctions.find(*itr) != subObjectsFunctions.end())
							itr = functionToFinalOverridersMapping[subObjFunc].erase(itr);
						else
							itr++;
					}

					// insert self
					functionToFinalOverridersMapping[subObjFunc].insert(SubObjectMemberFunction(node, myFunc));
				}
			}
		}
	}

	node->allSubTreeFunctions.insert(subObjectsFunctions.begin(), subObjectsFunctions.end());
	for (Function *myFunc : node->type->func_list)
	{
		node->allSubTreeFunctions.insert({ node, myFunc });
		if (myFunc->is_marked_as_virtual)
			myFunc->is_virtual_candidate = true;
	}
}

///////////////////////////////////////////////////////////////////////////////

// Local Variables:
// c-basic-offset: 4
// tab-width: 4
// End:

// End of file.